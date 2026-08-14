#include "nextviper/tensor.hpp"
#include "nextviper/autograd.hpp"
#include "nextviper/interpreter.hpp"
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace nextviper {

// --- CPUTensorBackend Implementation ---

CPUTensorBackend& CPUTensorBackend::instance() {
    static CPUTensorBackend backend;
    return backend;
}

std::shared_ptr<void> CPUTensorBackend::allocate(size_t bytes) {
    if (bytes == 0) return nullptr;
    void* raw = ::operator new(bytes);
    return std::shared_ptr<void>(raw, [](void* p) {
        if (p) ::operator delete(p);
    });
}

void CPUTensorBackend::copy(void* dst, const void* src, size_t bytes) {
    if (dst && src && bytes > 0) {
        std::memcpy(dst, src, bytes);
    }
}

void CPUTensorBackend::fill(void* data, size_t count, double val, DType dtype) {
    if (!data || count == 0) return;
    if (dtype == DType::FLOAT32) {
        float* ptr = static_cast<float*>(data);
        float fval = static_cast<float>(val);
        std::fill(ptr, ptr + count, fval);
    } else if (dtype == DType::FLOAT64) {
        double* ptr = static_cast<double*>(data);
        std::fill(ptr, ptr + count, val);
    } else if (dtype == DType::INT32) {
        int32_t* ptr = static_cast<int32_t*>(data);
        int32_t ival = static_cast<int32_t>(val);
        std::fill(ptr, ptr + count, ival);
    } else if (dtype == DType::INT64) {
        int64_t* ptr = static_cast<int64_t*>(data);
        int64_t ival = static_cast<int64_t>(val);
        std::fill(ptr, ptr + count, ival);
    }
}

// --- Tensor Implementation ---

void Tensor::compute_strides() {
    strides_.resize(shape_.size());
    if (shape_.empty()) return;
    int64_t stride = 1;
    for (int i = static_cast<int>(shape_.size()) - 1; i >= 0; --i) {
        strides_[i] = stride;
        stride *= shape_[i];
    }
}

Tensor::Tensor()
    : shape_({0}), numel_(0), dtype_(DType::FLOAT32), device_(Device::CPU), is_contiguous_(true) {
    compute_strides();
}

Tensor::Tensor(std::vector<int64_t> shape, DType dtype, Device device)
    : shape_(std::move(shape)), dtype_(dtype), device_(device), is_contiguous_(true) {
    constexpr int64_t MAX_TENSOR_NUMEL = 100'000'000;
    numel_ = 1;
    for (int64_t dim : shape_) {
        if (dim < 0) throw std::invalid_argument("Negative dimension size in Tensor");
        if (dim > MAX_TENSOR_NUMEL || (dim > 0 && numel_ > MAX_TENSOR_NUMEL / dim)) {
            throw std::invalid_argument("Tensor shape exceeds maximum allowed elements (100,000,000)");
        }
        numel_ *= dim;
    }
    compute_strides();
    size_t bytes = numel_ * dtype_size(dtype_);
    data_ = CPUTensorBackend::instance().allocate(bytes);
    CPUTensorBackend::instance().fill(data_.get(), numel_, 0.0, dtype_);
}

Tensor::Tensor(std::vector<int64_t> shape, const std::vector<double>& values, DType dtype, Device device)
    : shape_(std::move(shape)), dtype_(dtype), device_(device), is_contiguous_(true) {
    constexpr int64_t MAX_TENSOR_NUMEL = 100'000'000;
    numel_ = 1;
    for (int64_t dim : shape_) {
        if (dim < 0) throw std::invalid_argument("Negative dimension size in Tensor");
        if (dim > MAX_TENSOR_NUMEL || (dim > 0 && numel_ > MAX_TENSOR_NUMEL / dim)) {
            throw std::invalid_argument("Tensor shape exceeds maximum allowed elements (100,000,000)");
        }
        numel_ *= dim;
    }
    compute_strides();
    size_t bytes = numel_ * dtype_size(dtype_);
    data_ = CPUTensorBackend::instance().allocate(bytes);
    for (int64_t i = 0; i < numel_; ++i) {
        double val = (i < static_cast<int64_t>(values.size())) ? values[i] : 0.0;
        set_flat(i, val);
    }
}

Tensor Tensor::from_vector(const std::vector<double>& values, const std::vector<int64_t>& shape, DType dtype, Device device) {
    return Tensor(shape, values, dtype, device);
}

Tensor Tensor::zeros(const std::vector<int64_t>& shape, DType dtype, Device device) {
    return Tensor(shape, dtype, device);
}

Tensor Tensor::ones(const std::vector<int64_t>& shape, DType dtype, Device device) {
    Tensor t(shape, dtype, device);
    CPUTensorBackend::instance().fill(t.data_.get(), t.numel_, 1.0, dtype);
    return t;
}

Tensor Tensor::full(const std::vector<int64_t>& shape, double val, DType dtype, Device device) {
    Tensor t(shape, dtype, device);
    CPUTensorBackend::instance().fill(t.data_.get(), t.numel_, val, dtype);
    return t;
}

Tensor Tensor::randn(const std::vector<int64_t>& shape, double mean, double stddev, Device device) {
    Tensor t(shape, DType::FLOAT32, device);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(mean, stddev);
    for (int64_t i = 0; i < t.numel_; ++i) {
        t.set_flat(i, dist(gen));
    }
    return t;
}

Tensor Tensor::uniform(const std::vector<int64_t>& shape, double low, double high, Device device) {
    Tensor t(shape, DType::FLOAT32, device);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(low, high);
    for (int64_t i = 0; i < t.numel_; ++i) {
        t.set_flat(i, dist(gen));
    }
    return t;
}

double Tensor::get_flat(size_t index) const {
    if (index >= static_cast<size_t>(numel_)) return 0.0;
    if (dtype_ == DType::FLOAT32) return static_cast<const float*>(data_.get())[index];
    if (dtype_ == DType::FLOAT64) return static_cast<const double*>(data_.get())[index];
    if (dtype_ == DType::INT32) return static_cast<const int32_t*>(data_.get())[index];
    if (dtype_ == DType::INT64) return static_cast<const int64_t*>(data_.get())[index];
    return 0.0;
}

void Tensor::set_flat(size_t index, double val) {
    if (index >= static_cast<size_t>(numel_)) return;
    if (dtype_ == DType::FLOAT32) static_cast<float*>(data_.get())[index] = static_cast<float>(val);
    else if (dtype_ == DType::FLOAT64) static_cast<double*>(data_.get())[index] = val;
    else if (dtype_ == DType::INT32) static_cast<int32_t*>(data_.get())[index] = static_cast<int32_t>(val);
    else if (dtype_ == DType::INT64) static_cast<int64_t*>(data_.get())[index] = static_cast<int64_t>(val);
}

size_t Tensor::compute_offset(const std::vector<int64_t>& indices) const {
    size_t offset = 0;
    for (size_t i = 0; i < indices.size(); ++i) {
        if (i < strides_.size()) {
            offset += indices[i] * strides_[i];
        }
    }
    return offset;
}

double Tensor::get(const std::vector<int64_t>& indices) const {
    return get_flat(compute_offset(indices));
}

void Tensor::set(const std::vector<int64_t>& indices, double val) {
    set_flat(compute_offset(indices), val);
}

std::vector<double> Tensor::to_vector() const {
    std::vector<double> res(numel_);
    for (int64_t i = 0; i < numel_; ++i) res[i] = get_flat(i);
    return res;
}

double Tensor::item() const {
    if (numel_ == 0) return 0.0;
    return get_flat(0);
}

Tensor Tensor::to(Device dev) const {
    if (dev == device_) return *this;
    Tensor res(shape_, dtype_, dev);
    CPUTensorBackend::instance().copy(res.data_.get(), data_.get(), numel_ * dtype_size(dtype_));
    return res;
}

Tensor Tensor::clone() const {
    Tensor res(shape_, dtype_, device_);
    CPUTensorBackend::instance().copy(res.data_.get(), data_.get(), numel_ * dtype_size(dtype_));
    res.set_requires_grad(requires_grad());
    res.set_is_leaf(is_leaf());
    return res;
}

Tensor Tensor::reshape(const std::vector<int64_t>& new_shape) const {
    int64_t new_numel = 1;
    int64_t infer_idx = -1;
    for (size_t i = 0; i < new_shape.size(); ++i) {
        if (new_shape[i] == -1) {
            if (infer_idx != -1) throw std::invalid_argument("Can only specify one unknown dimension (-1)");
            infer_idx = static_cast<int64_t>(i);
        } else {
            new_numel *= new_shape[i];
        }
    }
    std::vector<int64_t> final_shape = new_shape;
    if (infer_idx != -1) {
        final_shape[infer_idx] = numel_ / new_numel;
        new_numel *= final_shape[infer_idx];
    }
    if (new_numel != numel_) {
        throw std::invalid_argument("Total size of new array must be unchanged in reshape");
    }

    Tensor res = *this;
    res.shape_ = final_shape;
    res.compute_strides();

    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<ReshapeBackwardNode>(shape_);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::transpose(int64_t dim0, int64_t dim1) const {
    if (ndim() != 2) throw std::invalid_argument("transpose currently supports 2D tensors");
    (void)dim0; (void)dim1;
    return T();
}

Tensor Tensor::T() const {
    if (ndim() != 2) throw std::invalid_argument("T() requires a 2D tensor");
    int64_t rows = shape_[0];
    int64_t cols = shape_[1];
    Tensor res({cols, rows}, dtype_, device_);
    for (int64_t r = 0; r < rows; ++r) {
        for (int64_t c = 0; c < cols; ++c) {
            res.set({c, r}, get({r, c}));
        }
    }
    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<TransposeBackwardNode>();
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::flatten() const {
    return reshape({numel_});
}

// Mathematical operations with Autograd integration

template<typename Op>
static Tensor binary_broadcast_op(const Tensor& a, const Tensor& b, Op op) {
    if (a.shape() == b.shape()) {
        Tensor res(a.shape(), a.dtype(), a.device());
        for (int64_t i = 0; i < a.numel(); ++i) {
            res.set_flat(i, op(a.get_flat(i), b.get_flat(i)));
        }
        return res;
    }
    if (a.numel() == 1) {
        Tensor res(b.shape(), b.dtype(), b.device());
        double s = a.get_flat(0);
        for (int64_t i = 0; i < b.numel(); ++i) {
            res.set_flat(i, op(s, b.get_flat(i)));
        }
        return res;
    }
    if (b.numel() == 1) {
        Tensor res(a.shape(), a.dtype(), a.device());
        double s = b.get_flat(0);
        for (int64_t i = 0; i < a.numel(); ++i) {
            res.set_flat(i, op(a.get_flat(i), s));
        }
        return res;
    }
    if (a.shape().size() == 2 && b.shape().size() == 1 && a.shape()[1] == b.shape()[0]) {
        Tensor res(a.shape(), a.dtype(), a.device());
        for (int64_t r = 0; r < a.shape()[0]; ++r) {
            for (int64_t c = 0; c < a.shape()[1]; ++c) {
                res.set({r, c}, op(a.get({r, c}), b.get_flat(c)));
            }
        }
        return res;
    }
    if (a.shape().size() == 1 && b.shape().size() == 2 && a.shape()[0] == b.shape()[1]) {
        Tensor res(b.shape(), b.dtype(), b.device());
        for (int64_t r = 0; r < b.shape()[0]; ++r) {
            for (int64_t c = 0; c < b.shape()[1]; ++c) {
                res.set({r, c}, op(a.get_flat(c), b.get({r, c})));
            }
        }
        return res;
    }
    if (a.shape().size() == 2 && b.shape().size() == 2) {
        if (a.shape()[0] == b.shape()[0] && b.shape()[1] == 1) {
            Tensor res(a.shape(), a.dtype(), a.device());
            for (int64_t r = 0; r < a.shape()[0]; ++r) {
                for (int64_t c = 0; c < a.shape()[1]; ++c) {
                    res.set({r, c}, op(a.get({r, c}), b.get({r, 0})));
                }
            }
            return res;
        }
        if (a.shape()[0] == b.shape()[0] && a.shape()[1] == 1) {
            Tensor res(b.shape(), b.dtype(), b.device());
            for (int64_t r = 0; r < b.shape()[0]; ++r) {
                for (int64_t c = 0; c < b.shape()[1]; ++c) {
                    res.set({r, c}, op(a.get({r, 0}), b.get({r, c})));
                }
            }
            return res;
        }
    }
    throw std::invalid_argument("Tensor shape mismatch in binary operation");
}

Tensor Tensor::add(const Tensor& other) const {
    Tensor res = binary_broadcast_op(*this, other, [](double u, double v) { return u + v; });
    if (AutogradContext::is_grad_enabled() && (requires_grad() || other.requires_grad())) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<AddBackwardNode>(*this, other);
        node->inputs = {std::make_shared<Tensor>(*this), std::make_shared<Tensor>(other)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::sub(const Tensor& other) const {
    Tensor res = binary_broadcast_op(*this, other, [](double u, double v) { return u - v; });
    if (AutogradContext::is_grad_enabled() && (requires_grad() || other.requires_grad())) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<SubBackwardNode>(*this, other);
        node->inputs = {std::make_shared<Tensor>(*this), std::make_shared<Tensor>(other)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::mul(const Tensor& other) const {
    Tensor res = binary_broadcast_op(*this, other, [](double u, double v) { return u * v; });
    if (AutogradContext::is_grad_enabled() && (requires_grad() || other.requires_grad())) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<MulBackwardNode>(*this, other);
        node->inputs = {std::make_shared<Tensor>(*this), std::make_shared<Tensor>(other)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::div(const Tensor& other) const {
    Tensor res = binary_broadcast_op(*this, other, [](double u, double v) { return v == 0.0 ? 0.0 : (u / v); });
    if (AutogradContext::is_grad_enabled() && (requires_grad() || other.requires_grad())) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<DivBackwardNode>(*this, other);
        node->inputs = {std::make_shared<Tensor>(*this), std::make_shared<Tensor>(other)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::scalar_add(double scalar) const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) res.set_flat(i, get_flat(i) + scalar);
    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<ScalarAddBackwardNode>();
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::scalar_mul(double scalar) const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) res.set_flat(i, get_flat(i) * scalar);
    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<ScalarMulBackwardNode>(scalar);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::neg() const {
    return scalar_mul(-1.0);
}

Tensor Tensor::pow(double exponent) const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) res.set_flat(i, std::pow(get_flat(i), exponent));
    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<PowBackwardNode>(*this, exponent);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::exp() const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) res.set_flat(i, std::exp(get_flat(i)));
    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<ExpBackwardNode>(res);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::log(double eps) const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) res.set_flat(i, std::log(std::max(eps, get_flat(i))));
    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<LogBackwardNode>(*this, eps);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::abs() const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) res.set_flat(i, std::abs(get_flat(i)));
    return res;
}

Tensor Tensor::clamp(double low, double high) const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) res.set_flat(i, std::clamp(get_flat(i), low, high));
    return res;
}

Tensor Tensor::matmul(const Tensor& other) const {
    Tensor res;
    if (ndim() == 2 && other.ndim() == 2) {
        int64_t M = shape_[0];
        int64_t K = shape_[1];
        int64_t K2 = other.shape_[0];
        int64_t N = other.shape_[1];

        if (K != K2) {
            throw std::invalid_argument("Matrix multiplication dimension mismatch: [" +
                                        std::to_string(M) + ", " + std::to_string(K) + "] @ [" +
                                        std::to_string(K2) + ", " + std::to_string(N) + "]");
        }

        res = Tensor({M, N}, dtype_, device_);
        for (int64_t i = 0; i < M; ++i) {
            for (int64_t k = 0; k < K; ++k) {
                double a_ik = get({i, k});
                for (int64_t j = 0; j < N; ++j) {
                    res.set({i, j}, res.get({i, j}) + a_ik * other.get({k, j}));
                }
            }
        }
    } else if (ndim() == 1 && other.ndim() == 2) {
        Tensor a = reshape({1, shape_[0]});
        Tensor out = a.matmul(other);
        res = out.reshape({other.shape_[1]});
    } else if (ndim() == 2 && other.ndim() == 1) {
        Tensor b = other.reshape({other.shape_[0], 1});
        Tensor out = matmul(b);
        res = out.reshape({shape_[0]});
    } else {
        throw std::invalid_argument("matmul supports 1D and 2D tensors");
    }

    if (AutogradContext::is_grad_enabled() && (requires_grad() || other.requires_grad())) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<MatMulBackwardNode>(*this, other);
        node->inputs = {std::make_shared<Tensor>(*this), std::make_shared<Tensor>(other)};
        res.set_grad_fn(node);
    }
    return res;
}

// Reductions
Tensor Tensor::sum(int64_t dim, bool keepdims) const {
    Tensor res;
    if (dim == -1) {
        double s = 0.0;
        for (int64_t i = 0; i < numel_; ++i) s += get_flat(i);
        std::vector<int64_t> out_shape = keepdims ? std::vector<int64_t>(ndim(), 1) : std::vector<int64_t>{1};
        res = Tensor(out_shape, {s}, dtype_, device_);
    } else {
        if (dim < 0) dim += ndim();
        if (dim == 0 && ndim() == 2) {
            res = Tensor(keepdims ? std::vector<int64_t>{1, shape_[1]} : std::vector<int64_t>{shape_[1]}, dtype_, device_);
            for (int64_t c = 0; c < shape_[1]; ++c) {
                double s = 0.0;
                for (int64_t r = 0; r < shape_[0]; ++r) s += get({r, c});
                res.set_flat(c, s);
            }
        } else if (dim == 1 && ndim() == 2) {
            res = Tensor(keepdims ? std::vector<int64_t>{shape_[0], 1} : std::vector<int64_t>{shape_[0]}, dtype_, device_);
            for (int64_t r = 0; r < shape_[0]; ++r) {
                double s = 0.0;
                for (int64_t c = 0; c < shape_[1]; ++c) s += get({r, c});
                res.set_flat(r, s);
            }
        } else {
            throw std::invalid_argument("Unsupported sum dimension reduction");
        }
    }

    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<SumBackwardNode>(shape_, dim, keepdims);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::mean(int64_t dim, bool keepdims) const {
    Tensor s = sum(dim, keepdims);
    double count = (dim == -1) ? static_cast<double>(numel_) : static_cast<double>(shape_[dim < 0 ? dim + ndim() : dim]);
    Tensor res = s.scalar_mul(1.0 / std::max(1.0, count));

    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<MeanBackwardNode>(shape_, dim, keepdims, count);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::max(int64_t dim, bool keepdims) const {
    if (dim == -1) {
        if (numel_ == 0) return Tensor::zeros({1}, dtype_, device_);
        double m = get_flat(0);
        for (int64_t i = 1; i < numel_; ++i) m = std::max(m, get_flat(i));
        std::vector<int64_t> out_shape = keepdims ? std::vector<int64_t>(ndim(), 1) : std::vector<int64_t>{1};
        return Tensor(out_shape, {m}, dtype_, device_);
    }
    throw std::invalid_argument("Unsupported max dimension reduction");
}

Tensor Tensor::min(int64_t dim, bool keepdims) const {
    if (dim == -1) {
        if (numel_ == 0) return Tensor::zeros({1}, dtype_, device_);
        double m = get_flat(0);
        for (int64_t i = 1; i < numel_; ++i) m = std::min(m, get_flat(i));
        std::vector<int64_t> out_shape = keepdims ? std::vector<int64_t>(ndim(), 1) : std::vector<int64_t>{1};
        return Tensor(out_shape, {m}, dtype_, device_);
    }
    throw std::invalid_argument("Unsupported min dimension reduction");
}

Tensor Tensor::argmax(int64_t dim) const {
    if (dim == -1 || (dim == 1 && ndim() == 2)) {
        if (ndim() == 2 && dim == 1) {
            Tensor res({shape_[0]}, DType::INT64, device_);
            for (int64_t r = 0; r < shape_[0]; ++r) {
                int64_t best_idx = 0;
                double best_val = get({r, 0});
                for (int64_t c = 1; c < shape_[1]; ++c) {
                    double v = get({r, c});
                    if (v > best_val) {
                        best_val = v;
                        best_idx = c;
                    }
                }
                res.set_flat(r, static_cast<double>(best_idx));
            }
            return res;
        }

        int64_t best_idx = 0;
        double best_val = numel_ > 0 ? get_flat(0) : 0.0;
        for (int64_t i = 1; i < numel_; ++i) {
            double v = get_flat(i);
            if (v > best_val) {
                best_val = v;
                best_idx = i;
            }
        }
        return Tensor({1}, {static_cast<double>(best_idx)}, DType::INT64, device_);
    }
    throw std::invalid_argument("Unsupported argmax dimension");
}

// Activations
Tensor Tensor::relu() const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) {
        res.set_flat(i, std::max(0.0, get_flat(i)));
    }
    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<ReLUBackwardNode>(*this);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::sigmoid() const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) {
        double v = get_flat(i);
        res.set_flat(i, 1.0 / (1.0 + std::exp(-v)));
    }
    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<SigmoidBackwardNode>(res);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::tanh() const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) {
        res.set_flat(i, std::tanh(get_flat(i)));
    }
    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<TanhBackwardNode>(res);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

