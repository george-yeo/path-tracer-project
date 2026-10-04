#pragma once

#include "core/Frame.hpp"
#include "core/Swapchain.hpp"
#include "renderer/GpuScene.hpp"
#include "renderer/ImGuiLayer.hpp"
#include "renderer/PathTracer.hpp"
#include "renderer/RenderSettings.hpp"
#include "scene/Bvh.hpp"

#include <array>
#include <filesystem>
#include <memory>

namespace pt {

class VulkanContext;
class Window;
struct Camera;
struct Scene;

struct SceneStats {
    uint32_t triangleCount = 0;
    uint32_t sphereCount = 0;
    uint32_t lightCount = 0;
    BvhStats bvh;
};

struct FrameStats {
    double gpuMs = 0.0;            // GPU time of the path tracing + resolve passes (smoothed)
    double samplesPerSecond = 0.0; // pixel samples per second (smoothed)
};

// Owns everything per-frame: swapchain, frames in flight, the path tracer, the scene's GPU
// buffers and the UI layer. drawFrame() is the only place command buffers are submitted.
class Renderer {
    public:
    Renderer(VulkanContext& context, Window& window, bool vsync);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // Compiles the scene (BVH, light table), uploads it and restarts accumulation.
    void loadScene(const Scene& scene);

    void drawFrame(const Camera& camera);

    void resetAccumulation() { pathTracer->resetAccumulation(); }
    uint32_t getSampleCount() const { return pathTracer->getSampleCount(); }
    bool isConverged() const { return getSampleCount() >= settings.targetSamples; }

    void setVsync(bool enabled);
    bool getVsync() const { return vsync; }

    // Writes the current display image (what is on screen, minus the UI) as a PNG.
    void saveScreenshot(const std::filesystem::path& path);

    RenderSettings settings;

    ImGuiLayer& getUi() { return *ui; }
    const SceneStats& getSceneStats() const { return sceneStats; }
    const FrameStats& getFrameStats() const { return frameStats; }
    VkExtent2D getRenderExtent() const { return swapchain->getExtent(); }

    private:
    static constexpr uint32_t kFramesInFlight = 2;

    VulkanContext& context;
    Device& device;
    Window& window;
    bool vsync;
    bool swapchainDirty = false;

    std::unique_ptr<Swapchain> swapchain;
    std::array<std::unique_ptr<Frame>, kFramesInFlight> frames;
    uint32_t frameIndex = 0;
    std::array<uint32_t, kFramesInFlight> samplesInFlight{}; // samples traced by each frame slot

    std::unique_ptr<PathTracer> pathTracer;
    std::unique_ptr<GpuScene> gpuScene;
    std::unique_ptr<ImGuiLayer> ui;

    SceneStats sceneStats;
    FrameStats frameStats;

    void recreateSwapchain();
    void recordFrame(Frame& frame, uint32_t imageIndex, const Camera& camera);
    void readTimestamps(Frame& frame, uint32_t slot);
};

} // namespace pt
