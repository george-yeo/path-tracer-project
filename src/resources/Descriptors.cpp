#include "resources/Descriptors.hpp"

#include "core/Device.hpp"
#include "core/VkCheck.hpp"
#include "resources/Buffer.hpp"

namespace pt {

DescriptorSetLayout::DescriptorSetLayout(Device& device,
                                         std::span<const VkDescriptorSetLayoutBinding> bindings)
    : device(device) {
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(device.get(), &info, nullptr, &layout));
}

DescriptorSetLayout::~DescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(device.get(), layout, nullptr);
}

DescriptorPool::DescriptorPool(Device& device, uint32_t maxSets,
                               std::span<const VkDescriptorPoolSize> sizes)
    : device(device) {
    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets = maxSets;
    info.poolSizeCount = static_cast<uint32_t>(sizes.size());
    info.pPoolSizes = sizes.data();
    VK_CHECK(vkCreateDescriptorPool(device.get(), &info, nullptr, &pool));
}

DescriptorPool::~DescriptorPool() {
    vkDestroyDescriptorPool(device.get(), pool, nullptr);
}

VkDescriptorSet DescriptorPool::allocate(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device.get(), &info, &set));
    return set;
}

DescriptorWriter& DescriptorWriter::storageImage(uint32_t binding, VkImageView view,
                                                 VkImageLayout layout) {
    entries.push_back(
        {binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, {VK_NULL_HANDLE, view, layout}, {}});
    return *this;
}

DescriptorWriter& DescriptorWriter::storageBuffer(uint32_t binding, const Buffer& buffer) {
    entries.push_back(
        {binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, {}, {buffer.get(), 0, VK_WHOLE_SIZE}});
    return *this;
}

void DescriptorWriter::update(VkDevice device, VkDescriptorSet set) const {
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(entries.size());
    for (const Entry& entry : entries) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = entry.binding;
        write.descriptorCount = 1;
        write.descriptorType = entry.type;
        if (entry.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
            write.pImageInfo = &entry.image;
        } else {
            write.pBufferInfo = &entry.buffer;
        }
        writes.push_back(write);
    }
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

} // namespace pt
