// Shared by pathtrace.comp and resolve.comp. Mirrors PushConstants in renderer/GpuTypes.hpp.
#ifndef COMMON_GLSL
#define COMMON_GLSL

#define INTEGRATOR_MIS 0u
#define INTEGRATOR_BSDF 1u
#define INTEGRATOR_LIGHT 2u
#define INTEGRATOR_HEATMAP 3u

#define FLAG_ENCODE_SRGB 1u
#define FLAG_RAW_OUTPUT 2u

const float PI = 3.14159265358979323846;
const float INF = 1e30;

layout(push_constant) uniform PushConstants {
    vec4 cameraPosition; // w = tan(fovY / 2)
    vec4 cameraRight;    // w = aspect ratio
    vec4 cameraUp;
    vec4 cameraForward;
    vec4 sky;

    uint sampleIndex; // trace: first sample index of this dispatch
    uint sampleCount; // trace: samples in this dispatch; resolve: total accumulated
    uint maxBounces;
    uint integrator;

    uint sphereCount;
    uint lightCount;
    uint flags;
    float exposure;
} pc;

layout(binding = 0, rgba32f) uniform image2D accumulationImage; // running sum of samples
layout(binding = 1, rgba8) uniform writeonly image2D displayImage;

#endif
