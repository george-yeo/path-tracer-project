#include "renderer/PathTracer.hpp"

#include "core/Barriers.hpp"
#include "core/Device.hpp"
#include "renderer/GpuScene.hpp"
#include "renderer/GpuTypes.hpp"
#include "scene/Camera.hpp"
#include "utils/Paths.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace pt {

namespace {

// Must match the binding numbers in shaders/common.glsl and shaders/scene.glsl.
enum Binding : uint32_t {
    kAccumulation = 0,
    kDisplay = 1,
    kTriangles = 2,
    kShading = 3,
    kBvh = 4,
    kMaterials = 5,
    kSpheres = 6,
    kLights = 7,
};

constexpr uint32_t kWorkgroupSize = 8; // local_size_x/y in the shaders

constexpr VkDescriptorSetLayoutBinding binding(uint32_t index, VkDescriptorType type) {
    return {index, type, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
}

constexpr std::array kBindings = {
    binding(kAccumulation, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
    binding(kDisplay, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
    binding(kTriangles, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
    binding(kShading, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
    binding(kBvh, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
    binding(kMaterials, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
    binding(kSpheres, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
    binding(kLights, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
};

constexpr std::array kPoolSizes = {
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6},
};

uint32_t groupCount(uint32_t pixels) {
    return (pixels + kWorkgroupSize - 1) / kWorkgroupSize;
}

} // namespace

PathTracer::PathTracer(Device& device, VkExtent2D extent)
    : device(device), setLayout(device, kBindings), pool(device, 1, kPoolSizes),
      descriptorSet(pool.allocate(setLayout.get())),
      tracePipeline(device, paths::shader("pathtrace.comp.spv"), setLayout.get(),
                    sizeof(PushConstants)),
      resolvePipeline(device, paths::shader("resolve.comp.spv"), setLayout.get(),
                      sizeof(PushConstants)) {
    resize(extent);
}

void PathTracer::resize(VkExtent2D extent) {
    accumulation = std::make_unique<Image>(device, extent, VK_FORMAT_R32G32B32A32_SFLOAT,
                                           VK_IMAGE_USAGE_STORAGE_BIT);
    display = std::make_unique<Image>(device, extent, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    accumulationLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    displayLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    sampleCount = 0;

    DescriptorWriter()
        .storageImage(kAccumulation, accumulation->getView(), VK_IMAGE_LAYOUT_GENERAL)
        .storageImage(kDisplay, display->getView(), VK_IMAGE_LAYOUT_GENERAL)
        .update(device.get(), descriptorSet);
}

void PathTracer::setScene(const GpuScene& newScene) {
    scene = &newScene;
    sampleCount = 0;

    DescriptorWriter()
        .storageBuffer(kTriangles, newScene.getTriangles())
        .storageBuffer(kShading, newScene.getShading())
        .storageBuffer(kBvh, newScene.getBvhNodes())
        .storageBuffer(kMaterials, newScene.getMaterials())
        .storageBuffer(kSpheres, newScene.getSpheres())
        .storageBuffer(kLights, newScene.getLights())
        .update(device.get(), descriptorSet);
}

uint32_t PathTracer::record(VkCommandBuffer cmd, const Camera& camera,
                            const RenderSettings& settings, bool encodeSrgb) {
    const VkExtent2D extent = display->getExtent();
    const BarrierScope computeWrite{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    const BarrierScope computeRead{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                   VK_ACCESS_2_SHADER_STORAGE_READ_BIT};
    const BarrierScope computeReadWrite{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};

    if (accumulationLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
        imageBarrier(cmd, accumulation->get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                     {}, computeReadWrite);
        accumulationLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    const float tanHalfFov = std::tan(glm::radians(camera.fovYDeg) * 0.5f);
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    PushConstants push{};
    push.cameraPosition = glm::vec4(camera.position, tanHalfFov);
    push.cameraRight = glm::vec4(camera.right(), aspect);
    push.cameraUp = glm::vec4(camera.up(), 0.0f);
    push.cameraForward = glm::vec4(camera.forward(), 0.0f);
    push.sky = glm::vec4(settings.sky, 0.0f);
    push.maxBounces = settings.maxBounces;
    push.integrator = static_cast<uint32_t>(settings.integrator);
    push.sphereCount = scene != nullptr ? scene->getSphereCount() : 0;
    push.lightCount = scene != nullptr ? scene->getLightCount() : 0;
    push.exposure = settings.exposure;
    push.flags = (encodeSrgb ? kFlagEncodeSrgb : 0u) |
                 (settings.integrator == Integrator::BvhHeatmap ? kFlagRawOutput : 0u);

    uint32_t traced = 0;
    if (scene != nullptr && sampleCount < settings.targetSamples) {
        traced =
            std::min(std::max(settings.samplesPerFrame, 1u), settings.targetSamples - sampleCount);

        // The previous frame's trace wrote the accumulation image (RAW/WAW) and its resolve read
        // it (WAR). This was the missing barrier behind the SYNC-HAZARD-WRITE-AFTER-WRITE errors.
        memoryBarrier(cmd, computeWrite, computeReadWrite);

        push.sampleIndex = sampleCount;
        push.sampleCount = traced;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tracePipeline.get());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tracePipeline.getLayout(), 0,
                                1, &descriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, tracePipeline.getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(push), &push);
        vkCmdDispatch(cmd, groupCount(extent.width), groupCount(extent.height), 1);
        sampleCount += traced;

        memoryBarrier(cmd, computeWrite, computeRead);
    }

    // Display image: last frame's blit/copy read it, now the resolve pass overwrites it.
    BarrierScope previousUse{};
    if (displayLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
        previousUse.stage = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT; // WAR: execution dependency only
    }
    imageBarrier(cmd, display->get(), displayLayout, VK_IMAGE_LAYOUT_GENERAL, previousUse,
                 computeWrite);

    push.sampleIndex = 0;
    push.sampleCount = sampleCount;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, resolvePipeline.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, resolvePipeline.getLayout(), 0, 1,
                            &descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, resolvePipeline.getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push), &push);
    vkCmdDispatch(cmd, groupCount(extent.width), groupCount(extent.height), 1);

    imageBarrier(cmd, display->get(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 computeWrite,
                 {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT});
    displayLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    displaySrgbEncoded = encodeSrgb;
    return traced;
}

} // namespace pt
