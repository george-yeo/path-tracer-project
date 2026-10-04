#include "app/Application.hpp"

#include "scene/ScenePresets.hpp"
#include "utils/Paths.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <format>

namespace pt {

namespace {

ContextOptions contextOptions(const AppOptions& options) {
    return {options.validation, options.syncValidation};
}

std::string formatCount(double value) {
    if (value >= 1e9) return std::format("{:.2f} G", value * 1e-9);
    if (value >= 1e6) return std::format("{:.2f} M", value * 1e-6);
    if (value >= 1e3) return std::format("{:.1f} k", value * 1e-3);
    return std::format("{:.0f}", value);
}

} // namespace

Application::Application(const AppOptions& options)
    : options(options),
      window(static_cast<int>(options.width), static_cast<int>(options.height), "Path Tracer"),
      context(window, contextOptions(options)), renderer(context, window, options.vsync) {
    const ScenePreset* initial = findScenePreset(options.scene);
    loadPreset(initial != nullptr ? *initial : scenePresets().front());
    applyCommandLineSettings();
}

void Application::applyCommandLineSettings() {
    RenderSettings& s = renderer.settings;
    if (options.targetSamples) s.targetSamples = *options.targetSamples;
    if (options.samplesPerFrame) s.samplesPerFrame = *options.samplesPerFrame;
    if (options.maxBounces) s.maxBounces = *options.maxBounces;
    if (options.integrator) s.integrator = *options.integrator;
    if (options.exposure) s.exposure = *options.exposure;
}

void Application::loadPreset(const ScenePreset& newPreset) {
    Scene scene = newPreset.build();
    renderer.loadScene(scene);
    preset = &newPreset;
    camera = scene.camera;
    initialCamera = scene.camera;
    moveSpeed = scene.moveSpeed;
    status.clear();
}

int Application::run() {
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    Camera lastCamera = camera;

    while (!window.shouldClose()) {
        // Once converged there is nothing new to show: sleep until input arrives instead of
        // spinning (replaces the old busy-wait FPS limiter).
        if (renderer.isConverged() && !controller.isActive() && options.screenshot.empty()) {
            window.waitEvents(0.25);
        } else {
            window.pollEvents();
        }

        auto now = Clock::now();
        float dt = std::min(std::chrono::duration<float>(now - last).count(), 0.1f);
        last = now;

        ImGuiLayer& ui = renderer.getUi();
        ui.beginFrame();
        if (options.screenshot.empty()) {
            drawUi();
        }
        controller.update(window.getHandle(), camera, dt, moveSpeed, !ui.wantsKeyboard(),
                          !ui.wantsMouse());
        if (camera != lastCamera) {
            renderer.resetAccumulation();
            lastCamera = camera;
        }

        renderer.drawFrame(camera);

        if (!options.screenshot.empty() && renderer.isConverged()) {
            renderer.saveScreenshot(options.screenshot);
            std::printf("Saved %s (%u spp)\n", options.screenshot.string().c_str(),
                        renderer.getSampleCount());
            break;
        }
    }

    if (options.validation || options.syncValidation) {
        std::printf("Validation errors: %u\n", context.getValidationErrorCount());
    }
    return 0;
}

void Application::saveScreenshot() {
    auto stamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1);
    auto path = paths::executableDir() / "screenshots" /
                std::format("{}-{}spp-{}.png", preset->id, renderer.getSampleCount(), stamp);
    try {
        renderer.saveScreenshot(path);
        status = "Saved " + path.string();
    } catch (const std::exception& e) {
        status = e.what();
    }
}

