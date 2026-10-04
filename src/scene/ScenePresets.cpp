#include "scene/ScenePresets.hpp"

#include "scene/MeshLoader.hpp"
#include "utils/Paths.hpp"

#include <array>

namespace pt {

namespace {

void addTriangle(Scene& scene, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 normal,
                 uint32_t material) {
    scene.triangles.push_back({v0, v1, v2, normal, normal, normal, material});
}

// The Cornell box geometry from the original ApplicationV1::createScene(), unchanged.
Scene buildCornellBox() {
    Scene scene;
    scene.name = "Cornell box";
    scene.camera = {.position = {0.0f, 2.5f, 4.0f}, .yawDeg = 0.0f, .pitchDeg = 0.0f};
    scene.moveSpeed = 2.0f;

    const uint32_t white = scene.addMaterial({.color = {0.73f, 0.73f, 0.73f}, .roughness = 1.0f});
    const uint32_t red = scene.addMaterial({.color = {0.65f, 0.05f, 0.05f}, .roughness = 0.1f});
    const uint32_t green = scene.addMaterial({.color = {0.12f, 0.45f, 0.15f}, .roughness = 0.1f});
    const uint32_t light = scene.addMaterial({.color = {1.0f, 1.0f, 1.0f}, .emission = 15.0f});

    auto tri = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 n, uint32_t material) {
        addTriangle(scene, v0, v1, v2, n, material);
    };

    // clang-format off
    // Area light
    tri({-0.884011f, 5.319334f, -3.567968f}, {0.415989f, 5.319334f, -2.517968f}, {-0.884011f, 5.319334f, -2.517968f}, {0.0f, -1.0f, 0.0f}, light);
    tri({-0.884011f, 5.319334f, -3.567968f}, {0.415989f, 5.319334f, -3.567968f}, {0.415989f, 5.319334f, -2.517968f}, {0.0f, -1.0f, 0.0f}, light);

    // Back wall, ceiling, floor
    tri({-2.98f, -0.16f, -5.84f}, {2.55f, 5.33f, -5.84f}, {-2.98f, 5.33f, -5.84f}, {0.0f, 0.0f, 1.0f}, white);
    tri({-2.98f, -0.16f, -5.84f}, {2.55f, -0.16f, -5.84f}, {2.55f, 5.33f, -5.84f}, {0.0f, 0.0f, 1.0f}, white);
    tri({-2.98f, 5.33f, -5.84f}, {2.55f, 5.33f, -0.25f}, {-2.98f, 5.33f, -0.25f}, {0.0f, -1.0f, 0.0f}, white);
    tri({-2.98f, 5.33f, -5.84f}, {2.55f, 5.33f, -5.84f}, {2.55f, 5.33f, -0.25f}, {0.0f, -1.0f, 0.0f}, white);
    tri({2.55f, -0.16f, -0.25f}, {-2.98f, -0.16f, -5.84f}, {-2.98f, -0.16f, -0.25f}, {0.0f, 1.0f, 0.0f}, white);
    tri({2.55f, -0.16f, -0.25f}, {2.55f, -0.16f, -5.84f}, {-2.98f, -0.16f, -5.84f}, {0.0f, 1.0f, 0.0f}, white);

    // Left (red) and right (green) walls
    tri({-2.98f, -0.16f, -0.25f}, {-2.98f, 5.33f, -5.84f}, {-2.98f, 5.33f, -0.25f}, {1.0f, 0.0f, 0.0f}, red);
    tri({-2.98f, -0.16f, -0.25f}, {-2.98f, -0.16f, -5.84f}, {-2.98f, 5.33f, -5.84f}, {1.0f, 0.0f, 0.0f}, red);
    tri({2.55f, -0.16f, -0.25f}, {2.55f, 5.33f, -5.84f}, {2.55f, -0.16f, -5.84f}, {-1.0f, 0.0f, 0.0f}, green);
    tri({2.55f, -0.16f, -0.25f}, {2.55f, 5.33f, -0.25f}, {2.55f, 5.33f, -5.84f}, {-1.0f, 0.0f, 0.0f}, green);

