#pragma once

#include "app/CameraController.hpp"
#include "app/CommandLine.hpp"
#include "core/VulkanContext.hpp"
#include "renderer/Renderer.hpp"
#include "scene/Camera.hpp"
#include "window/Window.hpp"

#include <string>

namespace pt {

struct ScenePreset;

class Application {
    public:
    explicit Application(const AppOptions& options);

    // Runs until the window is closed (or, with --screenshot, until the image is saved).
    int run();

    private:
    AppOptions options;
    Window window;
    VulkanContext context;
    Renderer renderer;
    CameraController controller;

    const ScenePreset* preset = nullptr;
    Camera camera;
    Camera initialCamera;
    float moveSpeed = 2.0f;
    std::string status;

    void loadPreset(const ScenePreset& preset);
    void applyCommandLineSettings();
    void drawUi();
    void saveScreenshot();
};

} // namespace pt
