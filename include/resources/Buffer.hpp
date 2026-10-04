#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace pt {

class Device;

enum class MemoryUsage {
    DeviceLocal, // GPU-only; filled via a staging copy
    Upload,      // host-visible, persistently mapped, written sequentially by the CPU
    Readback,    // host-visible, persistently mapped, read by the CPU
};

class Buffer {
    public:
    // Sizes are clamped to a 16-byte minimum: Vulkan forbids zero-sized buffers, and the scene
    // may legitimately have zero spheres or lights.
    Buffer(Device& device, VkDeviceSize size, VkBufferUsageFlags usage, MemoryUsage memory);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;

    VkBuffer get() const { return buffer; }
    VkDeviceSize getSize() const { return size; }

    // Upload/Readback only.
    void* getMapped() const { return mapped; }
    void write(const void* data, VkDeviceSize bytes, VkDeviceSize offset = 0);
    void invalidate(); // make GPU writes visible to the host before reading getMapped()

    private:
    Device& device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;
};

} // namespace pt
