#pragma once

#include <vulkan/vulkan.h>

namespace pt {

class VulkanContext;
class Window;

// Dear ImGui on top of GLFW + Vulkan dynamic rendering (no render pass / framebuffer objects).
class ImGuiLayer {
    public:
    ImGuiLayer(VulkanContext& context, Window& window, VkFormat colorFormat, uint32_t minImageCount,
               uint32_t imageCount);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    void beginFrame();
    void endFrameWithoutRendering(); // for frames that are skipped (minimised, out-of-date)

    // Finalises the ImGui frame and draws it onto `target`, which must be in
    // COLOR_ATTACHMENT_OPTIMAL. Existing contents are kept (load op LOAD).
    void record(VkCommandBuffer cmd, VkImageView target, VkExtent2D extent);

    void setMinImageCount(uint32_t count);

    bool wantsKeyboard() const;
    bool wantsMouse() const;

    private:
    VkFormat colorFormat; // must outlive ImGui_ImplVulkan_Init (pointed to by the init info)
    bool frameActive = false;
};

} // namespace pt
