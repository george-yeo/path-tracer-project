#include "core/Frame.hpp"

#include "core/Device.hpp"
#include "core/VkCheck.hpp"

namespace pt {

Frame::Frame(Device& device) : device(device) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = device.getQueueFamily();
    VK_CHECK(vkCreateCommandPool(device.get(), &poolInfo, nullptr, &commandPool));

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(device.get(), &allocInfo, &commandBuffer));

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK(vkCreateSemaphore(device.get(), &semaphoreInfo, nullptr, &imageAvailable));

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(device.get(), &fenceInfo, nullptr, &inFlight));

    VkQueryPoolCreateInfo queryInfo{};
    queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryInfo.queryCount = 2;
    VK_CHECK(vkCreateQueryPool(device.get(), &queryInfo, nullptr, &queryPool));
}

Frame::~Frame() {
    vkDestroyQueryPool(device.get(), queryPool, nullptr);
    vkDestroyFence(device.get(), inFlight, nullptr);
    vkDestroySemaphore(device.get(), imageAvailable, nullptr);
    vkDestroyCommandPool(device.get(), commandPool, nullptr); // frees the command buffer
}

void Frame::waitUntilAvailable() const {
    VK_CHECK(vkWaitForFences(device.get(), 1, &inFlight, VK_TRUE, UINT64_MAX));
}

void Frame::reset() {
    VK_CHECK(vkResetFences(device.get(), 1, &inFlight));
    VK_CHECK(vkResetCommandPool(device.get(), commandPool, 0));
}

} // namespace pt
