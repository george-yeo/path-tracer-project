#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace pt {

enum class Integrator : uint32_t {
    Mis = 0,        // BSDF sampling + next-event estimation, combined with the power heuristic
    BsdfOnly = 1,   // unidirectional path tracing, lights only found by chance
    LightOnly = 2,  // next-event estimation only (emitters hit by BSDF rays are ignored)
    BvhHeatmap = 3, // debug view: BVH nodes visited by the primary ray
};

// All three estimators converge to the same image; comparing them is a direct test of the MIS
// weights and pdfs. The heatmap is a visualisation, not an estimator.
constexpr const char* kIntegratorNames[] = {
    "MIS",
    "BSDF sampling only",
    "Light sampling only",
    "BVH heatmap",
};

struct RenderSettings {
    uint32_t maxBounces = 8;
    uint32_t targetSamples = 1024;
    uint32_t samplesPerFrame = 1;
    Integrator integrator = Integrator::Mis;
    float exposure = 1.0f;
    glm::vec3 sky{0.0f};
};

} // namespace pt