Tensor Tensor::softmax(int64_t dim) const {
    Tensor res(shape_, dtype_, device_);
    if (dim == -1 || dim == 1) {
        if (ndim() == 2) {
            for (int64_t r = 0; r < shape_[0]; ++r) {
                double max_val = get({r, 0});
                for (int64_t c = 1; c < shape_[1]; ++c) max_val = std::max(max_val, get({r, c}));

                double sum_exp = 0.0;
                for (int64_t c = 0; c < shape_[1]; ++c) sum_exp += std::exp(get({r, c}) - max_val);

                for (int64_t c = 0; c < shape_[1]; ++c) {
                    res.set({r, c}, std::exp(get({r, c}) - max_val) / std::max(1e-12, sum_exp));
                }
            }
        } else if (ndim() == 1) {
            double max_val = get_flat(0);
            for (int64_t i = 1; i < numel_; ++i) max_val = std::max(max_val, get_flat(i));

            double sum_exp = 0.0;
            for (int64_t i = 0; i < numel_; ++i) sum_exp += std::exp(get_flat(i) - max_val);

            for (int64_t i = 0; i < numel_; ++i) {
                res.set_flat(i, std::exp(get_flat(i) - max_val) / std::max(1e-12, sum_exp));
            }
        }
    } else {
        throw std::invalid_argument("Unsupported softmax dimension");
    }

    if (AutogradContext::is_grad_enabled() && requires_grad()) {
        res.set_requires_grad(true);
        res.set_is_leaf(false);
        auto node = std::make_shared<SoftmaxBackwardNode>(res, dim);
        node->inputs = {std::make_shared<Tensor>(*this)};
        res.set_grad_fn(node);
    }
    return res;
}

