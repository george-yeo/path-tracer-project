#include "resources/Buffer.hpp"

#include "core/Device.hpp"
#include "core/VkCheck.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace pt {

Buffer::Buffer(Device& device, VkDeviceSize requestedSize, VkBufferUsageFlags usage,
               MemoryUsage memory)
    : device(device), size(std::max<VkDeviceSize>(requestedSize, 16)) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    switch (memory) {
        case MemoryUsage::DeviceLocal: allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; break;
        case MemoryUsage::Upload:
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case MemoryUsage::Readback:
            allocInfo.flags =
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
    }

    VmaAllocationInfo result{};
    VK_CHECK(vmaCreateBuffer(device.getAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation,
                             &result));
    mapped = result.pMappedData;
}

Buffer::~Buffer() {
    vmaDestroyBuffer(device.getAllocator(), buffer, allocation);
}

void Buffer::write(const void* data, VkDeviceSize bytes, VkDeviceSize offset) {
    if (mapped == nullptr) {
        throw std::logic_error("Buffer::write on a buffer that is not host-visible");
    }
    if (offset + bytes > size) {
        throw std::out_of_range("Buffer::write exceeds buffer size");
    }
    std::memcpy(static_cast<char*>(mapped) + offset, data, static_cast<size_t>(bytes));
    // No-op on host-coherent memory; required otherwise.
    VK_CHECK(vmaFlushAllocation(device.getAllocator(), allocation, offset, bytes));
}

void Buffer::invalidate() {
    VK_CHECK(vmaInvalidateAllocation(device.getAllocator(), allocation, 0, VK_WHOLE_SIZE));
}

} // namespace pt
