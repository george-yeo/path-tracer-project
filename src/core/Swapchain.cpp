#include "core/Swapchain.hpp"

#include "core/Device.hpp"
#include "core/VkCheck.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pt {

namespace {

bool canBlitTo(VkPhysicalDevice physicalDevice, VkFormat format) {
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
    return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0;
}

// The path tracer's display image is blitted into the swapchain, so any 8-bit BGRA/RGBA format
// works (blits convert channel order). UNORM is preferred: the resolve shader then applies the
// exact sRGB curve itself and ImGui's sRGB-authored colours are displayed as intended.
VkSurfaceFormatKHR chooseFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, formats.data());

    if (count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    constexpr VkFormat preferred[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB};
    for (VkFormat wanted : preferred) {
        for (const auto& candidate : formats) {
            if (candidate.format == wanted &&
                candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
                canBlitTo(physicalDevice, wanted)) {
                return candidate;
            }
        }
    }
    throw std::runtime_error("Surface offers no 8-bit RGBA/BGRA format usable as a blit target");
}

VkPresentModeKHR choosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                   bool vsync) {
    if (vsync) {
        return VK_PRESENT_MODE_FIFO_KHR; // always supported
    }
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, modes.data());

    for (VkPresentModeKHR wanted : {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR}) {
        if (std::find(modes.begin(), modes.end(), wanted) != modes.end()) {
            return wanted;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

} // namespace

Swapchain::Swapchain(Device& device, VkSurfaceKHR surface, VkExtent2D desiredExtent, bool vsync)
    : device(device), surface(surface) {
    create(desiredExtent, vsync, VK_NULL_HANDLE);
}

Swapchain::~Swapchain() {
    destroyImageResources();
    vkDestroySwapchainKHR(device.get(), swapchain, nullptr);
}

bool Swapchain::isSrgb() const {
    return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;
}

void Swapchain::recreate(VkExtent2D desiredExtent, bool vsync) {
    destroyImageResources();
    VkSwapchainKHR old = swapchain;
    create(desiredExtent, vsync, old);
    vkDestroySwapchainKHR(device.get(), old, nullptr);
}

void Swapchain::create(VkExtent2D desiredExtent, bool vsync, VkSwapchainKHR oldSwapchain) {
    VkPhysicalDevice physicalDevice = device.getPhysical();

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps));

    constexpr VkImageUsageFlags usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((caps.supportedUsageFlags & usage) != usage) {
        throw std::runtime_error("Swapchain images cannot be used as a blit destination");
    }

    VkSurfaceFormatKHR surfaceFormat = chooseFormat(physicalDevice, surface);
    format = surfaceFormat.format;

    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent = caps.currentExtent;
    } else {
        extent.width =
            std::clamp(desiredExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(desiredExtent.height, caps.minImageExtent.height,
                                   caps.maxImageExtent.height);
    }

    minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) {
        minImageCount = std::min(minImageCount, caps.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = minImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = usage;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // single queue family
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = choosePresentMode(physicalDevice, surface, vsync);
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VK_CHECK(vkCreateSwapchainKHR(device.get(), &createInfo, nullptr, &swapchain));

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(device.get(), swapchain, &count, nullptr);
    images.resize(count);
    vkGetSwapchainImagesKHR(device.get(), swapchain, &count, images.data());

    views.resize(count);
    renderFinished.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_CHECK(vkCreateImageView(device.get(), &viewInfo, nullptr, &views[i]));

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(device.get(), &semaphoreInfo, nullptr, &renderFinished[i]));
    }
}

void Swapchain::destroyImageResources() {
    for (VkImageView view : views) {
        vkDestroyImageView(device.get(), view, nullptr);
    }
    for (VkSemaphore semaphore : renderFinished) {
        vkDestroySemaphore(device.get(), semaphore, nullptr);
    }
    views.clear();
    renderFinished.clear();
    images.clear();
}

VkResult Swapchain::acquire(VkSemaphore signalSemaphore, uint32_t& imageIndex) {
    VkResult result = vkAcquireNextImageKHR(device.get(), swapchain, UINT64_MAX, signalSemaphore,
                                            VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(result);
    }
    return result;
}

VkResult Swapchain::present(VkQueue queue, uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished[imageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(result);
    }
    return result;
}

} // namespace pt
