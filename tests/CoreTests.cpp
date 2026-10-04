// CPU unit tests for the scene pipeline: BVH construction and traversal, the light alias table,
// scene compilation and the camera basis. No GPU required.

#include "renderer/SceneCompiler.hpp"
#include "scene/Bvh.hpp"
#include "scene/Camera.hpp"
#include "scene/MeshLoader.hpp"
#include "scene/ScenePresets.hpp"
#include "utils/Paths.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

using pt::Aabb;
using pt::Bvh;
using pt::BvhNode;
using pt::Triangle;

std::vector<Triangle> randomTriangles(size_t count, std::mt19937& rng, bool clustered) {
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::vector<glm::vec3> centres = {{0, 0, 0}, {40, 5, -10}, {-25, 30, 15}, {10, -20, 35}};
    std::vector<Triangle> triangles;
    for (size_t i = 0; i < count; ++i) {
        glm::vec3 base = clustered ? centres[i % centres.size()] +
                                         glm::vec3(unit(rng), unit(rng), unit(rng)) * 6.0f
                                   : glm::vec3(unit(rng), unit(rng), unit(rng)) * 100.0f;
        auto jitter = [&] {
            return glm::vec3(unit(rng), unit(rng), unit(rng)) - 0.5f;
        };
        Triangle t{};
        t.v0 = base;
        t.v1 = base + jitter() * 2.0f;
        t.v2 = base + jitter() * 2.0f;
        triangles.push_back(t);
    }
    return triangles;
}

std::vector<Aabb> boundsOf(const std::vector<Triangle>& triangles) {
    std::vector<Aabb> bounds;
    for (const Triangle& t : triangles) {
        Aabb box;
        box.grow(t.v0);
        box.grow(t.v1);
        box.grow(t.v2);
        bounds.push_back(box);
    }
    return bounds;
}

bool contains(const BvhNode& node, const Aabb& box) {
    return glm::all(glm::lessThanEqual(node.boundsMin, box.min)) &&
           glm::all(glm::greaterThanEqual(node.boundsMax, box.max));
}

// Structural invariants: a permutation of all primitives, adjacent children, tight containment.
void checkBvhInvariants(const Bvh& bvh, const std::vector<Aabb>& bounds) {
    const size_t n = bounds.size();
    std::vector<uint32_t> order = bvh.primitiveOrder;
    std::sort(order.begin(), order.end());
    std::vector<uint32_t> expected(n);
    std::iota(expected.begin(), expected.end(), 0u);
    CHECK(order == expected);

    std::vector<int> covered(n, 0);
    std::vector<int> visited(bvh.nodes.size(), 0);
    std::function<void(uint32_t)> visit = [&](uint32_t index) {
        CHECK(index < bvh.nodes.size());
        ++visited[index];
        const BvhNode& node = bvh.nodes[index];
        if (node.isLeaf()) {
            for (uint32_t i = node.leftOrFirst; i < node.leftOrFirst + node.count; ++i) {
                ++covered[i];
                CHECK(contains(node, bounds[bvh.primitiveOrder[i]]));
            }
            return;
        }
        uint32_t left = node.leftOrFirst;
        CHECK(left + 1 < bvh.nodes.size());
        for (uint32_t child : {left, left + 1}) {
            Aabb childBox{bvh.nodes[child].boundsMin, bvh.nodes[child].boundsMax};
            CHECK(contains(node, childBox));
            visit(child);
        }
    };
    if (n > 0) {
        visit(0);
        CHECK(std::all_of(covered.begin(), covered.end(), [](int c) { return c == 1; }));
        CHECK(std::all_of(visited.begin(), visited.end(), [](int v) { return v == 1; }));
    }
}

