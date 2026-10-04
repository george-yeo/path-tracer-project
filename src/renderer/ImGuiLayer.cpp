#include "renderer/ImGuiLayer.hpp"

#include "core/VkCheck.hpp"
#include "core/VulkanContext.hpp"
#include "window/Window.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <cstdio>
#include <cstdlib>

namespace pt {

namespace {

void checkImGuiVulkanResult(VkResult result) {
    if (result < 0) {
        std::fprintf(stderr, "[imgui] Vulkan call failed: %s\n", toString(result));
        std::abort();
    }
}

void applyStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(12.0f, 10.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.Colors[ImGuiCol_WindowBg].w = 0.92f;
}

} // namespace

ImGuiLayer::ImGuiLayer(VulkanContext& context, Window& window, VkFormat colorFormat,
                       uint32_t minImageCount, uint32_t imageCount)
    : colorFormat(colorFormat) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // don't drop imgui.ini into whatever directory we were run from
    applyStyle();

    ImGui_ImplGlfw_InitForVulkan(window.getHandle(), true);

    Device& device = context.getDevice();
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = VK_API_VERSION_1_3;
    info.Instance = context.getInstance();
    info.PhysicalDevice = device.getPhysical();
    info.Device = device.get();
    info.QueueFamily = device.getQueueFamily();
    info.Queue = device.getQueue();
    info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
    info.MinImageCount = minImageCount;
    info.ImageCount = imageCount;
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &this->colorFormat;
    info.CheckVkResultFn = checkImGuiVulkanResult;
    ImGui_ImplVulkan_Init(&info);
}

ImGuiLayer::~ImGuiLayer() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    frameActive = true;
}

void ImGuiLayer::endFrameWithoutRendering() {
    if (frameActive) {
        ImGui::EndFrame();
        frameActive = false;
    }
}

void ImGuiLayer::record(VkCommandBuffer cmd, VkImageView target, VkExtent2D extent) {
    ImGui::Render();
    frameActive = false;

    VkRenderingAttachmentInfo color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = target;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea = {{0, 0}, extent};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;

    vkCmdBeginRendering(cmd, &rendering);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void ImGuiLayer::setMinImageCount(uint32_t count) {
    ImGui_ImplVulkan_SetMinImageCount(count);
}

bool ImGuiLayer::wantsKeyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool ImGuiLayer::wantsMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

} // namespace pt
