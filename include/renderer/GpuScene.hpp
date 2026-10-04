#pragma once

#include "resources/Buffer.hpp"

#include <memory>

namespace pt {

class Device;
struct CompiledScene;

// Device-local storage buffers holding a CompiledScene, uploaded once through a staging buffer.
class GpuScene {
    public:
    GpuScene(Device& device, const CompiledScene& scene);

    const Buffer& getTriangles() const { return *triangles; }
    const Buffer& getShading() const { return *shading; }
    const Buffer& getBvhNodes() const { return *bvhNodes; }
    const Buffer& getMaterials() const { return *materials; }
    const Buffer& getSpheres() const { return *spheres; }
    const Buffer& getLights() const { return *lights; }

    uint32_t getSphereCount() const { return sphereCount; }
    uint32_t getLightCount() const { return lightCount; }

    private:
    std::unique_ptr<Buffer> triangles;
    std::unique_ptr<Buffer> shading;
    std::unique_ptr<Buffer> bvhNodes;
    std::unique_ptr<Buffer> materials;
    std::unique_ptr<Buffer> spheres;
    std::unique_ptr<Buffer> lights;
    uint32_t sphereCount = 0;
    uint32_t lightCount = 0;
};

} // namespace pt
