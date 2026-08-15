#include "nextviper/gpu_backend.hpp"
#include "nextviper/gpu_kernels.hpp"
#include "nextviper/gpu_shaders_spirv.hpp"
#include <vulkan/vulkan.h>
#include <iostream>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace nextviper {

static Device g_default_device = Device::CPU;
static std::mutex g_backend_mutex;

// Destructor for GPUBuffer
GPUBuffer::~GPUBuffer() {
    // Managed via shared_ptr custom deleter in VulkanGPUTensorBackend
}

class VulkanGPUTensorBackend : public GPUTensorBackend {
public:
    VulkanGPUTensorBackend() {
        init_vulkan();
    }

    ~VulkanGPUTensorBackend() override {
        cleanup();
    }

    bool is_available() const override {
        return initialized_ && physical_device_ != VK_NULL_HANDLE;
    }

    std::string device_name() const override {
        return device_name_;
    }

    std::string vendor_name() const override {
        return vendor_name_;
    }

    std::string driver_version() const override {
        return driver_version_;
    }

    size_t total_memory() const override {
        return total_memory_bytes_;
    }

    size_t free_memory() const override {
        return total_memory_bytes_ > allocated_memory_bytes_ ? (total_memory_bytes_ - allocated_memory_bytes_) : 0;
    }

    int device_count() const override {
        return device_count_;
    }

    void synchronize() override {
        if (!is_available()) return;
        std::lock_guard<std::mutex> lock(queue_mutex_);
        vkQueueWaitIdle(compute_queue_);
    }

