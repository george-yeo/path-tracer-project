#include "core/Device.hpp"

#include "core/VkCheck.hpp"

#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>

namespace pt {

namespace {

constexpr const char* kRequiredExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct Candidate {
    VkPhysicalDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    int score = 0;
};

bool supportsExtensions(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    for (const char* required : kRequiredExtensions) {
        bool found = false;
        for (const auto& extension : available) {
            if (std::strcmp(extension.extensionName, required) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

// One queue family that does graphics, compute and present keeps synchronisation simple
// (no queue family ownership transfers). Every desktop GPU exposes one.
std::optional<uint32_t> findQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    constexpr VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    for (uint32_t i = 0; i < count; ++i) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present);
        if ((families[i].queueFlags & required) == required && present == VK_TRUE) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<Candidate> evaluate(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(device, &props);
    if (props.apiVersion < VK_API_VERSION_1_3 || !supportsExtensions(device)) {
        return std::nullopt;
    }

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features13;
    vkGetPhysicalDeviceFeatures2(device, &features);
    if (features13.synchronization2 == VK_FALSE || features13.dynamicRendering == VK_FALSE) {
        return std::nullopt;
    }

    uint32_t formatCount = 0;
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (formatCount == 0 || presentModeCount == 0) {
        return std::nullopt;
    }

    auto family = findQueueFamily(device, surface);
    if (!family) {
        return std::nullopt;
    }

    int score = 1;
    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score = 1000; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 100; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score = 50; break;
        default: break;
    }
    return Candidate{device, *family, score};
}

} // namespace

Device::Device(VkInstance instance, VkSurfaceKHR surface) {
    pickPhysicalDevice(instance, surface);
    createLogicalDevice();
    createAllocator(instance);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &immediatePool));

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &immediateFence));
}

Device::~Device() {
    vkDeviceWaitIdle(device);
    vkDestroyFence(device, immediateFence, nullptr);
    vkDestroyCommandPool(device, immediatePool, nullptr);
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
}

void Device::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    std::optional<Candidate> best;
    for (VkPhysicalDevice candidate : devices) {
        auto result = evaluate(candidate, surface);
        if (result && (!best || result->score > best->score)) {
            best = result;
        }
    }
    if (!best) {
        throw std::runtime_error(
            "No GPU supports Vulkan 1.3 with synchronization2, dynamic rendering and presentation");
    }

    physicalDevice = best->device;
    queueFamily = best->queueFamily;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    name = properties.deviceName;

    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());
    if (families[queueFamily].timestampValidBits > 0) {
        timestampPeriodNs = properties.limits.timestampPeriod;
    }

    std::printf("Using GPU: %s\n", name.c_str());
}

void Device::createLogicalDevice() {
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features13;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &features;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(kRequiredExtensions));
    createInfo.ppEnabledExtensionNames = kRequiredExtensions;

    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
}

void Device::createAllocator(VkInstance instance) {
    VmaAllocatorCreateInfo info{};
    info.vulkanApiVersion = VK_API_VERSION_1_3;
    info.instance = instance;
    info.physicalDevice = physicalDevice;
    info.device = device;
    VK_CHECK(vmaCreateAllocator(&info, &allocator));
}

void Device::immediateSubmit(const std::function<void(VkCommandBuffer)>& record) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = immediatePool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
    record(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = cmd;

    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;

    VK_CHECK(vkQueueSubmit2(queue, 1, &submit, immediateFence));
    VK_CHECK(vkWaitForFences(device, 1, &immediateFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &immediateFence));
    VK_CHECK(vkResetCommandPool(device, immediatePool, 0));
}

void Device::waitIdle() const {
    VK_CHECK(vkDeviceWaitIdle(device));
}

} // namespace pt