std::string Tensor::to_string() const {
    std::ostringstream ss;
    ss << "tensor(";
    if (ndim() == 1) {
        ss << "[";
        for (int64_t i = 0; i < numel_; ++i) {
            if (i > 0) ss << ", ";
            if (i >= 8 && numel_ > 10) {
                ss << "...";
                break;
            }
            ss << std::fixed << std::setprecision(4) << get_flat(i);
        }
        ss << "]";
    } else if (ndim() == 2) {
        ss << "[\n";
        for (int64_t r = 0; r < shape_[0]; ++r) {
            if (r >= 6 && shape_[0] > 8) {
                ss << "  ...\n";
                break;
            }
            ss << "  [";
            for (int64_t c = 0; c < shape_[1]; ++c) {
                if (c > 0) ss << ", ";
                if (c >= 6 && shape_[1] > 8) {
                    ss << "...";
                    break;
                }
                ss << std::fixed << std::setprecision(4) << get({r, c});
            }
            ss << "]" << (r + 1 < shape_[0] ? ",\n" : "\n");
        }
        ss << "]";
    } else {
        ss << "[data=" << numel_ << " elements]";
    }
    ss << ", shape=[";
    for (size_t i = 0; i < shape_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << shape_[i];
    }
    ss << "], dtype=" << dtype_to_string(dtype_) << ", device=" << device_to_string(device_) << ")";
    return ss.str();
}

