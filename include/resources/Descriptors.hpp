#pragma once

#include <vulkan/vulkan.h>

#include <span>
#include <vector>

namespace pt {

class Buffer;
class Device;

class DescriptorSetLayout {
    public:
    DescriptorSetLayout(Device& device, std::span<const VkDescriptorSetLayoutBinding> bindings);
    ~DescriptorSetLayout();

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout(DescriptorSetLayout&&) = delete;
    DescriptorSetLayout& operator=(DescriptorSetLayout&&) = delete;

    VkDescriptorSetLayout get() const { return layout; }

    private:
    Device& device;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
};

class DescriptorPool {
    public:
    DescriptorPool(Device& device, uint32_t maxSets, std::span<const VkDescriptorPoolSize> sizes);
    ~DescriptorPool();

    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;
    DescriptorPool(DescriptorPool&&) = delete;
    DescriptorPool& operator=(DescriptorPool&&) = delete;

    // Sets are freed together with the pool.
    VkDescriptorSet allocate(VkDescriptorSetLayout layout);

    private:
    Device& device;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};

// Collects descriptor writes and applies them in one vkUpdateDescriptorSets call. The
// VkWriteDescriptorSet array is built inside update(), after every info has been added, so the
// pInfo pointers can never dangle (the old DescriptorSet relied on vector::reserve for that).
class DescriptorWriter {
    public:
    DescriptorWriter& storageImage(uint32_t binding, VkImageView view, VkImageLayout layout);
    DescriptorWriter& storageBuffer(uint32_t binding, const Buffer& buffer);

    void update(VkDevice device, VkDescriptorSet set) const;

    private:
    struct Entry {
        uint32_t binding;
        VkDescriptorType type;
        VkDescriptorImageInfo image;
        VkDescriptorBufferInfo buffer;
    };
    std::vector<Entry> entries;
};

} // namespace pt
