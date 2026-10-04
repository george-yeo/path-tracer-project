#include "core/ComputePipeline.hpp"

#include "core/Device.hpp"
#include "core/VkCheck.hpp"
#include "utils/Paths.hpp"

namespace pt {

ComputePipeline::ComputePipeline(Device& device, const std::filesystem::path& spirvPath,
                                 VkDescriptorSetLayout setLayout, uint32_t pushConstantSize)
    : device(device) {
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.size = pushConstantSize;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &setLayout;
    layoutInfo.pushConstantRangeCount = pushConstantSize > 0 ? 1 : 0;
    layoutInfo.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(device.get(), &layoutInfo, nullptr, &layout));

    auto code = paths::readSpirv(spirvPath);
    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = code.size() * sizeof(uint32_t);
    moduleInfo.pCode = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device.get(), &moduleInfo, nullptr, &module));

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = module;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = layout;

    VkResult result = vkCreateComputePipelines(device.get(), VK_NULL_HANDLE, 1, &pipelineInfo,
                                               nullptr, &pipeline);
    vkDestroyShaderModule(device.get(), module, nullptr);
    VK_CHECK(result);
}

ComputePipeline::~ComputePipeline() {
    vkDestroyPipeline(device.get(), pipeline, nullptr);
    vkDestroyPipelineLayout(device.get(), layout, nullptr);
}

} // namespace pt
