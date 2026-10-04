#include "renderer/GpuScene.hpp"

#include "core/Barriers.hpp"
#include "core/Device.hpp"
#include "renderer/SceneCompiler.hpp"

#include <vector>

namespace pt {

namespace {

struct Upload {
    std::unique_ptr<Buffer>* target;
    const void* data;
    VkDeviceSize size;
};

template <typename T> Upload upload(std::unique_ptr<Buffer>& target, const std::vector<T>& data) {
    return {&target, data.data(), sizeof(T) * data.size()};
}

} // namespace

GpuScene::GpuScene(Device& device, const CompiledScene& scene)
    : sphereCount(static_cast<uint32_t>(scene.spheres.size())),
      lightCount(static_cast<uint32_t>(scene.lights.size())) {
    const Upload uploads[] = {
        upload(triangles, scene.triangles), upload(shading, scene.shading),
        upload(bvhNodes, scene.bvhNodes),   upload(materials, scene.materials),
        upload(spheres, scene.spheres),     upload(lights, scene.lights),
    };

    VkDeviceSize stagingSize = 0;
    for (const Upload& u : uploads) {
        stagingSize += u.size;
    }
    Buffer staging(device, stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryUsage::Upload);

    VkDeviceSize offset = 0;
    std::vector<VkDeviceSize> offsets;
    for (const Upload& u : uploads) {
        *u.target = std::make_unique<Buffer>(
            device, u.size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            MemoryUsage::DeviceLocal);
        if (u.size > 0) {
            staging.write(u.data, u.size, offset);
        }
        offsets.push_back(offset);
        offset += u.size;
    }

    device.immediateSubmit([&](VkCommandBuffer cmd) {
        for (size_t i = 0; i < std::size(uploads); ++i) {
            if (uploads[i].size == 0) {
                continue;
            }
            VkBufferCopy region{offsets[i], 0, uploads[i].size};
            vkCmdCopyBuffer(cmd, staging.get(), (*uploads[i].target)->get(), 1, &region);
        }
        // Make the copies visible to every later submission that reads the scene.
        memoryBarrier(cmd, {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT},
                      {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT});
    });
}

} // namespace pt
