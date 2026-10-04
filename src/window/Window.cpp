#include "window/Window.hpp"

#include "core/VkCheck.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <stdexcept>

namespace pt {

Window::Window(int width, int height, const char* title) {
    glfwSetErrorCallback([](int code, const char* description) {
        std::fprintf(stderr, "[glfw] error %d: %s\n", code, description);
    });

    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error("Failed to initialise GLFW");
    }
    if (glfwVulkanSupported() == GLFW_FALSE) {
        glfwTerminate();
        throw std::runtime_error("GLFW could not find a Vulkan loader");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (handle == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create window");
    }

    glfwSetWindowUserPointer(handle, this);
    glfwSetFramebufferSizeCallback(handle, [](GLFWwindow* window, int, int) {
        static_cast<Window*>(glfwGetWindowUserPointer(window))->resized = true;
    });
}

Window::~Window() {
    glfwDestroyWindow(handle);
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(handle) == GLFW_TRUE;
}

void Window::requestClose() {
    glfwSetWindowShouldClose(handle, GLFW_TRUE);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::waitEvents(double timeoutSeconds) {
    glfwWaitEventsTimeout(timeoutSeconds);
}

VkExtent2D Window::getFramebufferExtent() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(handle, &width, &height);
    return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

bool Window::consumeResized() {
    bool wasResized = resized;
    resized = false;
    return wasResized;
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VK_CHECK(glfwCreateWindowSurface(instance, handle, nullptr, &surface));
    return surface;
}

std::vector<const char*> Window::requiredInstanceExtensions() {
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (extensions == nullptr) {
        throw std::runtime_error("GLFW reports no Vulkan surface support on this platform");
    }
    return {extensions, extensions + count};
}

} // namespace pt
