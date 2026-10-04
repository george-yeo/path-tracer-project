#pragma once

#include <vulkan/vulkan.h>

namespace pt {

// Thin synchronization2 helpers. Every barrier in the renderer goes through these, so each one
// states its full (stage, access) -> (stage, access) dependency explicitly at the call site.

struct BarrierScope {
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
};

void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout,
                  VkImageLayout newLayout, BarrierScope src, BarrierScope dst);

void memoryBarrier(VkCommandBuffer cmd, BarrierScope src, BarrierScope dst);

} // namespace pt
