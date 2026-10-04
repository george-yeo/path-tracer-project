// Scene storage buffers. Mirrors the Gpu* structs in renderer/GpuTypes.hpp (std430).
#ifndef SCENE_GLSL
#define SCENE_GLSL

#define NO_LIGHT 0xFFFFFFFFu

struct Triangle {
    vec3 v0;
    float pad0;
    vec3 v1;
    float pad1;
    vec3 v2;
    float pad2;
};

struct TriangleShading {
    vec3 n0;
    uint material;
    vec3 n1;
    uint lightIndex;
    vec3 n2;
    uint pad;
};

struct BvhNode {
    vec3 boundsMin;
    uint leftOrFirst; // internal: left child (right = left + 1); leaf: first triangle
    vec3 boundsMax;
    uint count;       // 0 for internal nodes
};

struct Material {
    vec3 color;
    float roughness;
    vec3 emission;
    float metallic;
};

struct Sphere {
    vec3 center;
    float radius;
    uint material;
    uint pad0;
    uint pad1;
    uint pad2;
};

struct Light {
    uint triangle;
    float pmf;
    float aliasThreshold;
    uint alias;
};

layout(binding = 2, std430) readonly buffer Triangles { Triangle triangles[]; };
layout(binding = 3, std430) readonly buffer Shading { TriangleShading shading[]; };
layout(binding = 4, std430) readonly buffer BvhNodes { BvhNode bvhNodes[]; };
layout(binding = 5, std430) readonly buffer Materials { Material materials[]; };
layout(binding = 6, std430) readonly buffer Spheres { Sphere spheres[]; };
layout(binding = 7, std430) readonly buffer Lights { Light lights[]; };

#endif