    // Short box
    tri({1.245989f, 1.491249f, -0.894913f}, {0.145989f, 1.489601f, -2.964912f}, {-0.354011f, 1.490859f, -1.384912f}, {0.0f, 1.0f, -0.0008f}, white);
    tri({1.245989f, 1.491249f, -0.894913f}, {1.725989f, 1.489975f, -2.494912f}, {0.145989f, 1.489601f, -2.964912f}, {0.0f, 1.0f, -0.0008f}, white);
    tri({1.245989f, 1.491249f, -0.894913f}, {-0.354011f, -0.159141f, -1.383599f}, {1.245989f, -0.158750f, -0.893599f}, {0.3017f, 0.0000f, 0.9534f}, white);
    tri({1.245989f, 1.491249f, -0.894913f}, {-0.354011f, 1.490859f, -1.384912f}, {-0.354011f, -0.159141f, -1.383599f}, {0.3017f, 0.0000f, 0.9534f}, white);
    tri({0.145989f, 1.489601f, -2.964912f}, {0.145989f, -0.160399f, -2.963598f}, {-0.354011f, -0.159141f, -1.383599f}, {-0.9562f, 0.0000f, -0.2928f}, white);
    tri({0.145989f, 1.489601f, -2.964912f}, {-0.354011f, -0.159141f, -1.383599f}, {-0.354011f, 1.490859f, -1.384912f}, {-0.9562f, 0.0000f, -0.2928f}, white);
    tri({1.725989f, 1.489975f, -2.494912f}, {1.725989f, -0.160024f, -2.493598f}, {0.145989f, -0.160399f, -2.963598f}, {0.2873f, 0.0000f, -0.9578f}, white);
    tri({1.725989f, 1.489975f, -2.494912f}, {0.145989f, -0.160399f, -2.963598f}, {0.145989f, 1.489601f, -2.964912f}, {0.2873f, 0.0000f, -0.9578f}, white);
    tri({1.245989f, 1.491249f, -0.894913f}, {1.725989f, -0.160024f, -2.493598f}, {1.725989f, 1.489975f, -2.494912f}, {0.9585f, 0.0000f, 0.2851f}, white);
    tri({1.245989f, 1.491249f, -0.894913f}, {1.245989f, -0.158750f, -0.893599f}, {1.725989f, -0.160024f, -2.493598f}, {0.9585f, 0.0000f, 0.2851f}, white);

    // Tall box
    tri({-1.684011f, 3.139799f, -2.716226f}, {-0.594011f, 3.138135f, -4.806226f}, {-2.174011f, 3.138533f, -4.306226f}, {0.0f, 1.0f, 0.0f}, white);
    tri({-1.684011f, 3.139799f, -2.716226f}, {-0.104011f, 3.139409f, -3.206226f}, {-0.594011f, 3.138135f, -4.806226f}, {0.0f, 1.0f, 0.0f}, white);
    tri({-1.684011f, -0.17f, -2.713598f}, {-2.174011f, 3.138533f, -4.306226f}, {-2.174011f, -0.17f, -4.303598f}, {-0.9556f, 0.0002f, 0.2945f}, white);
    tri({-1.684011f, -0.17f, -2.713598f}, {-1.684011f, 3.139799f, -2.716226f}, {-2.174011f, 3.138533f, -4.306226f}, {-0.9556f, 0.0002f, 0.2945f}, white);
    tri({-2.174011f, -0.17f, -4.303598f}, {-0.594011f, 3.138135f, -4.806226f}, {-0.594011f, -0.17f, -4.803598f}, {-0.3017f, -0.0008f, -0.9534f}, white);
    tri({-2.174011f, -0.17f, -4.303598f}, {-2.174011f, 3.138533f, -4.306226f}, {-0.594011f, 3.138135f, -4.806226f}, {-0.3017f, -0.0008f, -0.9534f}, white);
    tri({-0.594011f, -0.17f, -4.803598f}, {-0.104011f, 3.139409f, -3.206226f}, {-0.104011f, -0.17f, -3.203598f}, {0.9562f, -0.0002f, -0.2928f}, white);
    tri({-0.594011f, -0.17f, -4.803598f}, {-0.594011f, 3.138135f, -4.806226f}, {-0.104011f, 3.139409f, -3.206226f}, {0.9562f, -0.0002f, -0.2928f}, white);
    tri({-0.104011f, -0.17f, -3.203598f}, {-1.684011f, 3.139799f, -2.716226f}, {-1.684011f, -0.17f, -2.713598f}, {0.2962f, 0.0008f, 0.9551f}, white);
    tri({-0.104011f, -0.17f, -3.203598f}, {-0.104011f, 3.139409f, -3.206226f}, {-1.684011f, 3.139799f, -2.716226f}, {0.2962f, 0.0008f, 0.9551f}, white);
    // clang-format on

