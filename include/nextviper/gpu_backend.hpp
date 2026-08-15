#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>
#include "nextviper/tensor.hpp"

namespace nextviper {

// GPU Buffer descriptor holding allocated device memory and host mapping
struct GPUBuffer {
    void* handle = nullptr;        // Backend-specific handle (e.g. VkBuffer)
    void* memory = nullptr;        // Backend-specific device memory (e.g. VkDeviceMemory)
    void* mapped_data = nullptr;   // Mapped host memory pointer if host-visible
    size_t size_bytes = 0;
    bool is_device_local = true;

    ~GPUBuffer();
};

// Abstract GPU Tensor Backend Interface for vendor-agnostic accelerator support
class GPUTensorBackend : public TensorBackend {
public:
    virtual ~GPUTensorBackend() = default;

    // Backend metadata
    std::string name() const override { return "gpu"; }
    Device device() const override { return Device::GPU; }
    virtual std::string device_name() const = 0;
    virtual std::string vendor_name() const = 0;
    virtual std::string driver_version() const = 0;
    virtual size_t total_memory() const = 0;
    virtual size_t free_memory() const = 0;
    virtual int device_count() const = 0;

    // Synchronization
    virtual void synchronize() = 0;

    // Memory transfers
    virtual void copy_host_to_device(void* dst_device, const void* src_host, size_t bytes) = 0;
    virtual void copy_device_to_host(void* dst_host, const void* src_device, size_t bytes) = 0;
    virtual void copy_device_to_device(void* dst_device, const void* src_device, size_t bytes) = 0;

    // Direct data access (downloads to host buffer for inspection/printing)
    virtual void read_buffer(const void* device_ptr, void* host_dst, size_t bytes) = 0;
    virtual void write_buffer(void* device_ptr, const void* host_src, size_t bytes) = 0;

    // GPU Compute Operations
    virtual void add(void* out, const void* a, const void* b, size_t count, DType dtype) = 0;
    virtual void sub(void* out, const void* a, const void* b, size_t count, DType dtype) = 0;
    virtual void mul(void* out, const void* a, const void* b, size_t count, DType dtype) = 0;
    virtual void div(void* out, const void* a, const void* b, size_t count, DType dtype) = 0;
    virtual void scalar_add(void* out, const void* in, double scalar, size_t count, DType dtype) = 0;
    virtual void scalar_mul(void* out, const void* in, double scalar, size_t count, DType dtype) = 0;

    virtual void relu(void* out, const void* in, size_t count, DType dtype) = 0;
    virtual void sigmoid(void* out, const void* in, size_t count, DType dtype) = 0;
    virtual void tanh_act(void* out, const void* in, size_t count, DType dtype) = 0;
    virtual void exp_act(void* out, const void* in, size_t count, DType dtype) = 0;
    virtual void log_act(void* out, const void* in, size_t count, DType dtype) = 0;
    virtual void neg_act(void* out, const void* in, size_t count, DType dtype) = 0;
    virtual void abs_act(void* out, const void* in, size_t count, DType dtype) = 0;

    virtual void matmul(void* out, const void* a, const void* b, int64_t M, int64_t K, int64_t N, DType dtype) = 0;
    virtual void transpose(void* out, const void* in, int64_t rows, int64_t cols, DType dtype) = 0;
    virtual void sum(void* out, const void* in, size_t count, DType dtype) = 0;
    virtual void min_val(void* out, const void* in, size_t count, DType dtype) = 0;
    virtual void max_val(void* out, const void* in, size_t count, DType dtype) = 0;

    // GPU Optimizer Kernels
    virtual void sgd_step(void* param, const void* grad, size_t count, float lr, float weight_decay) = 0;
    virtual void momentum_step(void* param, const void* grad, void* momentum_buf, size_t count, float lr, float beta, float weight_decay) = 0;
    virtual void adam_step(void* param, const void* grad, void* m_buf, void* v_buf, size_t count, float lr, float beta1, float beta2, float eps, float weight_decay, int64_t step) = 0;
    virtual void adamw_step(void* param, const void* grad, void* m_buf, void* v_buf, size_t count, float lr, float beta1, float beta2, float eps, float weight_decay, int64_t step) = 0;

    // Singleton and discovery
    static GPUTensorBackend& instance();
    static bool is_gpu_available();
    static std::string get_gpu_name();
    static void set_default_device(Device dev);
    static Device get_default_device();
};

} // namespace nextviper
