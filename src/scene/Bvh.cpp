#include "scene/Bvh.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>

namespace pt {

namespace {

struct BuildTask {
    uint32_t node;
    uint32_t begin;
    uint32_t end;
    uint32_t depth;
};

struct Split {
    int axis = -1;
    uint32_t bin = 0; // primitives in bins [0, bin) go left
    float cost = std::numeric_limits<float>::max();
};

struct Bin {
    Aabb bounds;
    uint32_t count = 0;
};

class Builder {
    public:
    Builder(std::span<const Aabb> bounds, const BvhBuildOptions& options, Bvh& out)
        : bounds(bounds), options(options), bins(options.binCount), rightAreas(options.binCount),
          rightCounts(options.binCount), order(&out.primitiveOrder), nodes(&out.nodes),
          stats(&out.stats) {
        centroids.reserve(bounds.size());
        for (const Aabb& box : bounds) {
            centroids.push_back((box.min + box.max) * 0.5f);
        }
    }

    void build() {
        auto primitiveCount = static_cast<uint32_t>(bounds.size());
        order->resize(primitiveCount);
        std::iota(order->begin(), order->end(), 0u);

        nodes->reserve(primitiveCount > 0 ? 2 * primitiveCount - 1 : 1);
        nodes->push_back({});

        std::vector<BuildTask> stack;
        stack.push_back({0, 0, primitiveCount, 0});
        while (!stack.empty()) {
            BuildTask task = stack.back();
            stack.pop_back();
            processNode(task, stack);
        }
    }

    private:
    std::span<const Aabb> bounds;
    const BvhBuildOptions& options;
    std::vector<glm::vec3> centroids;
    std::vector<Bin> bins;
    std::vector<float> rightAreas;
    std::vector<uint32_t> rightCounts;
    std::vector<uint32_t>* order;
    std::vector<BvhNode>* nodes;
    BvhStats* stats;

    void processNode(const BuildTask& task, std::vector<BuildTask>& stack) {
        Aabb nodeBounds;
        Aabb centroidBounds;
        for (uint32_t i = task.begin; i < task.end; ++i) {
            uint32_t prim = (*order)[i];
            nodeBounds.grow(bounds[prim]);
            centroidBounds.grow(centroids[prim]);
        }

        BvhNode& node = (*nodes)[task.node];
        node.boundsMin = nodeBounds.isEmpty() ? glm::vec3(0.0f) : nodeBounds.min;
        node.boundsMax = nodeBounds.isEmpty() ? glm::vec3(0.0f) : nodeBounds.max;
        stats->maxDepth = std::max(stats->maxDepth, task.depth);

        const uint32_t count = task.end - task.begin;
        const auto leafCost = static_cast<float>(count);
        Split split = count > 1 ? findBestSplit(task, nodeBounds, centroidBounds) : Split{};

        bool makeLeaf = split.axis < 0 || (split.cost >= leafCost && count <= options.maxLeafSize);
        uint32_t mid = 0;
        if (!makeLeaf) {
            mid = partition(task, split, centroidBounds);
            makeLeaf = (mid == task.begin || mid == task.end);
        }

        if (makeLeaf) {
            node.leftOrFirst = task.begin;
            node.count = count;
            if (count == 0) {
                // Only possible for an empty scene: give the root inverted bounds so every ray
                // misses it (traversal always tests the root box before reading children).
                node.boundsMin = glm::vec3(1.0f);
                node.boundsMax = glm::vec3(-1.0f);
            }
            return;
        }

        auto left = static_cast<uint32_t>(nodes->size());
        nodes->push_back({});
        nodes->push_back({});
        BvhNode& parent = (*nodes)[task.node]; // re-fetch: push_back may reallocate
        parent.leftOrFirst = left;
        parent.count = 0;

        stack.push_back({left + 1, mid, task.end, task.depth + 1});
        stack.push_back({left, task.begin, mid, task.depth + 1});
    }

