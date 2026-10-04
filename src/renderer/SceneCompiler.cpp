#include "renderer/SceneCompiler.hpp"

#include <format>
#include <numeric>
#include <stdexcept>

namespace pt {

namespace {

float luminance(const glm::vec3& c) {
    return glm::dot(c, glm::vec3(0.2126f, 0.7152f, 0.0722f));
}

float triangleArea(const Triangle& t) {
    return 0.5f * glm::length(glm::cross(t.v1 - t.v0, t.v2 - t.v0));
}

} // namespace

void buildAliasTable(std::span<const float> weights, std::span<GpuLight> lights) {
    const size_t n = weights.size();
    if (n == 0) {
        return;
    }

    double total = std::accumulate(weights.begin(), weights.end(), 0.0);
    std::vector<double> scaled(n);
    for (size_t i = 0; i < n; ++i) {
        double pmf = total > 0.0 ? weights[i] / total : 1.0 / static_cast<double>(n);
        lights[i].pmf = static_cast<float>(pmf);
        scaled[i] = pmf * static_cast<double>(n);
    }

    std::vector<uint32_t> small;
    std::vector<uint32_t> large;
    for (uint32_t i = 0; i < n; ++i) {
        (scaled[i] < 1.0 ? small : large).push_back(i);
    }

    while (!small.empty() && !large.empty()) {
        uint32_t s = small.back();
        small.pop_back();
        uint32_t l = large.back();
        large.pop_back();

        lights[s].aliasThreshold = static_cast<float>(scaled[s]);
        lights[s].alias = l;
        scaled[l] = (scaled[l] + scaled[s]) - 1.0;
        (scaled[l] < 1.0 ? small : large).push_back(l);
    }
    // Leftovers are exactly 1 up to rounding error.
    for (uint32_t i : small) {
        lights[i].aliasThreshold = 1.0f;
        lights[i].alias = i;
    }
    for (uint32_t i : large) {
        lights[i].aliasThreshold = 1.0f;
        lights[i].alias = i;
    }
}

CompiledScene compileScene(const Scene& scene) {
    CompiledScene out;

    const auto materialCount = static_cast<uint32_t>(scene.materials.size());
    auto checkMaterial = [&](uint32_t index, const char* what) {
        if (index >= materialCount) {
            throw std::runtime_error(std::format("{} uses material {} but the scene only has {}",
                                                 what, index, materialCount));
        }
    };

    for (const Material& m : scene.materials) {
        out.materials.push_back({m.color, m.roughness, m.color * m.emission, m.metallic});
    }
    for (const Sphere& s : scene.spheres) {
        checkMaterial(s.material, "A sphere");
        out.spheres.push_back({s.center, s.radius, s.material, {0, 0, 0}});
    }

    std::vector<Aabb> bounds;
    bounds.reserve(scene.triangles.size());
    for (const Triangle& t : scene.triangles) {
        checkMaterial(t.material, "A triangle");
        Aabb box;
        box.grow(t.v0);
        box.grow(t.v1);
        box.grow(t.v2);
        bounds.push_back(box);
    }

    Bvh bvh = buildBvh(bounds);
    if (bvh.stats.maxDepth >= kMaxBvhDepth) {
        throw std::runtime_error(
            std::format("BVH depth {} exceeds the shader's traversal stack ({})",
                        bvh.stats.maxDepth, kMaxBvhDepth));
    }
    out.bvhNodes = std::move(bvh.nodes);
    out.bvhStats = bvh.stats;

    std::vector<float> lightWeights;
    out.triangles.reserve(scene.triangles.size());
    out.shading.reserve(scene.triangles.size());
    for (uint32_t prim : bvh.primitiveOrder) {
        const Triangle& t = scene.triangles[prim];
        const auto index = static_cast<uint32_t>(out.triangles.size());

        out.triangles.push_back({t.v0, 0.0f, t.v1, 0.0f, t.v2, 0.0f});
        out.shading.push_back({t.n0, t.material, t.n1, kNoLight, t.n2, 0});

        const Material& material = scene.materials[t.material];
        float area = triangleArea(t);
        if (material.emission > 0.0f && area > 0.0f) {
            out.shading.back().lightIndex = static_cast<uint32_t>(out.lights.size());
            out.lights.push_back({index, 0.0f, 1.0f, 0});
            lightWeights.push_back(luminance(material.color * material.emission) * area);
        }
    }
    buildAliasTable(lightWeights, out.lights);
    return out;
}

} // namespace pt
