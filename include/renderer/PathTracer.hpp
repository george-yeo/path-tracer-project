#pragma once

#include "core/ComputePipeline.hpp"
#include "renderer/RenderSettings.hpp"
#include "resources/Descriptors.hpp"
#include "resources/Image.hpp"

#include <memory>

namespace pt {

class Device;
class GpuScene;
struct Camera;

// Progressive path tracing as two compute passes:
//   pathtrace.comp  adds N samples per pixel into an RGBA32F accumulation image
//   resolve.comp    averages, exposes, tone maps and encodes into an RGBA8 display image
// Keeping them separate means exposure changes never restart accumulation.
class PathTracer {
    public:
    PathTracer(Device& device, VkExtent2D extent);

    PathTracer(const PathTracer&) = delete;
    PathTracer& operator=(const PathTracer&) = delete;
    PathTracer(PathTracer&&) = delete;
    PathTracer& operator=(PathTracer&&) = delete;

    // Both require the device to be idle (they rewrite descriptors the GPU may be reading).
    void resize(VkExtent2D extent);
    void setScene(const GpuScene& scene);

    void resetAccumulation() { sampleCount = 0; }
    uint32_t getSampleCount() const { return sampleCount; }

    // Records this frame's work. Leaves the display image in TRANSFER_SRC_OPTIMAL, ready to blit.
    // Returns the number of samples per pixel traced in this frame (0 once converged).
    uint32_t record(VkCommandBuffer cmd, const Camera& camera, const RenderSettings& settings,
                    bool encodeSrgb);

    const Image& getDisplayImage() const { return *display; }
    VkImageLayout getDisplayLayout() const { return displayLayout; }
    bool isDisplaySrgbEncoded() const { return displaySrgbEncoded; }

    private:
    Device& device;
    DescriptorSetLayout setLayout;
    DescriptorPool pool;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    ComputePipeline tracePipeline;
    ComputePipeline resolvePipeline;

    std::unique_ptr<Image> accumulation;
    std::unique_ptr<Image> display;
    VkImageLayout accumulationLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout displayLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool displaySrgbEncoded = false;

    const GpuScene* scene = nullptr;
    uint32_t sampleCount = 0;
};

} // namespace pt