bool intersectTriangle(const glm::vec3& o, const glm::vec3& d, const Triangle& tri, float tMax,
                       float& t) {
    glm::vec3 e1 = tri.v1 - tri.v0;
    glm::vec3 e2 = tri.v2 - tri.v0;
    glm::vec3 p = glm::cross(d, e2);
    float det = glm::dot(e1, p);
    if (det == 0.0f) return false;
    float inv = 1.0f / det;
    glm::vec3 s = o - tri.v0;
    float u = glm::dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(s, e1);
    float v = glm::dot(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = glm::dot(e2, q) * inv;
    return t > 0.0f && t < tMax;
}

float boxEntry(const BvhNode& node, const glm::vec3& o, const glm::vec3& invDir, float tMax) {
    glm::vec3 t0 = (node.boundsMin - o) * invDir;
    glm::vec3 t1 = (node.boundsMax - o) * invDir;
    glm::vec3 tn = glm::min(t0, t1);
    glm::vec3 tf = glm::max(t0, t1);
    float enter = std::max({tn.x, tn.y, tn.z, 0.0f});
    float exit = std::min({tf.x, tf.y, tf.z, tMax});
    return enter <= exit ? enter : std::numeric_limits<float>::infinity();
}

// Same front-to-back algorithm as traverseBvh() in shaders/intersect.glsl.
float closestHitBvh(const Bvh& bvh, const std::vector<Triangle>& triangles, glm::vec3 o,
                    glm::vec3 d) {
    constexpr float inf = std::numeric_limits<float>::infinity();
    float tMax = inf;
    glm::vec3 invDir = 1.0f / d;
    if (boxEntry(bvh.nodes[0], o, invDir, tMax) == inf) return inf;

    std::vector<std::pair<uint32_t, float>> stack;
    uint32_t current = 0;
    for (;;) {
        const BvhNode& node = bvh.nodes[current];
        if (node.isLeaf()) {
            for (uint32_t i = node.leftOrFirst; i < node.leftOrFirst + node.count; ++i) {
                float t;
                if (intersectTriangle(o, d, triangles[bvh.primitiveOrder[i]], tMax, t)) tMax = t;
            }
        } else {
            uint32_t a = node.leftOrFirst;
            uint32_t b = a + 1;
            float ta = boxEntry(bvh.nodes[a], o, invDir, tMax);
            float tb = boxEntry(bvh.nodes[b], o, invDir, tMax);
            if (tb < ta) {
                std::swap(a, b);
                std::swap(ta, tb);
            }
            if (ta < inf) {
                if (tb < inf) stack.emplace_back(b, tb);
                current = a;
                continue;
            }
        }
        bool popped = false;
        while (!stack.empty()) {
            auto [index, entry] = stack.back();
            stack.pop_back();
            if (entry < tMax) {
                current = index;
                popped = true;
                break;
            }
        }
        if (!popped) break;
    }
    return tMax;
}

float closestHitBruteForce(const std::vector<Triangle>& triangles, glm::vec3 o, glm::vec3 d) {
    float tMax = std::numeric_limits<float>::infinity();
    for (const Triangle& tri : triangles) {
        float t;
        if (intersectTriangle(o, d, tri, tMax, t)) tMax = t;
    }
    return tMax;
}

// Baseline for comparison: object-median split on the longest axis (the original builder).
float medianSplitSahCost(const std::vector<Aabb>& bounds) {
    std::vector<uint32_t> order(bounds.size());
    std::iota(order.begin(), order.end(), 0u);
    float rootArea = 0.0f;
    float cost = 0.0f;
    std::function<void(size_t, size_t)> build = [&](size_t begin, size_t end) {
        Aabb box;
        for (size_t i = begin; i < end; ++i)
            box.grow(bounds[order[i]]);
        if (rootArea == 0.0f) rootArea = box.surfaceArea();
        float relative = box.surfaceArea() / rootArea;
        if (end - begin <= 4) {
            cost += relative * static_cast<float>(end - begin);
            return;
        }
        cost += relative;
        glm::vec3 e = box.extent();
        int axis = 2;
        if (e.x >= e.y && e.x >= e.z) {
            axis = 0;
        } else if (e.y >= e.z) {
            axis = 1;
        }
        size_t mid = begin + (end - begin) / 2;
        std::nth_element(order.begin() + static_cast<long>(begin),
                         order.begin() + static_cast<long>(mid),
                         order.begin() + static_cast<long>(end), [&](uint32_t a, uint32_t b) {
                             return bounds[a].min[axis] + bounds[a].max[axis] <
                                    bounds[b].min[axis] + bounds[b].max[axis];
                         });
        build(begin, mid);
        build(mid, end);
    };
    build(0, bounds.size());
    return cost;
}

void testBvhStructure() {
    std::printf("BVH structure\n");
    std::mt19937 rng(1234);
    for (bool clustered : {false, true}) {
        for (size_t count : {1u, 2u, 3u, 17u, 1000u, 20000u}) {
            auto triangles = randomTriangles(count, rng, clustered);
            auto bounds = boundsOf(triangles);
            Bvh bvh = pt::buildBvh(bounds);
            checkBvhInvariants(bvh, bounds);
            CHECK(bvh.stats.maxDepth < 64);
        }
    }

    // Degenerate input: identical primitives cannot be separated by any plane.
    std::vector<Aabb> identical(50, Aabb{glm::vec3(1.0f), glm::vec3(2.0f)});
    Bvh same = pt::buildBvh(identical);
    checkBvhInvariants(same, identical);

    // Empty scene: a single root that every ray misses.
    Bvh empty = pt::buildBvh({});
    CHECK(empty.nodes.size() == 1);
    CHECK(empty.nodes[0].count == 0);
    CHECK(empty.nodes[0].boundsMin.x > empty.nodes[0].boundsMax.x);
}

void testBvhTraversal() {
    std::printf("BVH traversal matches brute force\n");
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    auto triangles = randomTriangles(4000, rng, true);
    Bvh bvh = pt::buildBvh(boundsOf(triangles));

    int mismatches = 0;
    int hits = 0;
    for (int i = 0; i < 5000; ++i) {
        glm::vec3 origin = glm::vec3(unit(rng), unit(rng), unit(rng)) * 80.0f;
        // Aim near a random triangle so most rays hit something; some still miss past edges.
        const Triangle& aim = triangles[static_cast<size_t>(i) % triangles.size()];
        glm::vec3 target =
            (aim.v0 + aim.v1 + aim.v2) / 3.0f + glm::vec3(unit(rng), unit(rng), unit(rng));
        glm::vec3 dir = glm::normalize(target - origin);
        float a = closestHitBvh(bvh, triangles, origin, dir);
        float b = closestHitBruteForce(triangles, origin, dir);
        hits += std::isfinite(b) ? 1 : 0;
        if (a != b) ++mismatches;
    }
    std::printf("  %d / 5000 rays hit, %d mismatches\n", hits, mismatches);
    CHECK(mismatches == 0);
    CHECK(hits > 500);
}

void testSahBeatsMedianSplit() {
    std::printf("Binned SAH vs median split (lower is better)\n");
    std::mt19937 rng(7);
    for (bool clustered : {false, true}) {
        auto bounds = boundsOf(randomTriangles(20000, rng, clustered));
        float sah = pt::buildBvh(bounds).stats.sahCost;
        float median = medianSplitSahCost(bounds);
        std::printf("  %-9s SAH cost %.2f vs median split %.2f\n",
                    clustered ? "clustered" : "uniform", sah, median);
        CHECK(sah <= median);
    }
}

void testAliasTable() {
    std::printf("Alias table reproduces the target distribution\n");
    std::mt19937 rng(99);
    std::uniform_real_distribution<float> unit(0.0f, 10.0f);
    for (size_t n : {1u, 2u, 7u, 64u, 1000u}) {
        std::vector<float> weights(n);
        for (float& w : weights)
            w = unit(rng);
        if (n > 2) weights[1] = 0.0f; // zero-power entries must never be chosen

        std::vector<pt::GpuLight> lights(n);
        pt::buildAliasTable(weights, lights);

        // Exact probability of returning i: own slot kept + every slot aliasing to i.
        std::vector<double> probability(n, 0.0);
        for (size_t i = 0; i < n; ++i) {
            probability[i] += lights[i].aliasThreshold / static_cast<double>(n);
            probability[lights[i].alias] +=
                (1.0 - lights[i].aliasThreshold) / static_cast<double>(n);
        }
        double total = std::accumulate(weights.begin(), weights.end(), 0.0);
        double pmfSum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            CHECK(std::abs(probability[i] - weights[i] / total) < 1e-5);
            CHECK(std::abs(lights[i].pmf - weights[i] / total) < 1e-5);
            pmfSum += lights[i].pmf;
        }
        CHECK(std::abs(pmfSum - 1.0) < 1e-4);
    }
}

