#pragma once

#include <vulkan/vulkan.h>

#include <source_location>
#include <stdexcept>

namespace pt {

const char* toString(VkResult result);

class VulkanError : public std::runtime_error {
    public:
    VulkanError(VkResult result, const char* expression, std::source_location location);

    VkResult getResult() const { return result; }

    private:
    VkResult result;
};

inline void vkCheck(VkResult result, const char* expression,
                    std::source_location location = std::source_location::current()) {
    if (result != VK_SUCCESS) {
        throw VulkanError(result, expression, location);
    }
}

} // namespace pt

// Throws pt::VulkanError (with the failing call, result code and source location) on failure.
#define VK_CHECK(expr) ::pt::vkCheck((expr), #expr)
