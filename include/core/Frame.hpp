#pragma once

#include <vulkan/vulkan.h>

namespace pt {

class Device;

// Per-frame-in-flight resources: command buffer, acquire semaphore, completion fence and a
// two-entry timestamp query pool for GPU timing.
class Frame {
    public:
    explicit Frame(Device& device);
    ~Frame();

    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    Frame(Frame&&) = delete;
    Frame& operator=(Frame&&) = delete;

    VkCommandBuffer getCommandBuffer() const { return commandBuffer; }
    VkSemaphore getImageAvailable() const { return imageAvailable; }
    VkFence getInFlightFence() const { return inFlight; }
    VkQueryPool getQueryPool() const { return queryPool; }

    void waitUntilAvailable() const;
    void reset();

    // True when the timestamps written by this frame's previous submission are valid to read.
    bool hasTimestamps = false;

    private:
    Device& device;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkFence inFlight = VK_NULL_HANDLE;
    VkQueryPool queryPool = VK_NULL_HANDLE;
};

} // namespace pt
