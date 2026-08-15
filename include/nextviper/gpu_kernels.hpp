#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <cstdint>

namespace nextviper {

struct VulkanComputePipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout = VK_NULL_HANDLE;
    VkShaderModule shader_module = VK_NULL_HANDLE;

    ~VulkanComputePipeline();
};

class VulkanKernelManager {
public:
    VulkanKernelManager(VkDevice device);
    ~VulkanKernelManager();

    std::shared_ptr<VulkanComputePipeline> get_or_create_pipeline(
        const std::string& name,
        const uint32_t* spirv_code,
        size_t spirv_size,
        uint32_t num_buffers,
        uint32_t push_constant_size = 0
    );

    void cleanup();

private:
    VkDevice device_;
    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<VulkanComputePipeline>> pipelines_;
};

} // namespace nextviper
