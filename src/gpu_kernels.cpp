#include "nextviper/gpu_kernels.hpp"
#include <iostream>
#include <stdexcept>

namespace nextviper {

VulkanComputePipeline::~VulkanComputePipeline() {
    // Pipeline cleanup is managed via VulkanKernelManager::cleanup
}

VulkanKernelManager::VulkanKernelManager(VkDevice device) : device_(device) {}

VulkanKernelManager::~VulkanKernelManager() {
    cleanup();
}

void VulkanKernelManager::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, pipe] : pipelines_) {
        if (pipe) {
            if (pipe->pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipe->pipeline, nullptr);
            if (pipe->layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipe->layout, nullptr);
            if (pipe->desc_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, pipe->desc_layout, nullptr);
            if (pipe->shader_module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, pipe->shader_module, nullptr);
            pipe->pipeline = VK_NULL_HANDLE;
            pipe->layout = VK_NULL_HANDLE;
            pipe->desc_layout = VK_NULL_HANDLE;
            pipe->shader_module = VK_NULL_HANDLE;
        }
    }
    pipelines_.clear();
}

std::shared_ptr<VulkanComputePipeline> VulkanKernelManager::get_or_create_pipeline(
    const std::string& name,
    const uint32_t* spirv_code,
    size_t spirv_size,
    uint32_t num_buffers,
    uint32_t push_constant_size) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pipelines_.find(name);
    if (it != pipelines_.end()) {
        return it->second;
    }

    auto pipe = std::make_shared<VulkanComputePipeline>();

    // 1. Create Shader Module
    VkShaderModuleCreateInfo sm_info{};
    sm_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    sm_info.codeSize = spirv_size;
    sm_info.pCode = spirv_code;

    if (vkCreateShaderModule(device_, &sm_info, nullptr, &pipe->shader_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan shader module for kernel: " + name);
    }

    // 2. Create Descriptor Set Layout
    std::vector<VkDescriptorSetLayoutBinding> bindings(num_buffers);
    for (uint32_t i = 0; i < num_buffers; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo desc_info{};
    desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    desc_info.bindingCount = static_cast<uint32_t>(bindings.size());
    desc_info.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &desc_info, nullptr, &pipe->desc_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, pipe->shader_module, nullptr);
        throw std::runtime_error("Failed to create Vulkan descriptor set layout for kernel: " + name);
    }

    // 3. Create Pipeline Layout
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &pipe->desc_layout;

    VkPushConstantRange pc_range{};
    if (push_constant_size > 0) {
        pc_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pc_range.offset = 0;
        pc_range.size = push_constant_size;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pc_range;
    }

    if (vkCreatePipelineLayout(device_, &layout_info, nullptr, &pipe->layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device_, pipe->desc_layout, nullptr);
        vkDestroyShaderModule(device_, pipe->shader_module, nullptr);
        throw std::runtime_error("Failed to create Vulkan pipeline layout for kernel: " + name);
    }

    // 4. Create Compute Pipeline
    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = pipe->shader_module;
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = pipe->layout;

    if (vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipe->pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device_, pipe->layout, nullptr);
        vkDestroyDescriptorSetLayout(device_, pipe->desc_layout, nullptr);
        vkDestroyShaderModule(device_, pipe->shader_module, nullptr);
        throw std::runtime_error("Failed to create Vulkan compute pipeline for kernel: " + name);
    }

    pipelines_[name] = pipe;
    return pipe;
}

} // namespace nextviper
