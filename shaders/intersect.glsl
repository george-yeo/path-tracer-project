// Ray/primitive tests and BVH traversal. Requires scene.glsl.
#ifndef INTERSECT_GLSL
#define INTERSECT_GLSL

#define BVH_STACK_SIZE 64 // SceneCompiler rejects BVHs deeper than this (kMaxBvhDepth)

struct Ray {
    vec3 origin;
    vec3 direction; // normalised
};

uint bvhNodesVisited; // for the heatmap debug view

// Möller-Trumbore. No epsilon on the determinant beyond exact zero: the old 1e-4 threshold
// silently dropped small or grazing triangles (holes in dense meshes such as the dragon).
bool intersectTriangle(Ray ray, Triangle tri, float tMax, out float t, out vec2 uv) {
    vec3 e1 = tri.v1 - tri.v0;
    vec3 e2 = tri.v2 - tri.v0;
    vec3 p = cross(ray.direction, e2);
    float det = dot(e1, p);
    if (det == 0.0) {
        return false;
    }
    float invDet = 1.0 / det;
    vec3 s = ray.origin - tri.v0;
    float u = dot(s, p) * invDet;
    if (u < 0.0 || u > 1.0) {
        return false;
    }
    vec3 q = cross(s, e1);
    float v = dot(ray.direction, q) * invDet;
    if (v < 0.0 || u + v > 1.0) {
        return false;
    }
    t = dot(e2, q) * invDet;
    uv = vec2(u, v);
    return t > 0.0 && t < tMax;
}

// Numerically stable form (Ray Tracing Gems, ch. 7) - matters for the r = 4000 ground sphere.
bool intersectSphere(Ray ray, Sphere sphere, float tMax, out float t) {
    vec3 f = ray.origin - sphere.center;
    float b = -dot(f, ray.direction);
    vec3 l = f + b * ray.direction;
    float r2 = sphere.radius * sphere.radius;
    float discriminant = r2 - dot(l, l);
    if (discriminant < 0.0) {
        return false;
    }
    float c = dot(f, f) - r2;
    float q = b + (b >= 0.0 ? 1.0 : -1.0) * sqrt(discriminant);
    float t0 = q != 0.0 ? c / q : 0.0;
    float t1 = q;
    if (t0 > t1) {
        float tmp = t0;
        t0 = t1;
        t1 = tmp;
    }
    t = t0 > 0.0 ? t0 : t1; // second root when the origin is inside
    return t > 0.0 && t < tMax;
}

vec3 safeInverse(vec3 d) {
    const float tiny = 1e-20;
    return 1.0 / vec3(abs(d.x) > tiny ? d.x : (d.x >= 0.0 ? tiny : -tiny),
                      abs(d.y) > tiny ? d.y : (d.y >= 0.0 ? tiny : -tiny),
                      abs(d.z) > tiny ? d.z : (d.z >= 0.0 ? tiny : -tiny));
}

// Entry distance into the box, or INF if the ray misses it within [0, tMax).
float boxEntry(vec3 boundsMin, vec3 boundsMax, vec3 origin, vec3 invDir, float tMax) {
    vec3 t0 = (boundsMin - origin) * invDir;
    vec3 t1 = (boundsMax - origin) * invDir;
    vec3 tNear = min(t0, t1);
    vec3 tFar = max(t0, t1);
    float enter = max(max(tNear.x, tNear.y), max(tNear.z, 0.0));
    float exit = min(min(tFar.x, tFar.y), min(tFar.z, tMax));
    return enter <= exit ? enter : INF;
}

float nodeEntry(uint index, vec3 origin, vec3 invDir, float tMax) {
    BvhNode node = bvhNodes[index];
    return boxEntry(node.boundsMin, node.boundsMax, origin, invDir, tMax);
}

// Front-to-back traversal: the nearer child is visited first and the farther one is pushed with
// its entry distance, so subtrees behind the current closest hit are skipped when popped.
// With anyHit = true it returns at the first intersection (shadow rays).
bool traverseBvh(Ray ray, bool anyHit, inout float tMax, out uint hitTriangle, out vec2 hitUv) {
    hitTriangle = 0u;
    hitUv = vec2(0.0);
    vec3 invDir = safeInverse(ray.direction);
    if (nodeEntry(0u, ray.origin, invDir, tMax) >= INF) {
        return false; // also covers an empty scene (root has inverted bounds)
    }

    uint stackNode[BVH_STACK_SIZE];
    float stackEntry[BVH_STACK_SIZE];
    int sp = 0;
    uint current = 0u;
    bool found = false;

    while (true) {
        BvhNode node = bvhNodes[current];
        ++bvhNodesVisited;

        if (node.count > 0u) {
            for (uint i = 0u; i < node.count; ++i) {
                uint index = node.leftOrFirst + i;
                float t;
                vec2 uv;
                if (intersectTriangle(ray, triangles[index], tMax, t, uv)) {
                    tMax = t;
                    hitTriangle = index;
                    hitUv = uv;
                    found = true;
                    if (anyHit) {
                        return true;
                    }
                }
            }
        } else {
            uint near = node.leftOrFirst;
            uint far = near + 1u;
            float tNear = nodeEntry(near, ray.origin, invDir, tMax);
            float tFar = nodeEntry(far, ray.origin, invDir, tMax);
            if (tFar < tNear) {
                uint tmpNode = near;
                near = far;
                far = tmpNode;
                float tmpT = tNear;
                tNear = tFar;
                tFar = tmpT;
            }
            if (tNear < INF) {
                if (tFar < INF) {
                    stackNode[sp] = far;
                    stackEntry[sp] = tFar;
                    ++sp;
                }
                current = near;
                continue;
            }
        }

        // Pop the next subtree that can still contain a closer hit.
        bool popped = false;
        while (sp > 0) {
            --sp;
            if (stackEntry[sp] < tMax) {
                current = stackNode[sp];
                popped = true;
                break;
            }
        }
        if (!popped) {
            break;
        }
    }
    return found;
}

bool occluded(Ray ray, float tMax) {
    for (uint i = 0u; i < pc.sphereCount; ++i) {
        float t;
        if (intersectSphere(ray, spheres[i], tMax, t)) {
            return true;
        }
    }
    uint triangle;
    vec2 uv;
    return traverseBvh(ray, true, tMax, triangle, uv);
}

#endif
