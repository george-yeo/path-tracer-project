#pragma once

#include <vulkan/vulkan.h>

#include <vector>

struct GLFWwindow;

namespace pt {

// RAII wrapper around a GLFW window (and the GLFW library, since the app has exactly one window).
class Window {
    public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    GLFWwindow* getHandle() const { return handle; }

    bool shouldClose() const;
    void requestClose();
    void pollEvents();
    void waitEvents(double timeoutSeconds);

    // Framebuffer size in pixels; {0, 0} while minimised.
    VkExtent2D getFramebufferExtent() const;

    // True once after the framebuffer has been resized.
    bool consumeResized();

    VkSurfaceKHR createSurface(VkInstance instance) const;
    static std::vector<const char*> requiredInstanceExtensions();

    private:
    GLFWwindow* handle = nullptr;
    bool resized = false;
};

} // namespace pt
