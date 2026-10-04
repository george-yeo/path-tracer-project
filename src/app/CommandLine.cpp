#include "app/CommandLine.hpp"

#include "scene/ScenePresets.hpp"

#include <charconv>
#include <cstdio>
#include <string_view>

namespace pt {

namespace {

void printUsage(const char* program) {
    std::printf("Usage: %s [options]\n\n", program);
    std::printf("  --scene <id>          scene preset:");
    for (const auto& preset : scenePresets()) {
        std::printf(" %s", preset.id);
    }
    std::printf(" (default cornell)\n"
                "  --size <W>x<H>        window size (default 1280x720)\n"
                "  --spp <n>             target samples per pixel (default 1024)\n"
                "  --spf <n>             samples per pixel per frame (default 1)\n"
                "  --bounces <n>         maximum path length in bounces (default 8)\n"
                "  --integrator <name>   mis | bsdf | light | heatmap (default mis)\n"
                "  --exposure <x>        exposure multiplier (default 1)\n"
                "  --vsync               present with FIFO (default: mailbox/immediate)\n"
                "  --screenshot <file>   render to --spp, save a PNG, and exit\n"
                "  --validation          enable the Vulkan validation layer\n"
                "  --sync-validation     also enable synchronization validation\n"
                "  --help                show this message\n");
}

template <typename T> bool parseNumber(std::string_view text, T& out) {
    const char* end = text.data() + text.size();
    auto [ptr, error] = std::from_chars(text.data(), end, out);
    return error == std::errc() && ptr == end;
}

std::optional<Integrator> parseIntegrator(std::string_view name) {
    if (name == "mis") return Integrator::Mis;
    if (name == "bsdf") return Integrator::BsdfOnly;
    if (name == "light") return Integrator::LightOnly;
    if (name == "heatmap") return Integrator::BvhHeatmap;
    return std::nullopt;
}

} // namespace

ParseResult parseCommandLine(int argc, char** argv) {
    AppOptions options;
    auto fail = [&](const char* message, std::string_view arg) {
        std::fprintf(stderr, "%s: %.*s\n\n", message, static_cast<int>(arg.size()), arg.data());
        printUsage(argv[0]);
        return ParseResult{std::nullopt, 2};
    };

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        auto value = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            return std::string_view(argv[++i]);
        };

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return {std::nullopt, 0};
        }
        if (arg == "--vsync") {
            options.vsync = true;
        } else if (arg == "--validation") {
            options.validation = true;
        } else if (arg == "--sync-validation") {
            options.syncValidation = true;
        } else if (arg == "--scene") {
            auto v = value();
            if (!v || findScenePreset(*v) == nullptr) return fail("Unknown scene", v.value_or(arg));
            options.scene = std::string(*v);
        } else if (arg == "--size") {
            auto v = value();
            auto x = v ? v->find('x') : std::string_view::npos;
            if (x == std::string_view::npos || !parseNumber(v->substr(0, x), options.width) ||
                !parseNumber(v->substr(x + 1), options.height) || options.width == 0 ||
                options.height == 0) {
                return fail("Invalid size (expected WxH)", v.value_or(arg));
            }
        } else if (arg == "--spp" || arg == "--spf" || arg == "--bounces") {
            auto v = value();
            uint32_t n = 0;
            if (!v || !parseNumber(*v, n) || n == 0)
                return fail("Expected a positive integer", arg);
            if (arg == "--spp") options.targetSamples = n;
            if (arg == "--spf") options.samplesPerFrame = n;
            if (arg == "--bounces") options.maxBounces = n;
        } else if (arg == "--integrator") {
            auto v = value();
            auto integrator = v ? parseIntegrator(*v) : std::nullopt;
            if (!integrator) return fail("Unknown integrator", v.value_or(arg));
            options.integrator = integrator;
        } else if (arg == "--exposure") {
            auto v = value();
            float exposure = 0.0f;
            if (!v || !parseNumber(*v, exposure) || exposure <= 0.0f) {
                return fail("Expected a positive number", arg);
            }
            options.exposure = exposure;
        } else if (arg == "--screenshot") {
            auto v = value();
            if (!v) return fail("Missing file name", arg);
            options.screenshot = std::filesystem::path(*v);
        } else {
            return fail("Unknown option", arg);
        }
    }
    return {options, 0};
}

} // namespace pt
