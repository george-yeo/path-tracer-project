#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace pt {

struct Aabb {
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{std::numeric_limits<float>::lowest()};

    void grow(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }
    void grow(const Aabb& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }
    bool isEmpty() const { return min.x > max.x; }
    glm::vec3 extent() const { return max - min; }
    float surfaceArea() const {
        if (isEmpty()) {
            return 0.0f;
        }
        glm::vec3 e = extent();
        return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
    }
};

// 32-byte node, uploaded to the GPU as-is (std430: vec3 + uint, vec3 + uint).
// Internal nodes: count == 0 and the two children are stored adjacently at
// leftOrFirst and leftOrFirst + 1. Leaves: primitives [leftOrFirst, leftOrFirst + count).
struct BvhNode {
    glm::vec3 boundsMin;
    uint32_t leftOrFirst;
    glm::vec3 boundsMax;
    uint32_t count;

    bool isLeaf() const { return count > 0; }
};
static_assert(sizeof(BvhNode) == 32, "BvhNode must match the GPU layout");

struct BvhBuildOptions {
    uint32_t binCount = 16;     // SAH bins per axis
    uint32_t maxLeafSize = 8;   // leaves larger than this are always split if possible
    float traversalCost = 1.0f; // SAH cost of visiting a node, relative to one intersection
};

struct BvhStats {
    uint32_t nodeCount = 0;
    uint32_t leafCount = 0;
    uint32_t maxDepth = 0;
    uint32_t maxLeafSize = 0;
    float sahCost = 0.0f; // expected cost of a random ray, in units of one intersection test
    double buildMs = 0.0;
};

struct Bvh {
    std::vector<BvhNode> nodes;           // nodes[0] is the root
    std::vector<uint32_t> primitiveOrder; // leaf ranges index into this permutation
    BvhStats stats;
};

// Top-down binned SAH build (Wald 2007) over arbitrary primitive bounds.
Bvh buildBvh(std::span<const Aabb> primitiveBounds, const BvhBuildOptions& options = {});

} // namespace pt