    return scene;
}

// Ground sphere + warm overhead area light, as in the original (commented-out) teapot scene.
void addStudio(Scene& scene, float groundY) {
    const uint32_t ground = scene.addMaterial({.color = {0.2f, 0.2f, 0.2f}, .roughness = 0.2f});
    const uint32_t light =
        scene.addMaterial({.color = {0.8f, 0.6f, 0.3f}, .roughness = 1.0f, .emission = 10.0f});

    constexpr float groundRadius = 4000.0f;
    scene.spheres.push_back({{0.0f, groundY - groundRadius, -5.0f}, groundRadius, ground});

    constexpr float lightY = 150.0f;
    constexpr float size = 50.0f;
    const glm::vec3 down{0.0f, -1.0f, 0.0f};
    const glm::vec3 c{0.0f, lightY, 0.0f};
    addTriangle(scene, c + glm::vec3(-size, 0, -size), c + glm::vec3(size, 0, -size),
                c + glm::vec3(-size, 0, size), down, light);
    addTriangle(scene, c + glm::vec3(size, 0, -size), c + glm::vec3(size, 0, size),
                c + glm::vec3(-size, 0, size), down, light);
}

void addMesh(Scene& scene, const char* file, uint32_t material) {
    auto triangles = loadObj(paths::asset(file), material);
    scene.triangles.insert(scene.triangles.end(), triangles.begin(), triangles.end());
}

Scene buildTeapot() {
    Scene scene;
    scene.name = "Utah teapot";
    scene.camera = {.position = {40.0f, 80.0f, 130.0f}, .yawDeg = -15.0f, .pitchDeg = -20.0f};
    scene.moveSpeed = 40.0f;

    addStudio(scene, 0.0f);
    const uint32_t copper =
        scene.addMaterial({.color = {0.57f, 0.25f, 0.17f}, .roughness = 0.2f, .metallic = 0.4f});
    addMesh(scene, "utah_teapot.obj", copper);
    return scene;
}

Scene buildDragon() {
    Scene scene;
    scene.name = "Stanford dragon";
    scene.camera = {.position = glm::vec3(35.0f, -10.0f, 110.0f) * 1.4f, .yawDeg = -10.0f};
    scene.moveSpeed = 40.0f;

    addStudio(scene, -62.75f); // dragon mesh's lowest point
    const uint32_t gold =
        scene.addMaterial({.color = {1.0f, 0.71f, 0.29f}, .roughness = 0.3f, .metallic = 1.0f});
    addMesh(scene, "stanford_dragon.obj", gold);
    return scene;
}

constexpr std::array kPresets = {
    ScenePreset{"cornell", "Cornell box", buildCornellBox},
    ScenePreset{"teapot", "Utah teapot", buildTeapot},
    ScenePreset{"dragon", "Stanford dragon", buildDragon},
};

} // namespace

std::span<const ScenePreset> scenePresets() {
    return kPresets;
}

const ScenePreset* findScenePreset(std::string_view id) {
    for (const auto& preset : kPresets) {
        if (id == preset.id) {
            return &preset;
        }
    }
    return nullptr;
}

} // namespace pt