    uint32_t binIndex(const glm::vec3& centroid, int axis, const Aabb& centroidBounds) const {
        float extent = centroidBounds.max[axis] - centroidBounds.min[axis];
        float t = (centroid[axis] - centroidBounds.min[axis]) / extent;
        const auto index = static_cast<uint32_t>(t * static_cast<float>(options.binCount));
        return std::min(index, options.binCount - 1);
    }

    // SAH: cost = C_trav + (A_L * N_L + A_R * N_R) / A_parent, in units of one intersection.
    Split findBestSplit(const BuildTask& task, const Aabb& nodeBounds, const Aabb& centroidBounds) {
        Split best;
        const float parentArea = nodeBounds.surfaceArea();
        if (parentArea <= 0.0f) {
            return best;
        }

        for (int axis = 0; axis < 3; ++axis) {
            if (centroidBounds.max[axis] <= centroidBounds.min[axis]) {
                continue; // all centroids coincide on this axis
            }

            std::fill(bins.begin(), bins.end(), Bin{});
            for (uint32_t i = task.begin; i < task.end; ++i) {
                uint32_t prim = (*order)[i];
                Bin& bin = bins[binIndex(centroids[prim], axis, centroidBounds)];
                bin.bounds.grow(bounds[prim]);
                ++bin.count;
            }

            // Sweep right-to-left to get the area/count of everything right of each plane...
            Aabb accumulated;
            uint32_t accumulatedCount = 0;
            for (uint32_t b = options.binCount - 1; b > 0; --b) {
                accumulated.grow(bins[b].bounds);
                accumulatedCount += bins[b].count;
                rightAreas[b] = accumulated.surfaceArea();
                rightCounts[b] = accumulatedCount;
            }

            // ...then left-to-right, evaluating the plane between bins b-1 and b.
            accumulated = Aabb{};
            accumulatedCount = 0;
            for (uint32_t b = 1; b < options.binCount; ++b) {
                accumulated.grow(bins[b - 1].bounds);
                accumulatedCount += bins[b - 1].count;
                if (accumulatedCount == 0 || rightCounts[b] == 0) {
                    continue;
                }
                float cost = options.traversalCost +
                             (accumulated.surfaceArea() * static_cast<float>(accumulatedCount) +
                              rightAreas[b] * static_cast<float>(rightCounts[b])) /
                                 parentArea;
                if (cost < best.cost) {
                    best = {axis, b, cost};
                }
            }
        }
        return best;
    }

    uint32_t partition(const BuildTask& task, const Split& split, const Aabb& centroidBounds) {
        auto first = order->begin() + task.begin;
        auto last = order->begin() + task.end;
        auto middle = std::partition(first, last, [&](uint32_t prim) {
            return binIndex(centroids[prim], split.axis, centroidBounds) < split.bin;
        });
        return static_cast<uint32_t>(middle - order->begin());
    }
};

// Expected traversal cost of the finished tree, normalised by root area (standard SAH metric).
void computeStats(Bvh& bvh, const BvhBuildOptions& options) {
    BvhStats& stats = bvh.stats;
    stats.nodeCount = static_cast<uint32_t>(bvh.nodes.size());
    if (bvh.nodes.empty()) {
        return;
    }

    auto area = [](const BvhNode& node) {
        Aabb box{node.boundsMin, node.boundsMax};
        return box.surfaceArea();
    };
    float rootArea = area(bvh.nodes[0]);
    float cost = 0.0f;
    for (const BvhNode& node : bvh.nodes) {
        float relativeArea = rootArea > 0.0f ? area(node) / rootArea : 0.0f;
        if (node.isLeaf()) {
            ++stats.leafCount;
            stats.maxLeafSize = std::max(stats.maxLeafSize, node.count);
            cost += relativeArea * static_cast<float>(node.count);
        } else {
            cost += relativeArea * options.traversalCost;
        }
    }
    stats.sahCost = cost;
}

} // namespace

Bvh buildBvh(std::span<const Aabb> primitiveBounds, const BvhBuildOptions& options) {
    auto start = std::chrono::steady_clock::now();

    Bvh bvh;
    Builder(primitiveBounds, options, bvh).build();
    computeStats(bvh, options);

    bvh.stats.buildMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return bvh;
}

} // namespace pt
