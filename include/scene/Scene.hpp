#pragma once

#include "scene/Camera.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace pt {

// Metal/roughness material. Emission radiance is `color * emission`.
struct Material {
    glm::vec3 color{1.0f};
    float roughness = 1.0f;
    float metallic = 0.0f;
    float emission = 0.0f;
};

// Triangle with per-vertex shading normals. Winding defines the geometric normal
// (cross(v1 - v0, v2 - v0)), which is the side an emissive triangle emits from.
struct Triangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 n0, n1, n2;
    uint32_t material = 0;
};

struct Sphere {
    glm::vec3 center{0.0f};
    float radius = 1.0f;
    uint32_t material = 0;
};

// Plain CPU-side description of what to render. SceneCompiler turns it into GPU buffers.
struct Scene {
    std::string name;
    std::vector<Material> materials;
    std::vector<Triangle> triangles;
    std::vector<Sphere> spheres;

    Camera camera;          // initial viewpoint
    float moveSpeed = 2.0f; // world units per second for the camera controller
    glm::vec3 sky{0.0f};    // constant environment radiance

    uint32_t addMaterial(const Material& material) {
        materials.push_back(material);
        return static_cast<uint32_t>(materials.size() - 1);
    }
};

} // namespace pt
