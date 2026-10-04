#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace pt {

class Device;

// Device-local 2D colour image with a full-image view. Layout tracking is the owner's job:
// the owner knows which pass touched the image last, which is what a barrier needs.
class Image {
    public:
    Image(Device& device, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage);
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&&) = delete;
    Image& operator=(Image&&) = delete;

    VkImage get() const { return image; }
    VkImageView getView() const { return view; }
    VkFormat getFormat() const { return format; }
    VkExtent2D getExtent() const { return extent; }

    private:
    Device& device;
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format;
    VkExtent2D extent;
};

} // namespace pt
