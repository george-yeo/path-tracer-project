#include "renderer/Renderer.hpp"

#include "core/Barriers.hpp"
#include "core/VkCheck.hpp"
#include "core/VulkanContext.hpp"
#include "renderer/SceneCompiler.hpp"
#include "resources/Buffer.hpp"
#include "scene/Camera.hpp"
#include "scene/Scene.hpp"
#include "window/Window.hpp"

#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <stdexcept>
#include <vector>

namespace pt {

namespace {

constexpr double kSmoothing = 0.1; // exponential moving average weight for displayed stats

uint8_t linearToSrgb8(uint8_t value) {
    float c = static_cast<float>(value) / 255.0f;
    c = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<uint8_t>(std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f));
}

} // namespace

Renderer::Renderer(VulkanContext& context, Window& window, bool vsync)
    : context(context), device(context.getDevice()), window(window), vsync(vsync) {
    swapchain = std::make_unique<Swapchain>(device, context.getSurface(),
                                            window.getFramebufferExtent(), vsync);
    for (auto& frame : frames) {
        frame = std::make_unique<Frame>(device);
    }
    pathTracer = std::make_unique<PathTracer>(device, swapchain->getExtent());
    ui = std::make_unique<ImGuiLayer>(context, window, swapchain->getFormat(),
                                      swapchain->getMinImageCount(), swapchain->getImageCount());
}

Renderer::~Renderer() {
    device.waitIdle();
    ui.reset();
    pathTracer.reset();
    gpuScene.reset();
    for (auto& frame : frames) {
        frame.reset();
    }
    swapchain.reset();
}

void Renderer::loadScene(const Scene& scene) {
    CompiledScene compiled = compileScene(scene);

    device.waitIdle(); // the old scene's buffers may still be in use
    pathTracer->resetAccumulation();
    gpuScene = std::make_unique<GpuScene>(device, compiled);
    pathTracer->setScene(*gpuScene);

    sceneStats.triangleCount = static_cast<uint32_t>(compiled.triangles.size());
    sceneStats.sphereCount = static_cast<uint32_t>(compiled.spheres.size());
    sceneStats.lightCount = static_cast<uint32_t>(compiled.lights.size());
    sceneStats.bvh = compiled.bvhStats;
    settings.sky = scene.sky;
}

void Renderer::setVsync(bool enabled) {
    if (enabled != vsync) {
        vsync = enabled;
        swapchainDirty = true;
    }
}

void Renderer::recreateSwapchain() {
    device.waitIdle();
    swapchain->recreate(window.getFramebufferExtent(), vsync);
    pathTracer->resize(swapchain->getExtent());
    ui->setMinImageCount(swapchain->getMinImageCount());
    swapchainDirty = false;
}

void Renderer::drawFrame(const Camera& camera) {
    VkExtent2D framebuffer = window.getFramebufferExtent();
    if (framebuffer.width == 0 || framebuffer.height == 0) {
        ui->endFrameWithoutRendering(); // minimised: nothing to draw into
        return;
    }
    if (window.consumeResized() || swapchainDirty) {
        recreateSwapchain();
    }

    const uint32_t slot = frameIndex;
    Frame& frame = *frames[slot];
    frame.waitUntilAvailable();
    readTimestamps(frame, slot);

    uint32_t imageIndex = 0;
    VkResult acquired = swapchain->acquire(frame.getImageAvailable(), imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        ui->endFrameWithoutRendering();
        return;
    }

    frame.reset();
    recordFrame(frame, imageIndex, camera);

    // The swapchain image is first touched by the blit, so only the transfer stage has to wait
    // for the acquire. The path tracing dispatches before it overlap with presentation.
    // (Previously the wait was at COLOR_ATTACHMENT_OUTPUT while the first write was a transfer:
    // SYNC-HAZARD-WRITE-AFTER-READ against the presentation engine.)
    VkSemaphoreSubmitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitInfo.semaphore = frame.getImageAvailable();
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;

    VkSemaphoreSubmitInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalInfo.semaphore = swapchain->getRenderFinished(imageIndex);
    signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = frame.getCommandBuffer();

    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &waitInfo;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signalInfo;
    VK_CHECK(vkQueueSubmit2(device.getQueue(), 1, &submit, frame.getInFlightFence()));

    VkResult presented = swapchain->present(device.getQueue(), imageIndex);
    if (acquired == VK_SUBOPTIMAL_KHR || presented != VK_SUCCESS) {
        swapchainDirty = true;
    }

    frameIndex = (frameIndex + 1) % kFramesInFlight;
}

