#pragma once

#include "nextviper/common.hpp"
#include "nextviper/value.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <iostream>

namespace nextviper {

struct AutogradNode;

enum class Device {
    CPU,
    GPU,
    CUDA,
    MPS,
    AUTO,
    CUSTOM
};

inline std::string device_to_string(Device dev) {
    switch (dev) {
        case Device::CPU: return "cpu";
        case Device::GPU: return "gpu";
        case Device::CUDA: return "cuda";
        case Device::MPS: return "mps";
        case Device::AUTO: return "auto";
        case Device::CUSTOM: return "custom";
    }
    return "cpu";
}

inline Device string_to_device(const std::string& str) {
    if (str == "gpu") return Device::GPU;
    if (str == "cuda") return Device::CUDA;
    if (str == "mps") return Device::MPS;
    if (str == "auto") return Device::AUTO;
    if (str == "custom") return Device::CUSTOM;
    return Device::CPU;
}

enum class DType {
    FLOAT32,
    FLOAT64,
    INT32,
    INT64
};

inline size_t dtype_size(DType dt) {
    switch (dt) {
        case DType::FLOAT32: return sizeof(float);
        case DType::FLOAT64: return sizeof(double);
        case DType::INT32: return sizeof(int32_t);
        case DType::INT64: return sizeof(int64_t);
    }
    return sizeof(float);
}

inline std::string dtype_to_string(DType dt) {
    switch (dt) {
        case DType::FLOAT32: return "float32";
        case DType::FLOAT64: return "float64";
        case DType::INT32: return "int32";
        case DType::INT64: return "int64";
    }
    return "float32";
}

// Tensor Backend Interface for multi-hardware execution (CPU / GPU / Accelerators)
class TensorBackend {
public:
    virtual ~TensorBackend() = default;
    virtual std::string name() const = 0;
    virtual Device device() const = 0;
    virtual bool is_available() const = 0;

    virtual std::shared_ptr<void> allocate(size_t bytes) = 0;
    virtual void copy(void* dst, const void* src, size_t bytes) = 0;
    virtual void fill(void* data, size_t count, double val, DType dtype) = 0;
};

// Default CPU Backend Implementation
class CPUTensorBackend : public TensorBackend {
public:
    std::string name() const override { return "cpu"; }
    Device device() const override { return Device::CPU; }
    bool is_available() const override { return true; }

    std::shared_ptr<void> allocate(size_t bytes) override;
    void copy(void* dst, const void* src, size_t bytes) override;
    void fill(void* data, size_t count, double val, DType dtype) override;

    static CPUTensorBackend& instance();
};

class Tensor;
struct AutogradNode;

struct AutogradMeta {
    bool requires_grad = false;
    bool is_leaf = true;
    std::shared_ptr<Tensor> grad = nullptr;
    std::shared_ptr<AutogradNode> grad_fn = nullptr;
};

class Tensor {
public:
    Tensor();
    Tensor(std::vector<int64_t> shape, DType dtype = DType::FLOAT32, Device device = Device::CPU);
    Tensor(std::vector<int64_t> shape, const std::vector<double>& values, DType dtype = DType::FLOAT32, Device device = Device::CPU);

    static Tensor from_vector(const std::vector<double>& values, const std::vector<int64_t>& shape, DType dtype = DType::FLOAT32, Device device = Device::CPU);
    static Tensor zeros(const std::vector<int64_t>& shape, DType dtype = DType::FLOAT32, Device device = Device::CPU);
    static Tensor ones(const std::vector<int64_t>& shape, DType dtype = DType::FLOAT32, Device device = Device::CPU);
    static Tensor full(const std::vector<int64_t>& shape, double val, DType dtype = DType::FLOAT32, Device device = Device::CPU);
    static Tensor randn(const std::vector<int64_t>& shape, double mean = 0.0, double stddev = 1.0, Device device = Device::CPU);
    static Tensor uniform(const std::vector<int64_t>& shape, double low = 0.0, double high = 1.0, Device device = Device::CPU);