void Application::drawUi() {
    RenderSettings& settings = renderer.settings;
    bool restart = false;

    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Path Tracer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // Progress and performance
    const uint32_t samples = renderer.getSampleCount();
    const FrameStats& frame = renderer.getFrameStats();
    const VkExtent2D extent = renderer.getRenderExtent();
    ImGui::TextDisabled("%s", context.getDevice().getName().c_str());
    float progress = static_cast<float>(samples) / static_cast<float>(settings.targetSamples);
    std::string label = std::format("{} / {} spp", samples, settings.targetSamples);
    ImGui::ProgressBar(std::min(progress, 1.0f), ImVec2(260.0f, 0.0f), label.c_str());
    ImGui::Text("%ux%u   %.2f ms GPU   %s samples/s", extent.width, extent.height, frame.gpuMs,
                formatCount(renderer.isConverged() ? 0.0 : frame.samplesPerSecond).c_str());

    if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginCombo("Preset", preset->displayName)) {
            for (const ScenePreset& candidate : scenePresets()) {
                if (ImGui::Selectable(candidate.displayName, &candidate == preset) &&
                    &candidate != preset) {
                    try {
                        loadPreset(candidate);
                    } catch (const std::exception& e) {
                        status = e.what();
                    }
                }
            }
            ImGui::EndCombo();
        }

        const SceneStats& stats = renderer.getSceneStats();
        ImGui::Text("%s triangles, %u spheres, %u emissive triangles",
                    formatCount(stats.triangleCount).c_str(), stats.sphereCount, stats.lightCount);
        ImGui::Text("BVH: %u nodes, %u leaves, depth %u, SAH cost %.1f", stats.bvh.nodeCount,
                    stats.bvh.leafCount, stats.bvh.maxDepth, stats.bvh.sahCost);
        ImGui::Text("     built in %.1f ms (binned SAH)", stats.bvh.buildMs);
        restart |= ImGui::ColorEdit3("Sky", &settings.sky.x,
                                     ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    }

    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::DragFloat3("Position", &camera.position.x, moveSpeed * 0.01f);
        ImGui::DragFloat("Yaw", &camera.yawDeg, 0.25f, 0.0f, 0.0f, "%.1f deg");
        ImGui::DragFloat("Pitch", &camera.pitchDeg, 0.25f, -89.0f, 89.0f, "%.1f deg");
        ImGui::SliderFloat("Vertical FOV", &camera.fovYDeg, 20.0f, 120.0f, "%.0f deg");
        ImGui::DragFloat("Move speed", &moveSpeed, 0.1f, 0.1f, 1000.0f, "%.1f /s");
        if (ImGui::Button("Reset view")) {
            camera = initialCamera;
        }
        ImGui::TextDisabled("WASD move, Q/E down/up, Shift faster\nArrows or right-drag to look");
    }

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        int integrator = static_cast<int>(settings.integrator);
        if (ImGui::Combo("Integrator", &integrator, kIntegratorNames,
                         static_cast<int>(std::size(kIntegratorNames)))) {
            settings.integrator = static_cast<Integrator>(integrator);
            restart = true;
        }

        int bounces = static_cast<int>(settings.maxBounces);
        if (ImGui::SliderInt("Max bounces", &bounces, 1, 32)) {
            settings.maxBounces = static_cast<uint32_t>(bounces);
            restart = true;
        }

        int target = static_cast<int>(settings.targetSamples);
        if (ImGui::InputInt("Target spp", &target, 64, 1024)) {
            settings.targetSamples = static_cast<uint32_t>(std::clamp(target, 1, 1 << 20));
        }

        int perFrame = static_cast<int>(settings.samplesPerFrame);
        if (ImGui::SliderInt("Samples / frame", &perFrame, 1, 64)) {
            settings.samplesPerFrame = static_cast<uint32_t>(perFrame);
        }

        ImGui::SliderFloat("Exposure", &settings.exposure, 0.05f, 20.0f, "%.2f",
                           ImGuiSliderFlags_Logarithmic);

        bool vsync = renderer.getVsync();
        if (ImGui::Checkbox("VSync", &vsync)) {
            renderer.setVsync(vsync);
        }

        if (ImGui::Button("Restart")) {
            restart = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save screenshot")) {
            saveScreenshot();
        }
    }

    if (!status.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status.c_str());
    }
    ImGui::End();

    if (restart) {
        renderer.resetAccumulation();
    }
}

} // namespace pt