    std::shared_ptr<void> allocate(size_t bytes) override {
        if (!is_available()) {
            throw std::runtime_error("GPU unavailable: No compatible GPU/Vulkan compute device initialized");
        }
        if (bytes == 0) bytes = 4; // minimum allocation

        auto gpu_buf = std::make_shared<GPUBuffer>();
        gpu_buf->size_bytes = bytes;

        // 1. Create VkBuffer
        VkBufferCreateInfo buf_info{};
        buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_info.size = bytes;
        buf_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer vk_buf = VK_NULL_HANDLE;
        if (vkCreateBuffer(device_, &buf_info, nullptr, &vk_buf) != VK_SUCCESS) {
            throw std::runtime_error("GPU Out of Memory: Failed to create Vulkan buffer of size " + std::to_string(bytes));
        }
        gpu_buf->handle = static_cast<void*>(vk_buf);

        // 2. Query memory requirements
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(device_, vk_buf, &mem_reqs);

        // 3. Allocate device memory (prefer host-visible + coherent for zero-overhead direct compute)
        uint32_t mem_type_idx = find_memory_type(
            mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = mem_type_idx;

        VkDeviceMemory dev_mem = VK_NULL_HANDLE;
        if (vkAllocateMemory(device_, &alloc_info, nullptr, &dev_mem) != VK_SUCCESS) {
            vkDestroyBuffer(device_, vk_buf, nullptr);
            throw std::runtime_error("GPU Out of Memory: Failed to allocate " + std::to_string(mem_reqs.size) + " bytes on device");
        }
        gpu_buf->memory = static_cast<void*>(dev_mem);

        // 4. Bind memory
        vkBindBufferMemory(device_, vk_buf, dev_mem, 0);

        // 5. Map persistent memory pointer
        void* mapped = nullptr;
        vkMapMemory(device_, dev_mem, 0, bytes, 0, &mapped);
        gpu_buf->mapped_data = mapped;

        allocated_memory_bytes_ += mem_reqs.size;

        return std::shared_ptr<void>(gpu_buf.get(), [gpu_buf, dev_mem, vk_buf, this, mem_size = mem_reqs.size](void*) mutable {
            if (this && this->device_ != VK_NULL_HANDLE) {
                if (gpu_buf->mapped_data) vkUnmapMemory(this->device_, dev_mem);
                if (vk_buf != VK_NULL_HANDLE) vkDestroyBuffer(this->device_, vk_buf, nullptr);
                if (dev_mem != VK_NULL_HANDLE) vkFreeMemory(this->device_, dev_mem, nullptr);
            }
            gpu_buf->mapped_data = nullptr;
            gpu_buf->handle = nullptr;
            gpu_buf->memory = nullptr;
            allocated_memory_bytes_ -= mem_size;
        });
    }

    void copy(void* dst, const void* src, size_t bytes) override {
        copy_device_to_device(dst, src, bytes);
    }

    void fill(void* data, size_t count, double val, DType dtype) override {
        auto* buf = static_cast<GPUBuffer*>(data);
        if (!buf || !buf->mapped_data) return;
        if (dtype == DType::FLOAT32) {
            float f = static_cast<float>(val);
            float* ptr = static_cast<float*>(buf->mapped_data);
            for (size_t i = 0; i < count; ++i) ptr[i] = f;
        } else if (dtype == DType::FLOAT64) {
            double* ptr = static_cast<double*>(buf->mapped_data);
            for (size_t i = 0; i < count; ++i) ptr[i] = val;
        } else if (dtype == DType::INT32) {
            int32_t* ptr = static_cast<int32_t*>(buf->mapped_data);
            for (size_t i = 0; i < count; ++i) ptr[i] = static_cast<int32_t>(val);
        } else if (dtype == DType::INT64) {
            int64_t* ptr = static_cast<int64_t*>(buf->mapped_data);
            for (size_t i = 0; i < count; ++i) ptr[i] = static_cast<int64_t>(val);
        }
    }

    void copy_host_to_device(void* dst_device, const void* src_host, size_t bytes) override {
        auto* buf = static_cast<GPUBuffer*>(dst_device);
        if (!buf || !buf->mapped_data || !src_host) return;
        std::memcpy(buf->mapped_data, src_host, bytes);
    }

    void copy_device_to_host(void* dst_host, const void* src_device, size_t bytes) override {
        auto* buf = static_cast<const GPUBuffer*>(src_device);
        if (!buf || !buf->mapped_data || !dst_host) return;
        std::memcpy(dst_host, buf->mapped_data, bytes);
    }

    void copy_device_to_device(void* dst_device, const void* src_device, size_t bytes) override {
        auto* d_buf = static_cast<GPUBuffer*>(dst_device);
        auto* s_buf = static_cast<const GPUBuffer*>(src_device);
        if (!d_buf || !s_buf || !d_buf->mapped_data || !s_buf->mapped_data) return;
        std::memcpy(d_buf->mapped_data, s_buf->mapped_data, bytes);
    }

    void read_buffer(const void* device_ptr, void* host_dst, size_t bytes) override {
        copy_device_to_host(host_dst, device_ptr, bytes);
    }

    void write_buffer(void* device_ptr, const void* host_src, size_t bytes) override {
        copy_host_to_device(device_ptr, host_src, bytes);
    }

    // ========================================================================
    // GPU Compute Kernel Dispatches
    // ========================================================================

    void add(void* out, const void* a, const void* b, size_t count, DType dtype) override {
        dispatch_binary_eltwise(out, a, b, count, 0u, dtype);
    }

    void sub(void* out, const void* a, const void* b, size_t count, DType dtype) override {
        dispatch_binary_eltwise(out, a, b, count, 1u, dtype);
    }

    void mul(void* out, const void* a, const void* b, size_t count, DType dtype) override {
        dispatch_binary_eltwise(out, a, b, count, 2u, dtype);
    }

    void div(void* out, const void* a, const void* b, size_t count, DType dtype) override {
        dispatch_binary_eltwise(out, a, b, count, 3u, dtype);
    }

    void scalar_add(void* out, const void* in, double scalar, size_t count, DType dtype) override {
        dispatch_unary_eltwise(out, in, count, 6u, static_cast<float>(scalar), 0.0f, dtype);
    }

    void scalar_mul(void* out, const void* in, double scalar, size_t count, DType dtype) override {
        dispatch_unary_eltwise(out, in, count, 7u, static_cast<float>(scalar), 0.0f, dtype);
    }

    void relu(void* out, const void* in, size_t count, DType dtype) override {
        dispatch_unary_eltwise(out, in, count, 0u, 0.0f, 0.0f, dtype);
    }

    void sigmoid(void* out, const void* in, size_t count, DType dtype) override {
        dispatch_unary_eltwise(out, in, count, 1u, 0.0f, 0.0f, dtype);
    }

    void tanh_act(void* out, const void* in, size_t count, DType dtype) override {
        dispatch_unary_eltwise(out, in, count, 2u, 0.0f, 0.0f, dtype);
    }

    void exp_act(void* out, const void* in, size_t count, DType dtype) override {
        dispatch_unary_eltwise(out, in, count, 3u, 0.0f, 0.0f, dtype);
    }

    void log_act(void* out, const void* in, size_t count, DType dtype) override {
        dispatch_unary_eltwise(out, in, count, 4u, 0.0f, 0.0f, dtype);
    }

    void neg_act(void* out, const void* in, size_t count, DType dtype) override {
        dispatch_unary_eltwise(out, in, count, 5u, 0.0f, 0.0f, dtype);
    }

    void abs_act(void* out, const void* in, size_t count, DType dtype) override {
        dispatch_unary_eltwise(out, in, count, 8u, 0.0f, 0.0f, dtype);
    }

    void matmul(void* out, const void* a, const void* b, int64_t M, int64_t K, int64_t N, DType dtype) override {
        if (!is_available()) throw std::runtime_error("GPU unavailable");
        (void)dtype;

        auto* buf_c = static_cast<GPUBuffer*>(out);
        auto* buf_a = static_cast<const GPUBuffer*>(a);
        auto* buf_b = static_cast<const GPUBuffer*>(b);

        auto pipe = kernel_mgr_->get_or_create_pipeline(
            "matmul",
            gpu_shaders::matmul_spv,
            gpu_shaders::matmul_spv_size,
            3,
            sizeof(uint32_t) * 3
        );

        struct PushDims {
            uint32_t M, K, N;
        } dims{static_cast<uint32_t>(M), static_cast<uint32_t>(K), static_cast<uint32_t>(N)};

        dispatch_pipeline_3buf(
            pipe,
            static_cast<VkBuffer>(buf_a->handle),
            static_cast<VkBuffer>(buf_b->handle),
            static_cast<VkBuffer>(buf_c->handle),
            &dims,
            sizeof(dims),
            (static_cast<uint32_t>(N) + 15u) / 16u,
            (static_cast<uint32_t>(M) + 15u) / 16u,
            1
        );
    }

    void transpose(void* out, const void* in, int64_t rows, int64_t cols, DType dtype) override {
        auto* buf_out = static_cast<GPUBuffer*>(out);
        auto* buf_in = static_cast<const GPUBuffer*>(in);
        if (dtype == DType::FLOAT32) {
            const float* src = static_cast<const float*>(buf_in->mapped_data);
            float* dst = static_cast<float*>(buf_out->mapped_data);
            for (int64_t r = 0; r < rows; ++r) {
                for (int64_t c = 0; c < cols; ++c) {
                    dst[c * rows + r] = src[r * cols + c];
                }
            }
        }
    }

    void sum(void* out, const void* in, size_t count, DType dtype) override {
        auto* buf_out = static_cast<GPUBuffer*>(out);
        auto* buf_in = static_cast<const GPUBuffer*>(in);
        if (count == 0) {
            fill(out, 1, 0.0, dtype);
            return;
        }

        if (count <= 256) {
            auto pipe = kernel_mgr_->get_or_create_pipeline(
                "reduce",
                gpu_shaders::reduce_spv,
                gpu_shaders::reduce_spv_size,
                2,
                sizeof(uint32_t) * 2
            );

            struct PushParams {
                uint32_t count;
                uint32_t op_type;
            } params{static_cast<uint32_t>(count), 0u};

            dispatch_pipeline_2buf(
                pipe,
                static_cast<VkBuffer>(buf_in->handle),
                static_cast<VkBuffer>(buf_out->handle),
                &params,
                sizeof(params),
                1,
                1,
                1
            );
        } else {
            // Fast fallback reduction for multi-block tensors
            float s = 0.0f;
            const float* ptr = static_cast<const float*>(buf_in->mapped_data);
            for (size_t i = 0; i < count; ++i) s += ptr[i];
            static_cast<float*>(buf_out->mapped_data)[0] = s;
        }
    }

    void min_val(void* out, const void* in, size_t count, DType dtype) override {
        (void)dtype;
        auto* buf_out = static_cast<GPUBuffer*>(out);
        auto* buf_in = static_cast<const GPUBuffer*>(in);
        if (count == 0) return;
        float m = static_cast<const float*>(buf_in->mapped_data)[0];
        const float* ptr = static_cast<const float*>(buf_in->mapped_data);
        for (size_t i = 1; i < count; ++i) m = std::min(m, ptr[i]);
        static_cast<float*>(buf_out->mapped_data)[0] = m;
    }

    void max_val(void* out, const void* in, size_t count, DType dtype) override {
        (void)dtype;
        auto* buf_out = static_cast<GPUBuffer*>(out);
        auto* buf_in = static_cast<const GPUBuffer*>(in);
        if (count == 0) return;
        float m = static_cast<const float*>(buf_in->mapped_data)[0];
        const float* ptr = static_cast<const float*>(buf_in->mapped_data);
        for (size_t i = 1; i < count; ++i) m = std::max(m, ptr[i]);
        static_cast<float*>(buf_out->mapped_data)[0] = m;
    }

    // ========================================================================
    // GPU Optimizer Kernels
    // ========================================================================

    void sgd_step(void* param, const void* grad, size_t count, float lr, float weight_decay) override {
        dispatch_optimizer_kernel(param, grad, nullptr, nullptr, count, 0u, lr, 0.0f, 0.0f, 0.0f, weight_decay, 1.0f);
    }

    void momentum_step(void* param, const void* grad, void* momentum_buf, size_t count, float lr, float beta, float weight_decay) override {
        dispatch_optimizer_kernel(param, grad, momentum_buf, nullptr, count, 1u, lr, beta, 0.0f, 0.0f, weight_decay, 1.0f);
    }

    void adam_step(void* param, const void* grad, void* m_buf, void* v_buf, size_t count, float lr, float beta1, float beta2, float eps, float weight_decay, int64_t step) override {
        dispatch_optimizer_kernel(param, grad, m_buf, v_buf, count, 2u, lr, beta1, beta2, eps, weight_decay, static_cast<float>(step));
    }

    void adamw_step(void* param, const void* grad, void* m_buf, void* v_buf, size_t count, float lr, float beta1, float beta2, float eps, float weight_decay, int64_t step) override {
        dispatch_optimizer_kernel(param, grad, m_buf, v_buf, count, 3u, lr, beta1, beta2, eps, weight_decay, static_cast<float>(step));
    }

private:
    bool initialized_ = false;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;
    uint32_t compute_queue_family_ = 0;
    VkCommandPool cmd_pool_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;

    std::unique_ptr<VulkanKernelManager> kernel_mgr_;
    std::mutex queue_mutex_;

    std::string device_name_ = "Unknown GPU";
    std::string vendor_name_ = "Unknown Vendor";
    std::string driver_version_ = "1.0.0";
    size_t total_memory_bytes_ = 0;
    size_t allocated_memory_bytes_ = 0;
    int device_count_ = 0;

    void init_vulkan() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "NextViper";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "NextViperGPU";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo inst_info{};
        inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        inst_info.pApplicationInfo = &app_info;

        if (vkCreateInstance(&inst_info, nullptr, &instance_) != VK_SUCCESS) {
            return;
        }

        // Enumerate Physical Devices
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance_, &count, nullptr);
        device_count_ = static_cast<int>(count);
        if (count == 0) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
            return;
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance_, &count, devices.data());

        // Find compute-capable device (prefer Discrete GPU, then Integrated, then CPU/Virtual)
        VkPhysicalDevice best_dev = VK_NULL_HANDLE;
        int best_score = -1;
        uint32_t best_qf = 0;

        for (auto dev : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);

            uint32_t qf_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
            std::vector<VkQueueFamilyProperties> qf_props(qf_count);
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, qf_props.data());

            for (uint32_t i = 0; i < qf_count; ++i) {
                if (qf_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    int score = 0;
                    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 100;
                    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 50;
                    else score = 10;

                    if (score > best_score) {
                        best_score = score;
                        best_dev = dev;
                        best_qf = i;
                    }
                    break;
                }
            }
        }

        if (best_dev == VK_NULL_HANDLE) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
            return;
        }

        physical_device_ = best_dev;
        compute_queue_family_ = best_qf;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physical_device_, &props);
        device_name_ = props.deviceName;
        vendor_name_ = std::to_string(props.vendorID);
        driver_version_ = std::to_string(VK_VERSION_MAJOR(props.driverVersion)) + "." +
                          std::to_string(VK_VERSION_MINOR(props.driverVersion));

        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);
        for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
            total_memory_bytes_ += mem_props.memoryHeaps[i].size;
        }

        // Create Logical Device
        float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = compute_queue_family_;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;

        VkDeviceCreateInfo dev_info{};
        dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dev_info.queueCreateInfoCount = 1;
        dev_info.pQueueCreateInfos = &queue_info;

        if (vkCreateDevice(physical_device_, &dev_info, nullptr, &device_) != VK_SUCCESS) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
            return;
        }

        vkGetDeviceQueue(device_, compute_queue_family_, 0, &compute_queue_);

        // Command Pool
        VkCommandPoolCreateInfo cp_info{};
        cp_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cp_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cp_info.queueFamilyIndex = compute_queue_family_;
        vkCreateCommandPool(device_, &cp_info, nullptr, &cmd_pool_);

        // Descriptor Pool
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = 1024;

        VkDescriptorPoolCreateInfo dp_info{};
        dp_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dp_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        dp_info.maxSets = 512;
        dp_info.poolSizeCount = 1;
        dp_info.pPoolSizes = &pool_size;
        vkCreateDescriptorPool(device_, &dp_info, nullptr, &desc_pool_);

        kernel_mgr_ = std::make_unique<VulkanKernelManager>(device_);
        initialized_ = true;
    }

    void cleanup() {
        if (device_ != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device_);
            kernel_mgr_.reset();
            if (desc_pool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, desc_pool_, nullptr);
            if (cmd_pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, cmd_pool_, nullptr);
            VkDevice dev = device_;
            device_ = VK_NULL_HANDLE;
            vkDestroyDevice(dev, nullptr);
        }
        if (instance_ != VK_NULL_HANDLE) {
            VkInstance inst = instance_;
            instance_ = VK_NULL_HANDLE;
            vkDestroyInstance(inst, nullptr);
        }
        initialized_ = false;
    }

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            if (type_filter & (1 << i)) return i;
        }
        return 0;
    }

    void dispatch_binary_eltwise(void* out, const void* a, const void* b, size_t count, uint32_t op_type, DType dtype) {
        if (!is_available()) throw std::runtime_error("GPU unavailable");
        (void)dtype;

        auto* buf_c = static_cast<GPUBuffer*>(out);
        auto* buf_a = static_cast<const GPUBuffer*>(a);
        auto* buf_b = static_cast<const GPUBuffer*>(b);

        auto pipe = kernel_mgr_->get_or_create_pipeline(
            "eltwise_binary",
            gpu_shaders::eltwise_binary_spv,
            gpu_shaders::eltwise_binary_spv_size,
            3,
            sizeof(uint32_t) * 2
        );

        struct PushParams {
            uint32_t count;
            uint32_t op_type;
        } params{static_cast<uint32_t>(count), op_type};

        uint32_t group_x = (static_cast<uint32_t>(count) + 255u) / 256u;
        dispatch_pipeline_3buf(
            pipe,
            static_cast<VkBuffer>(buf_a->handle),
            static_cast<VkBuffer>(buf_b->handle),
            static_cast<VkBuffer>(buf_c->handle),
            &params,
            sizeof(params),
            group_x,
            1,
            1
        );
    }

    void dispatch_unary_eltwise(void* out, const void* in, size_t count, uint32_t op_type, float scalar, float param, DType dtype) {
        if (!is_available()) throw std::runtime_error("GPU unavailable");
        (void)dtype;

        auto* buf_c = static_cast<GPUBuffer*>(out);
        auto* buf_a = static_cast<const GPUBuffer*>(in);

        auto pipe = kernel_mgr_->get_or_create_pipeline(
            "eltwise_unary",
            gpu_shaders::eltwise_unary_spv,
            gpu_shaders::eltwise_unary_spv_size,
            2,
            sizeof(uint32_t) * 2 + sizeof(float) * 2
        );

        struct PushParams {
            uint32_t count;
            uint32_t op_type;
            float scalar;
            float param;
        } params{static_cast<uint32_t>(count), op_type, scalar, param};

        uint32_t group_x = (static_cast<uint32_t>(count) + 255u) / 256u;
        dispatch_pipeline_2buf(
            pipe,
            static_cast<VkBuffer>(buf_a->handle),
            static_cast<VkBuffer>(buf_c->handle),
            &params,
            sizeof(params),
            group_x,
            1,
            1
        );
    }

    void dispatch_optimizer_kernel(void* param, const void* grad, void* m_buf, void* v_buf, size_t count, uint32_t opt_type, float lr, float beta1, float beta2, float eps, float weight_decay, float step) {
        if (!is_available()) throw std::runtime_error("GPU unavailable");

        auto* p_buf = static_cast<GPUBuffer*>(param);
        auto* g_buf = static_cast<const GPUBuffer*>(grad);
        auto* m_gpu = m_buf ? static_cast<GPUBuffer*>(m_buf) : p_buf;
        auto* v_gpu = v_buf ? static_cast<GPUBuffer*>(v_buf) : p_buf;

        auto pipe = kernel_mgr_->get_or_create_pipeline(
            "optimizer",
            gpu_shaders::optimizer_spv,
            gpu_shaders::optimizer_spv_size,
            4,
            sizeof(uint32_t) * 2 + sizeof(float) * 6
        );

        struct PushParams {
            uint32_t count;
            uint32_t opt_type;
            float lr, beta1, beta2, eps, weight_decay, step_count;
        } params{static_cast<uint32_t>(count), opt_type, lr, beta1, beta2, eps, weight_decay, step};

        uint32_t group_x = (static_cast<uint32_t>(count) + 255u) / 256u;
        dispatch_pipeline_4buf(
            pipe,
            static_cast<VkBuffer>(p_buf->handle),
            static_cast<VkBuffer>(g_buf->handle),
            static_cast<VkBuffer>(m_gpu->handle),
            static_cast<VkBuffer>(v_gpu->handle),
            &params,
            sizeof(params),
            group_x,
            1,
            1
        );
    }

    void dispatch_pipeline_2buf(std::shared_ptr<VulkanComputePipeline> pipe, VkBuffer b0, VkBuffer b1, const void* push_data, size_t push_size, uint32_t gx, uint32_t gy, uint32_t gz) {
        std::vector<VkBuffer> bufs = {b0, b1};
        dispatch_pipeline(pipe, bufs, push_data, push_size, gx, gy, gz);
    }

    void dispatch_pipeline_3buf(std::shared_ptr<VulkanComputePipeline> pipe, VkBuffer b0, VkBuffer b1, VkBuffer b2, const void* push_data, size_t push_size, uint32_t gx, uint32_t gy, uint32_t gz) {
        std::vector<VkBuffer> bufs = {b0, b1, b2};
        dispatch_pipeline(pipe, bufs, push_data, push_size, gx, gy, gz);
    }

    void dispatch_pipeline_4buf(std::shared_ptr<VulkanComputePipeline> pipe, VkBuffer b0, VkBuffer b1, VkBuffer b2, VkBuffer b3, const void* push_data, size_t push_size, uint32_t gx, uint32_t gy, uint32_t gz) {
        std::vector<VkBuffer> bufs = {b0, b1, b2, b3};
        dispatch_pipeline(pipe, bufs, push_data, push_size, gx, gy, gz);
    }

    void dispatch_pipeline(std::shared_ptr<VulkanComputePipeline> pipe, const std::vector<VkBuffer>& bufs, const void* push_data, size_t push_size, uint32_t gx, uint32_t gy, uint32_t gz) {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        // 1. Allocate Descriptor Set
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = desc_pool_;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &pipe->desc_layout;

        VkDescriptorSet desc_set;
        if (vkAllocateDescriptorSets(device_, &alloc_info, &desc_set) != VK_SUCCESS) {
            vkResetDescriptorPool(device_, desc_pool_, 0);
            vkAllocateDescriptorSets(device_, &alloc_info, &desc_set);
        }

        // 2. Update Descriptor Set
        std::vector<VkDescriptorBufferInfo> buf_infos(bufs.size());
        std::vector<VkWriteDescriptorSet> writes(bufs.size());
        for (size_t i = 0; i < bufs.size(); ++i) {
            buf_infos[i].buffer = bufs[i];
            buf_infos[i].offset = 0;
            buf_infos[i].range = VK_WHOLE_SIZE;

            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = desc_set;
            writes[i].dstBinding = static_cast<uint32_t>(i);
            writes[i].dstArrayElement = 0;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].descriptorCount = 1;
            writes[i].pBufferInfo = &buf_infos[i];
        }
        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        // 3. Allocate and record command buffer
        VkCommandBufferAllocateInfo cmd_info{};
        cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_info.commandPool = cmd_pool_;
        cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_info.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device_, &cmd_info, &cmd);

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin_info);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->layout, 0, 1, &desc_set, 0, nullptr);

        if (push_data && push_size > 0) {
            vkCmdPushConstants(cmd, pipe->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, static_cast<uint32_t>(push_size), push_data);
        }

        vkCmdDispatch(cmd, gx, gy, gz);
        vkEndCommandBuffer(cmd);

        // 4. Submit and wait
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        vkQueueSubmit(compute_queue_, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(compute_queue_);

        // 5. Clean up command and descriptor
        vkFreeCommandBuffers(device_, cmd_pool_, 1, &cmd);
        vkFreeDescriptorSets(device_, desc_pool_, 1, &desc_set);
    }
};

GPUTensorBackend& GPUTensorBackend::instance() {
    static VulkanGPUTensorBackend s_instance;
    return s_instance;
}

bool GPUTensorBackend::is_gpu_available() {
    return instance().is_available();
}

std::string GPUTensorBackend::get_gpu_name() {
    return instance().device_name();
}

void GPUTensorBackend::set_default_device(Device dev) {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    if (dev == Device::GPU && !is_gpu_available()) {
        throw std::runtime_error("GPU unavailable: Cannot set default device to 'gpu' because no supported Vulkan/GPU compute device was found");
    }
    g_default_device = dev;
}

Device GPUTensorBackend::get_default_device() {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    if (g_default_device == Device::AUTO) {
        return is_gpu_available() ? Device::GPU : Device::CPU;
    }
    return g_default_device;
}

} // namespace nextviper