    // Properties
    const std::vector<int64_t>& shape() const { return shape_; }
    const std::vector<int64_t>& strides() const { return strides_; }
    size_t ndim() const { return shape_.size(); }
    int64_t numel() const { return numel_; }
    DType dtype() const { return dtype_; }
    Device device() const { return device_; }
    bool is_contiguous() const { return is_contiguous_; }

    // Autograd Properties & Methods
    bool requires_grad() const { return autograd_meta_ ? autograd_meta_->requires_grad : false; }
    void set_requires_grad(bool req) { ensure_autograd_meta(); autograd_meta_->requires_grad = req; }
    std::shared_ptr<Tensor> grad() const { return autograd_meta_ ? autograd_meta_->grad : nullptr; }
    void set_grad(std::shared_ptr<Tensor> g) { ensure_autograd_meta(); autograd_meta_->grad = std::move(g); }
    std::shared_ptr<AutogradNode> grad_fn() const { return autograd_meta_ ? autograd_meta_->grad_fn : nullptr; }
    void set_grad_fn(std::shared_ptr<AutogradNode> fn) { ensure_autograd_meta(); autograd_meta_->grad_fn = std::move(fn); }
    bool is_leaf() const { return autograd_meta_ ? autograd_meta_->is_leaf : true; }
    void set_is_leaf(bool leaf) { ensure_autograd_meta(); autograd_meta_->is_leaf = leaf; }

    void backward(const Tensor& grad_output = {});
    void zero_grad();
    Tensor detach() const;

    // Memory access
    void* data() { return data_.get(); }
    const void* data() const { return data_.get(); }
    double get_flat(size_t index) const;
    void set_flat(size_t index, double val);
    double get(const std::vector<int64_t>& indices) const;
    void set(const std::vector<int64_t>& indices, double val);

    // Conversions
    std::vector<double> to_vector() const;
    double item() const;
    Tensor to(Device dev) const;
    Tensor clone() const;

    // Tensor shape transformations
    Tensor reshape(const std::vector<int64_t>& new_shape) const;
    Tensor transpose(int64_t dim0, int64_t dim1) const;
    Tensor T() const; // 2D Transpose
    Tensor flatten() const;

    // Mathematical operations (with automatic autograd tracking)
    Tensor add(const Tensor& other) const;
    Tensor sub(const Tensor& other) const;
    Tensor mul(const Tensor& other) const;
    Tensor div(const Tensor& other) const;
    Tensor scalar_add(double scalar) const;
    Tensor scalar_mul(double scalar) const;
    Tensor matmul(const Tensor& other) const;
    Tensor neg() const;
    Tensor pow(double exponent) const;
    Tensor exp() const;
    Tensor log(double eps = 1e-12) const;
    Tensor abs() const;
    Tensor clamp(double low, double high) const;

    // Reductions
    Tensor sum(int64_t dim = -1, bool keepdims = false) const;
    Tensor mean(int64_t dim = -1, bool keepdims = false) const;
    Tensor max(int64_t dim = -1, bool keepdims = false) const;
    Tensor min(int64_t dim = -1, bool keepdims = false) const;
    Tensor argmax(int64_t dim = -1) const;

    // Activations
    Tensor relu() const;
    Tensor sigmoid() const;
    Tensor tanh() const;
    Tensor softmax(int64_t dim = -1) const;

    // String formatting
    std::string to_string() const;

    // NextViper Value integration
    Value to_value() const;

private:
    std::vector<int64_t> shape_;
    std::vector<int64_t> strides_;
    int64_t numel_ = 0;
    DType dtype_ = DType::FLOAT32;
    Device device_ = Device::CPU;
    bool is_contiguous_ = true;
    std::shared_ptr<void> data_;

    // Autograd metadata
    std::shared_ptr<AutogradMeta> autograd_meta_ = nullptr;

    void ensure_autograd_meta() const {
        if (!autograd_meta_) {
            const_cast<Tensor*>(this)->autograd_meta_ = std::make_shared<AutogradMeta>();
        }
    }

    void compute_strides();
    size_t compute_offset(const std::vector<int64_t>& indices) const;
};

} // namespace nextviper
