#pragma once

// CPU mirrors of the structures declared in shaders/common.glsl and shaders/scene.glsl.
// Layouts follow std430: a vec3 followed by a 4-byte scalar packs into one 16-byte slot, which
// is how every struct below avoids padding-only members. Keep both sides in sync; the
// static_asserts catch size drift on the C++ side.

#include "scene/Bvh.hpp"

#include <glm/glm.hpp>

#include <cstdint>

namespace pt {

constexpr uint32_t kNoLight = 0xFFFFFFFFu;
constexpr uint32_t kMaxBvhDepth = 64; // traversal stack size in shaders/intersect.glsl

// Hot data for intersection only: positions (48 B).
struct GpuTriangle {
    glm::vec3 v0;
    float pad0;
    glm::vec3 v1;
    float pad1;
    glm::vec3 v2;
    float pad2;
};
static_assert(sizeof(GpuTriangle) == 48);

// Cold data, fetched once per hit: shading normals, material, light-table index (48 B).
struct GpuTriangleShading {
    glm::vec3 n0;
    uint32_t material;
    glm::vec3 n1;
    uint32_t lightIndex; // index into the light table, or kNoLight
    glm::vec3 n2;
    uint32_t pad;
};
static_assert(sizeof(GpuTriangleShading) == 48);

struct GpuMaterial {
    glm::vec3 color;
    float roughness;
    glm::vec3 emission; // radiance = color * strength, precomputed
    float metallic;
};
static_assert(sizeof(GpuMaterial) == 32);

struct GpuSphere {
    glm::vec3 center;
    float radius;
    uint32_t material;
    uint32_t pad[3];
};
static_assert(sizeof(GpuSphere) == 32);

// Emissive triangle with its slot in a Vose alias table (O(1) power-proportional sampling).
struct GpuLight {
    uint32_t triangle;    // index into the (BVH-ordered) triangle arrays
    float pmf;            // probability of choosing this light
    float aliasThreshold; // keep this slot if u < threshold, otherwise take `alias`
    uint32_t alias;
};
static_assert(sizeof(GpuLight) == 16);

static_assert(sizeof(BvhNode) == 32);

// Push constants shared by pathtrace.comp and resolve.comp (112 B, within the 128 B minimum).
struct PushConstants {
    glm::vec4 cameraPosition; // w = tan(fovY / 2)
    glm::vec4 cameraRight;    // w = aspect ratio
    glm::vec4 cameraUp;
    glm::vec4 cameraForward;
    glm::vec4 sky; // rgb = constant environment radiance

    uint32_t sampleIndex; // first sample index of this dispatch (seeds the RNG)
    uint32_t sampleCount; // trace: samples per pixel in this dispatch; resolve: total accumulated
    uint32_t maxBounces;
    uint32_t integrator;

    uint32_t sphereCount;
    uint32_t lightCount;
    uint32_t flags;
    float exposure;
};
static_assert(sizeof(PushConstants) == 112);

constexpr uint32_t kFlagEncodeSrgb = 1u << 0; // resolve applies the sRGB transfer function
constexpr uint32_t kFlagRawOutput = 1u << 1;  // resolve skips exposure and tone mapping

} // namespace pt
