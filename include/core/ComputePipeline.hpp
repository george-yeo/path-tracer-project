#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>

namespace pt {

class Device;

// A compute pipeline plus its layout: one descriptor set and one push-constant range.
class ComputePipeline {
    public:
    ComputePipeline(Device& device, const std::filesystem::path& spirvPath,
                    VkDescriptorSetLayout setLayout, uint32_t pushConstantSize);
    ~ComputePipeline();

    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;
    ComputePipeline(ComputePipeline&&) = delete;
    ComputePipeline& operator=(ComputePipeline&&) = delete;

    VkPipeline get() const { return pipeline; }
    VkPipelineLayout getLayout() const { return layout; }

    private:
    Device& device;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

} // namespace pt
