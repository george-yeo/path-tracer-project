#pragma once

#include "scene/Scene.hpp"

#include <span>
#include <string_view>

namespace pt {

struct ScenePreset {
    const char* id;          // used on the command line: --scene <id>
    const char* displayName; // shown in the UI
    Scene (*build)();
};

std::span<const ScenePreset> scenePresets();

// nullptr if no preset has this id.
const ScenePreset* findScenePreset(std::string_view id);

} // namespace pt
