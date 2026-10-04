#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <functional>
#include <string>

namespace pt {

// Physical device selection, logical device, the single graphics+compute+present queue, the VMA
// allocator, and a blocking "immediate submit" path for uploads and one-off transitions.
class Device {
    public:
    Device(VkInstance instance, VkSurfaceKHR surface);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    VkDevice get() const { return device; }
    VkPhysicalDevice getPhysical() const { return physicalDevice; }
    VkQueue getQueue() const { return queue; }
    uint32_t getQueueFamily() const { return queueFamily; }
    VmaAllocator getAllocator() const { return allocator; }
    const VkPhysicalDeviceProperties& getProperties() const { return properties; }
    const std::string& getName() const { return name; }

    // Nanoseconds per timestamp tick, or 0 if the queue does not support timestamps.
    double getTimestampPeriodNs() const { return timestampPeriodNs; }

    // Records commands into a transient command buffer, submits, and waits for completion.
    void immediateSubmit(const std::function<void(VkCommandBuffer)>& record);

    void waitIdle() const;

    private:
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VmaAllocator allocator = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties properties{};
    std::string name;
    double timestampPeriodNs = 0.0;

    VkCommandPool immediatePool = VK_NULL_HANDLE;
    VkFence immediateFence = VK_NULL_HANDLE;

    void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    void createLogicalDevice();
    void createAllocator(VkInstance instance);
};

} // namespace pt
