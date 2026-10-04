#pragma once

#include "core/Device.hpp"

#include <vulkan/vulkan.h>

#include <atomic>
#include <memory>

namespace pt {

class Window;

struct ContextOptions {
    bool validation = false;     // Khronos validation layer + debug messenger
    bool syncValidation = false; // additionally enable synchronization validation
};

// Instance, debug messenger, window surface, and the Device built on top of them.
class VulkanContext {
    public:
    VulkanContext(const Window& window, const ContextOptions& options);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    VkInstance getInstance() const { return instance; }
    VkSurfaceKHR getSurface() const { return surface; }
    Device& getDevice() { return *device; }
    const Device& getDevice() const { return *device; }

    // Number of validation errors reported so far (always 0 when validation is off).
    uint32_t getValidationErrorCount() const { return validationErrors.load(); }

    private:
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    std::unique_ptr<Device> device;
    std::atomic<uint32_t> validationErrors{0};

    void createInstance(const ContextOptions& options);
};

} // namespace pt
