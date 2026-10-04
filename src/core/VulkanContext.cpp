#include "core/VulkanContext.hpp"

#include "core/VkCheck.hpp"
#include "window/Window.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace pt {

namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

bool isLayerAvailable(const char* name) {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& layer : layers) {
        if (std::strcmp(layer.layerName, name) == 0) {
            return true;
        }
    }
    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT /*types*/,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data,
                                             void* userData) {
    const bool isError = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
    if (isError) {
        static_cast<std::atomic<uint32_t>*>(userData)->fetch_add(1);
    }
    std::fprintf(stderr, "[vulkan %s] %s\n\n", isError ? "error" : "warning", data->pMessage);
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT messengerInfo(std::atomic<uint32_t>* errorCounter) {
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    info.pUserData = errorCounter;
    return info;
}

} // namespace

VulkanContext::VulkanContext(const Window& window, const ContextOptions& options) {
    createInstance(options);
    surface = window.createSurface(instance);
    device = std::make_unique<Device>(instance, surface);
}

VulkanContext::~VulkanContext() {
    device.reset();
    vkDestroySurfaceKHR(instance, surface, nullptr);
    if (messenger != VK_NULL_HANDLE) {
        auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy != nullptr) {
            destroy(instance, messenger, nullptr);
        }
    }
    vkDestroyInstance(instance, nullptr);
}

void VulkanContext::createInstance(const ContextOptions& options) {
    uint32_t loaderVersion = VK_API_VERSION_1_0;
    vkEnumerateInstanceVersion(&loaderVersion);
    if (loaderVersion < VK_API_VERSION_1_3) {
        throw std::runtime_error("The installed Vulkan loader does not support Vulkan 1.3");
    }

    bool validation = options.validation || options.syncValidation;
    if (validation && !isLayerAvailable(kValidationLayer)) {
        std::fprintf(stderr,
                     "Validation requested but %s is not installed; continuing without it\n",
                     kValidationLayer);
        validation = false;
    }

    auto extensions = Window::requiredInstanceExtensions();
    if (validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Path Tracer";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.pEngineName = "pt";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Chained into instance creation so vkCreateInstance/vkDestroyInstance are also reported.
    auto debugInfo = messengerInfo(&validationErrors);
    VkValidationFeatureEnableEXT syncFeature =
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validationFeatures{};
    validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validationFeatures.enabledValidationFeatureCount = 1;
    validationFeatures.pEnabledValidationFeatures = &syncFeature;

    if (validation) {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &kValidationLayer;
        createInfo.pNext = &debugInfo;
        if (options.syncValidation) {
            debugInfo.pNext = &validationFeatures;
        }
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

    if (validation) {
        auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (create != nullptr) {
            debugInfo.pNext =
                nullptr; // the validation-features struct only belongs on the instance
            VK_CHECK(create(instance, &debugInfo, nullptr, &messenger));
        }
        std::printf("Vulkan validation enabled%s\n",
                    options.syncValidation ? " (with synchronization validation)" : "");
    }
}

} // namespace pt
