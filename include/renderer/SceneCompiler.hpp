#pragma once

#include "renderer/GpuTypes.hpp"
#include "scene/Bvh.hpp"
#include "scene/Scene.hpp"

#include <span>
#include <vector>

namespace pt {

// GPU-ready arrays for a Scene. Pure CPU: no Vulkan here, so it is unit-testable.
struct CompiledScene {
    std::vector<GpuTriangle> triangles; // in BVH leaf order
    std::vector<GpuTriangleShading> shading;
    std::vector<BvhNode> bvhNodes;
    std::vector<GpuMaterial> materials;
    std::vector<GpuSphere> spheres;
    std::vector<GpuLight> lights;
    BvhStats bvhStats;
};

// Builds the BVH, reorders triangles into leaf order, and builds the emissive-triangle light
// table (power-proportional alias table). Throws on invalid material indices.
CompiledScene compileScene(const Scene& scene);

// Fills pmf / aliasThreshold / alias for `lights` from non-negative `weights` (Vose's method).
void buildAliasTable(std::span<const float> weights, std::span<GpuLight> lights);

} // namespace pt
