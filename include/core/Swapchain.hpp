#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace pt {

class Device;

class Swapchain {
    public:
    Swapchain(Device& device, VkSurfaceKHR surface, VkExtent2D desiredExtent, bool vsync);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    // Caller must ensure the device is idle (no image is still in use).
    void recreate(VkExtent2D desiredExtent, bool vsync);

    // Returns VK_SUCCESS, VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR; throws on anything else.
    VkResult acquire(VkSemaphore signalSemaphore, uint32_t& imageIndex);
    // Waits on getRenderFinished(imageIndex). Same return contract as acquire().
    VkResult present(VkQueue queue, uint32_t imageIndex);

    VkSwapchainKHR get() const { return swapchain; }
    VkFormat getFormat() const { return format; }
    bool isSrgb() const;
    VkExtent2D getExtent() const { return extent; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(images.size()); }
    uint32_t getMinImageCount() const { return minImageCount; }

    VkImage getImage(uint32_t index) const { return images[index]; }
    VkImageView getView(uint32_t index) const { return views[index]; }
    // One "render finished" semaphore per swapchain image: the present engine may still be
    // waiting on it when the next frame-in-flight starts, so it cannot be per-frame.
    VkSemaphore getRenderFinished(uint32_t index) const { return renderFinished[index]; }

    private:
    Device& device;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    uint32_t minImageCount = 2;

    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkSemaphore> renderFinished;

    void create(VkExtent2D desiredExtent, bool vsync, VkSwapchainKHR oldSwapchain);
    void destroyImageResources();
};

} // namespace pt