void testCornellCompile() {
    std::printf("Cornell box compiles\n");
    const pt::ScenePreset* preset = pt::findScenePreset("cornell");
    CHECK(preset != nullptr);
    if (preset == nullptr) return;

    pt::Scene scene = preset->build();
    pt::CompiledScene compiled = pt::compileScene(scene);
    CHECK(compiled.triangles.size() == 32);
    CHECK(compiled.shading.size() == 32);
    CHECK(compiled.materials.size() == 4);
    CHECK(compiled.lights.size() == 2);

    float pmfSum = 0.0f;
    for (uint32_t i = 0; i < compiled.lights.size(); ++i) {
        const pt::GpuLight& light = compiled.lights[i];
        pmfSum += light.pmf;
        CHECK(compiled.shading[light.triangle].lightIndex == i);
        const pt::GpuMaterial& material =
            compiled.materials[compiled.shading[light.triangle].material];
        CHECK(material.emission.x > 0.0f);
    }
    CHECK(std::abs(pmfSum - 1.0f) < 1e-5f);

    size_t lightTagged = std::count_if(compiled.shading.begin(), compiled.shading.end(),
                                       [](const auto& s) { return s.lightIndex != pt::kNoLight; });
    CHECK(lightTagged == 2);

    pt::Scene broken = scene;
    broken.triangles[0].material = 99;
    bool threw = false;
    try {
        pt::compileScene(broken);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

void testCameraBasis() {
    std::printf("Camera basis matches the original quaternion camera\n");
    pt::Camera camera{.position = {}, .yawDeg = -15.0f, .pitchDeg = -20.0f};
    glm::vec3 f = camera.forward();
    glm::vec3 r = camera.right();
    glm::vec3 u = camera.up();
    // Reference values computed from the old glm::quat(eulerAngles) implementation.
    CHECK(glm::length(f - glm::vec3(-0.243f, -0.342f, -0.908f)) < 2e-3f);
    CHECK(glm::length(r - glm::vec3(0.966f, 0.0f, -0.259f)) < 2e-3f);
    CHECK(glm::length(u - glm::vec3(-0.089f, 0.940f, -0.330f)) < 2e-3f);
    CHECK(std::abs(glm::dot(f, r)) < 1e-5f);
    CHECK(std::abs(glm::dot(f, u)) < 1e-5f);
}

void reportMeshScenes() {
    std::printf("Mesh scenes (skipped if assets are missing)\n");
    for (const char* id : {"teapot", "dragon"}) {
        const pt::ScenePreset* preset = pt::findScenePreset(id);
        try {
            pt::Scene scene = preset->build();
            pt::CompiledScene compiled = pt::compileScene(scene);
            const pt::BvhStats& s = compiled.bvhStats;
            std::printf(
                "  %-7s %7zu tris  BVH %6u nodes  depth %2u  max leaf %u  SAH %.1f  %.0f ms\n", id,
                compiled.triangles.size(), s.nodeCount, s.maxDepth, s.maxLeafSize, s.sahCost,
                s.buildMs);
            CHECK(s.maxDepth < 64);
            CHECK(compiled.lights.size() == 2);
        } catch (const std::exception& e) {
            std::printf("  %-7s skipped: %s\n", id, e.what());
        }
    }
}

} // namespace

int main() {
    testBvhStructure();
    testBvhTraversal();
    testSahBeatsMedianSplit();
    testAliasTable();
    testCornellCompile();
    testCameraBasis();
    reportMeshScenes();

    if (failures > 0) {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("\nAll tests passed\n");
    return 0;
}