Value Tensor::to_value() const {
    auto self_tensor = std::make_shared<Tensor>(*this);
    std::map<std::string, Value> methods;

    methods["$type"] = Value::make_string("Tensor");
    methods["ndim"] = Value::make_int(static_cast<int64_t>(ndim()));
    methods["numel"] = Value::make_int(numel());
    methods["device"] = Value::make_string(device_to_string(device()));
    methods["dtype"] = Value::make_string(dtype_to_string(dtype()));
    methods["requires_grad"] = Value::make_bool(requires_grad());

    std::vector<Value> sh;
    for (int64_t s : shape()) sh.push_back(Value::make_int(s));
    methods["shape"] = Value::make_array(std::move(sh));

    if (grad()) {
        methods["grad"] = grad()->to_value();
    } else {
        methods["grad"] = Value::make_nil();
    }

    methods["set_requires_grad"] = Value::make_native_fn("set_requires_grad", 1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        bool req = args[0].as_bool();
        self_tensor->set_requires_grad(req);
        return Value::make_nil();
    });

    methods["backward"] = Value::make_native_fn("backward", -1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args.empty() && args[0].is_object()) {
            // Optional custom grad_output
            auto obj = args[0].as_object();
            auto to_list = obj->find("to_list");
            if (to_list != obj->end()) {
                Value arr_val = to_list->second.as_native_fn()->func({}, {});
                std::vector<double> vals;
                for (const auto& v : *arr_val.as_array()) vals.push_back(v.as_float());
                std::vector<int64_t> o_shape;
                for (const auto& s : *(*obj)["shape"].as_array()) o_shape.push_back(s.as_int());
                self_tensor->backward(Tensor(o_shape, vals));
                return Value::make_nil();
            }
        }
        self_tensor->backward();
        return Value::make_nil();
    });

    methods["zero_grad"] = Value::make_native_fn("zero_grad", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        self_tensor->zero_grad();
        return Value::make_nil();
    });

    methods["detach"] = Value::make_native_fn("detach", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        return self_tensor->detach().to_value();
    });

    methods["to_string"] = Value::make_native_fn("to_string", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_string(self_tensor->to_string());
    });

    methods["item"] = Value::make_native_fn("item", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_float(self_tensor->item());
    });

    methods["get"] = Value::make_native_fn("get", -1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args.empty()) return Value::make_float(self_tensor->item());
        std::vector<int64_t> idx;
        for (const auto& a : args) idx.push_back(a.as_int());
        return Value::make_float(self_tensor->get(idx));
    });

    methods["to_list"] = Value::make_native_fn("to_list", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        std::vector<Value> arr;
        for (double v : self_tensor->to_vector()) arr.push_back(Value::make_float(v));
        return Value::make_array(std::move(arr));
    });

    methods["reshape"] = Value::make_native_fn("reshape", 1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<int64_t> new_sh;
        if (args[0].is_array()) {
            for (const auto& x : *args[0].as_array()) new_sh.push_back(x.as_int());
        } else {
            new_sh.push_back(args[0].as_int());
        }
        return self_tensor->reshape(new_sh).to_value();
    });

    methods["transpose"] = Value::make_native_fn("transpose", 2, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        return self_tensor->transpose(args[0].as_int(), args[1].as_int()).to_value();
    });

    methods["T"] = Value::make_native_fn("T", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        return self_tensor->T().to_value();
    });

    methods["matmul"] = Value::make_native_fn("matmul", 1, [self_tensor](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_object()) throw RuntimeError("matmul requires a Tensor", span);
        auto other_obj = args[0].as_object();
        auto to_list = other_obj->find("to_list");
        if (to_list == other_obj->end()) throw RuntimeError("Expected Tensor for matmul", span);
        Value arr_val = to_list->second.as_native_fn()->func({}, span);
        std::vector<double> vals;
        for (const auto& v : *arr_val.as_array()) vals.push_back(v.as_float());
        std::vector<int64_t> o_shape;
        for (const auto& s : *(*other_obj)["shape"].as_array()) o_shape.push_back(s.as_int());
        Tensor other(o_shape, vals);
        return self_tensor->matmul(other).to_value();
    });

    methods["add"] = Value::make_native_fn("add", 1, [self_tensor](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_number()) return self_tensor->scalar_add(args[0].as_float()).to_value();
        auto other_obj = args[0].as_object();
        auto to_list = other_obj->find("to_list");
        if (to_list == other_obj->end()) throw RuntimeError("Expected Tensor", span);
        Value arr_val = to_list->second.as_native_fn()->func({}, span);
        std::vector<double> vals;
        for (const auto& v : *arr_val.as_array()) vals.push_back(v.as_float());
        std::vector<int64_t> o_shape;
        for (const auto& s : *(*other_obj)["shape"].as_array()) o_shape.push_back(s.as_int());
        return self_tensor->add(Tensor(o_shape, vals)).to_value();
    });

    methods["sub"] = Value::make_native_fn("sub", 1, [self_tensor](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_number()) return self_tensor->scalar_add(-args[0].as_float()).to_value();
        auto other_obj = args[0].as_object();
        auto to_list = other_obj->find("to_list");
        if (to_list == other_obj->end()) throw RuntimeError("Expected Tensor", span);
        Value arr_val = to_list->second.as_native_fn()->func({}, span);
        std::vector<double> vals;
        for (const auto& v : *arr_val.as_array()) vals.push_back(v.as_float());
        std::vector<int64_t> o_shape;
        for (const auto& s : *(*other_obj)["shape"].as_array()) o_shape.push_back(s.as_int());
        return self_tensor->sub(Tensor(o_shape, vals)).to_value();
    });

    methods["mul"] = Value::make_native_fn("mul", 1, [self_tensor](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_number()) return self_tensor->scalar_mul(args[0].as_float()).to_value();
        auto other_obj = args[0].as_object();
        auto to_list = other_obj->find("to_list");
        if (to_list == other_obj->end()) throw RuntimeError("Expected Tensor", span);
        Value arr_val = to_list->second.as_native_fn()->func({}, span);
        std::vector<double> vals;
        for (const auto& v : *arr_val.as_array()) vals.push_back(v.as_float());
        std::vector<int64_t> o_shape;
        for (const auto& s : *(*other_obj)["shape"].as_array()) o_shape.push_back(s.as_int());
        return self_tensor->mul(Tensor(o_shape, vals)).to_value();
    });

    methods["div"] = Value::make_native_fn("div", 1, [self_tensor](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_number()) return self_tensor->scalar_mul(1.0 / args[0].as_float()).to_value();
        auto other_obj = args[0].as_object();
        auto to_list = other_obj->find("to_list");
        if (to_list == other_obj->end()) throw RuntimeError("Expected Tensor", span);
        Value arr_val = to_list->second.as_native_fn()->func({}, span);
        std::vector<double> vals;
        for (const auto& v : *arr_val.as_array()) vals.push_back(v.as_float());
        std::vector<int64_t> o_shape;
        for (const auto& s : *(*other_obj)["shape"].as_array()) o_shape.push_back(s.as_int());
        return self_tensor->div(Tensor(o_shape, vals)).to_value();
    });

    methods["relu"] = Value::make_native_fn("relu", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        return self_tensor->relu().to_value();
    });

    methods["sigmoid"] = Value::make_native_fn("sigmoid", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        return self_tensor->sigmoid().to_value();
    });

    methods["tanh"] = Value::make_native_fn("tanh", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        return self_tensor->tanh().to_value();
    });

    methods["softmax"] = Value::make_native_fn("softmax", -1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t dim = args.empty() ? -1 : args[0].as_int();
        return self_tensor->softmax(dim).to_value();
    });

    methods["sum"] = Value::make_native_fn("sum", -1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t dim = args.empty() ? -1 : args[0].as_int();
        return self_tensor->sum(dim).to_value();
    });

    methods["mean"] = Value::make_native_fn("mean", -1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t dim = args.empty() ? -1 : args[0].as_int();
        return self_tensor->mean(dim).to_value();
    });

    methods["argmax"] = Value::make_native_fn("argmax", -1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t dim = args.empty() ? -1 : args[0].as_int();
        return self_tensor->argmax(dim).to_value();
    });

    methods["to_list"] = Value::make_native_fn("to_list", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        auto vec = self_tensor->to_vector();
        std::vector<Value> res;
        for (double v : vec) res.push_back(Value::make_float(v));
        return Value::make_array(std::move(res));
    });

    methods["to"] = Value::make_native_fn("to", 1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        Device d = string_to_device(args[0].as_string());
        return self_tensor->to(d).to_value();
    });

    return Value::make_object(std::move(methods));
}

} // namespace nextviper