void Renderer::recordFrame(Frame& frame, uint32_t imageIndex, const Camera& camera) {
    VkCommandBuffer cmd = frame.getCommandBuffer();
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    const bool timestamps = device.getTimestampPeriodNs() > 0.0;
    if (timestamps) {
        vkCmdResetQueryPool(cmd, frame.getQueryPool(), 0, 2);
        vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frame.getQueryPool(), 0);
    }

    // 1. Path trace + resolve into the display image (left in TRANSFER_SRC_OPTIMAL).
    samplesInFlight[frameIndex] = pathTracer->record(cmd, camera, settings, !swapchain->isSrgb());

    if (timestamps) {
        vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frame.getQueryPool(), 1);
        frame.hasTimestamps = true;
    }

    // 2. Blit to the swapchain image. A blit (unlike the old vkCmdCopyImage) converts between
    //    RGBA and BGRA, which fixes the red/blue swap on BGRA swapchains.
    VkImage target = swapchain->getImage(imageIndex);
    const VkExtent2D extent = swapchain->getExtent();
    const VkExtent2D source = pathTracer->getDisplayImage().getExtent();
    imageBarrier(
        cmd, target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_NONE}, // chains with the acquire wait
        {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    VkImageBlit2 region{};
    region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[1] = {static_cast<int32_t>(source.width), static_cast<int32_t>(source.height),
                            1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstOffsets[1] = {static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height),
                            1};

    VkBlitImageInfo2 blit{};
    blit.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blit.srcImage = pathTracer->getDisplayImage().get();
    blit.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blit.dstImage = target;
    blit.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blit.regionCount = 1;
    blit.pRegions = &region;
    blit.filter = VK_FILTER_NEAREST;
    vkCmdBlitImage2(cmd, &blit);

    // 3. UI overlay, drawn straight onto the swapchain image with dynamic rendering.
    imageBarrier(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                 {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT},
                 {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
    ui->record(cmd, swapchain->getView(imageIndex), extent);

    // 4. Hand over to the presentation engine. The destination stage must be one the
    //    render-finished semaphore signal (ALL_COMMANDS) waits on, so the layout transition is
    //    ordered before the present; an empty destination scope leaves it unordered.
    imageBarrier(
        cmd, target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
        {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_NONE});

    VK_CHECK(vkEndCommandBuffer(cmd));
}

void Renderer::readTimestamps(Frame& frame, uint32_t slot) {
    if (!frame.hasTimestamps) {
        return;
    }
    uint64_t ticks[2] = {};
    VkResult result = vkGetQueryPoolResults(device.get(), frame.getQueryPool(), 0, 2, sizeof(ticks),
                                            ticks, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    frame.hasTimestamps = false;
    if (result != VK_SUCCESS || ticks[1] < ticks[0]) {
        return;
    }

    const double ms =
        static_cast<double>(ticks[1] - ticks[0]) * device.getTimestampPeriodNs() * 1e-6;
    frameStats.gpuMs += kSmoothing * (ms - frameStats.gpuMs);

    const uint32_t samples = samplesInFlight[slot];
    if (samples > 0 && ms > 0.0) {
        const VkExtent2D extent = pathTracer->getDisplayImage().getExtent();
        const double pixelSamples = static_cast<double>(samples) * extent.width * extent.height;
        const double rate = pixelSamples / (ms * 1e-3);
        frameStats.samplesPerSecond += kSmoothing * (rate - frameStats.samplesPerSecond);
    }
}

void Renderer::saveScreenshot(const std::filesystem::path& path) {
    device.waitIdle();
    if (pathTracer->getDisplayLayout() != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        throw std::runtime_error("Nothing has been rendered yet");
    }

    const Image& image = pathTracer->getDisplayImage();
    const VkExtent2D extent = image.getExtent();
    const VkDeviceSize size = VkDeviceSize{extent.width} * extent.height * 4;
    Buffer readback(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryUsage::Readback);

    device.immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {extent.width, extent.height, 1};
        vkCmdCopyImageToBuffer(cmd, image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback.get(), 1, &region);
        memoryBarrier(cmd, {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT},
                      {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT});
    });
    readback.invalidate();

    std::vector<uint8_t> pixels(static_cast<size_t>(size));
    std::memcpy(pixels.data(), readback.getMapped(), pixels.size());
    for (size_t i = 0; i < pixels.size(); i += 4) {
        if (!pathTracer->isDisplaySrgbEncoded()) {
            for (size_t c = 0; c < 3; ++c) {
                pixels[i + c] = linearToSrgb8(pixels[i + c]);
            }
        }
        pixels[i + 3] = 255;
    }

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    const int stride = static_cast<int>(extent.width) * 4;
    if (stbi_write_png(path.string().c_str(), static_cast<int>(extent.width),
                       static_cast<int>(extent.height), 4, pixels.data(), stride) == 0) {
        throw std::runtime_error(std::format("Failed to write '{}'", path.string()));
    }
}

} // namespace pt
