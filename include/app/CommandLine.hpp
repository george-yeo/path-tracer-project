#pragma once

#include "renderer/RenderSettings.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace pt {

struct AppOptions {
    std::string scene = "cornell";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool vsync = false;

    std::optional<uint32_t> targetSamples;
    std::optional<uint32_t> samplesPerFrame;
    std::optional<uint32_t> maxBounces;
    std::optional<Integrator> integrator;
    std::optional<float> exposure;

    // When set: render until targetSamples is reached, save a PNG, and exit.
    std::filesystem::path screenshot;

    bool validation = false;
    bool syncValidation = false;
};

struct ParseResult {
    std::optional<AppOptions> options; // empty -> exit with exitCode
    int exitCode = 0;
};

ParseResult parseCommandLine(int argc, char** argv);

} // namespace pt
