#include "nextviper/tensor.hpp"
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
    numel_ = 1;
    for (int64_t dim : shape_) {
        if (dim < 0) throw std::invalid_argument("Negative dimension size in Tensor");
        numel_ *= dim;
    }
    compute_strides();
    size_t bytes = numel_ * dtype_size(dtype_);
    data_ = CPUTensorBackend::instance().allocate(bytes);
    CPUTensorBackend::instance().fill(data_.get(), numel_, 0.0, dtype_);
}

Tensor::Tensor(std::vector<int64_t> shape, const std::vector<double>& values, DType dtype, Device device)
    : shape_(std::move(shape)), dtype_(dtype), device_(device), is_contiguous_(true) {
    numel_ = 1;
    for (int64_t dim : shape_) numel_ *= dim;
    compute_strides();
    size_t bytes = numel_ * dtype_size(dtype_);
    data_ = CPUTensorBackend::instance().allocate(bytes);

    size_t count = std::min<size_t>(numel_, values.size());
    for (size_t i = 0; i < count; ++i) {
        set_flat(i, values[i]);
    }
    for (size_t i = count; i < static_cast<size_t>(numel_); ++i) {
        set_flat(i, 0.0);
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
    CPUTensorBackend::instance().fill(t.data(), t.numel(), 1.0, dtype);
    return t;
}

Tensor Tensor::full(const std::vector<int64_t>& shape, double val, DType dtype, Device device) {
    Tensor t(shape, dtype, device);
    CPUTensorBackend::instance().fill(t.data(), t.numel(), val, dtype);
    return t;
}

Tensor Tensor::randn(const std::vector<int64_t>& shape, double mean, double stddev, Device device) {
    Tensor t(shape, DType::FLOAT32, device);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(mean, stddev);

    for (int64_t i = 0; i < t.numel(); ++i) {
        t.set_flat(i, dist(gen));
    }
    return t;
}

Tensor Tensor::uniform(const std::vector<int64_t>& shape, double low, double high, Device device) {
    Tensor t(shape, DType::FLOAT32, device);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(low, high);

    for (int64_t i = 0; i < t.numel(); ++i) {
        t.set_flat(i, dist(gen));
    }
    return t;
}

double Tensor::get_flat(size_t index) const {
    if (index >= static_cast<size_t>(numel_)) throw std::out_of_range("Tensor index out of range");
    if (dtype_ == DType::FLOAT32) return static_cast<const float*>(data_.get())[index];
    if (dtype_ == DType::FLOAT64) return static_cast<const double*>(data_.get())[index];
    if (dtype_ == DType::INT32) return static_cast<const int32_t*>(data_.get())[index];
    if (dtype_ == DType::INT64) return static_cast<const int64_t*>(data_.get())[index];
    return 0.0;
}

void Tensor::set_flat(size_t index, double val) {
    if (index >= static_cast<size_t>(numel_)) throw std::out_of_range("Tensor index out of range");
    if (dtype_ == DType::FLOAT32) static_cast<float*>(data_.get())[index] = static_cast<float>(val);
    else if (dtype_ == DType::FLOAT64) static_cast<double*>(data_.get())[index] = val;
    else if (dtype_ == DType::INT32) static_cast<int32_t*>(data_.get())[index] = static_cast<int32_t>(val);
    else if (dtype_ == DType::INT64) static_cast<int64_t*>(data_.get())[index] = static_cast<int64_t>(val);
}

size_t Tensor::compute_offset(const std::vector<int64_t>& indices) const {
    if (indices.size() != shape_.size()) throw std::invalid_argument("Indices size mismatch with Tensor ndim");
    size_t off = 0;
    for (size_t i = 0; i < indices.size(); ++i) {
        int64_t idx = indices[i];
        if (idx < 0) idx += shape_[i];
        if (idx < 0 || idx >= shape_[i]) throw std::out_of_range("Tensor index out of bounds");
        off += idx * strides_[i];
    }
    return off;
}

double Tensor::get(const std::vector<int64_t>& indices) const {
    return get_flat(compute_offset(indices));
}

void Tensor::set(const std::vector<int64_t>& indices, double val) {
    set_flat(compute_offset(indices), val);
}

std::vector<double> Tensor::to_vector() const {
    std::vector<double> vec(numel_);
    for (int64_t i = 0; i < numel_; ++i) {
        vec[i] = get_flat(i);
    }
    return vec;
}

double Tensor::item() const {
    if (numel_ != 1) throw std::runtime_error("item() only valid for single-element Tensors");
    return get_flat(0);
}

Tensor Tensor::to(Device dev) const {
    Tensor res = clone();
    res.device_ = dev;
    return res;
}

Tensor Tensor::clone() const {
    Tensor res(shape_, dtype_, device_);
    size_t bytes = numel_ * dtype_size(dtype_);
    CPUTensorBackend::instance().copy(res.data(), data(), bytes);
    return res;
}

Tensor Tensor::reshape(const std::vector<int64_t>& new_shape) const {
    int64_t new_numel = 1;
    int auto_dim = -1;
    for (size_t i = 0; i < new_shape.size(); ++i) {
        if (new_shape[i] == -1) {
            if (auto_dim != -1) throw std::invalid_argument("Only one dimension can be -1 in reshape");
            auto_dim = static_cast<int>(i);
        } else {
            new_numel *= new_shape[i];
        }
    }

    std::vector<int64_t> final_shape = new_shape;
    if (auto_dim != -1) {
        if (numel_ % new_numel != 0) throw std::invalid_argument("Cannot deduce -1 shape in reshape");
        final_shape[auto_dim] = numel_ / new_numel;
    } else if (new_numel != numel_) {
        throw std::invalid_argument("Total element count must match in reshape");
    }

    Tensor res = clone();
    res.shape_ = final_shape;
    res.compute_strides();
    return res;
}

Tensor Tensor::transpose(int64_t dim0, int64_t dim1) const {
    if (dim0 < 0) dim0 += shape_.size();
    if (dim1 < 0) dim1 += shape_.size();
    if (dim0 < 0 || dim0 >= static_cast<int64_t>(shape_.size()) ||
        dim1 < 0 || dim1 >= static_cast<int64_t>(shape_.size())) {
        throw std::out_of_range("Transpose dimension out of range");
    }

    std::vector<int64_t> new_shape = shape_;
    std::swap(new_shape[dim0], new_shape[dim1]);

    Tensor res(new_shape, dtype_, device_);
    if (shape_.size() == 2) {
        for (int64_t r = 0; r < shape_[0]; ++r) {
            for (int64_t c = 0; c < shape_[1]; ++c) {
                res.set({c, r}, get({r, c}));
            }
        }
    } else {
        // Multi-dim transpose
        std::vector<int64_t> idx(shape_.size(), 0);
        for (int64_t i = 0; i < numel_; ++i) {
            std::vector<int64_t> t_idx = idx;
            std::swap(t_idx[dim0], t_idx[dim1]);
            res.set(t_idx, get(idx));

            // increment index
            for (int d = static_cast<int>(shape_.size()) - 1; d >= 0; --d) {
                if (++idx[d] < shape_[d]) break;
                idx[d] = 0;
            }
        }
    }
    return res;
}

Tensor Tensor::T() const {
    if (shape_.size() != 2) throw std::runtime_error(".T only valid for 2D tensors");
    return transpose(0, 1);
}

Tensor Tensor::flatten() const {
    return reshape({numel_});
}

// Elementwise operations
Tensor Tensor::add(const Tensor& other) const {
    if (shape_ == other.shape_) {
        Tensor res(shape_, dtype_, device_);
        for (int64_t i = 0; i < numel_; ++i) {
            res.set_flat(i, get_flat(i) + other.get_flat(i));
        }
        return res;
    }
    // 2D row/col broadcasting
    if (shape_.size() == 2 && other.shape_.size() == 1 && shape_[1] == other.shape_[0]) {
        Tensor res(shape_, dtype_, device_);
        for (int64_t r = 0; r < shape_[0]; ++r) {
            for (int64_t c = 0; c < shape_[1]; ++c) {
                res.set({r, c}, get({r, c}) + other.get_flat(c));
            }
        }
        return res;
    }
    throw std::invalid_argument("Tensor shape mismatch in add");
}

Tensor Tensor::sub(const Tensor& other) const {
    if (shape_ == other.shape_) {
        Tensor res(shape_, dtype_, device_);
        for (int64_t i = 0; i < numel_; ++i) {
            res.set_flat(i, get_flat(i) - other.get_flat(i));
        }
        return res;
    }
    if (shape_.size() == 2 && other.shape_.size() == 1 && shape_[1] == other.shape_[0]) {
        Tensor res(shape_, dtype_, device_);
        for (int64_t r = 0; r < shape_[0]; ++r) {
            for (int64_t c = 0; c < shape_[1]; ++c) {
                res.set({r, c}, get({r, c}) - other.get_flat(c));
            }
        }
        return res;
    }
    throw std::invalid_argument("Tensor shape mismatch in sub");
}

Tensor Tensor::mul(const Tensor& other) const {
    if (shape_ == other.shape_) {
        Tensor res(shape_, dtype_, device_);
        for (int64_t i = 0; i < numel_; ++i) {
            res.set_flat(i, get_flat(i) * other.get_flat(i));
        }
        return res;
    }
    throw std::invalid_argument("Tensor shape mismatch in mul");
}

Tensor Tensor::div(const Tensor& other) const {
    if (shape_ == other.shape_) {
        Tensor res(shape_, dtype_, device_);
        for (int64_t i = 0; i < numel_; ++i) {
            double v = other.get_flat(i);
            res.set_flat(i, v == 0.0 ? 0.0 : get_flat(i) / v);
        }
        return res;
    }
    throw std::invalid_argument("Tensor shape mismatch in div");
}

Tensor Tensor::scalar_add(double scalar) const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) res.set_flat(i, get_flat(i) + scalar);
    return res;
}

Tensor Tensor::scalar_mul(double scalar) const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) res.set_flat(i, get_flat(i) * scalar);
    return res;
}

Tensor Tensor::matmul(const Tensor& other) const {
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

        Tensor res({M, N}, dtype_, device_);
        for (int64_t i = 0; i < M; ++i) {
            for (int64_t k = 0; k < K; ++k) {
                double a_ik = get({i, k});
                for (int64_t j = 0; j < N; ++j) {
                    res.set({i, j}, res.get({i, j}) + a_ik * other.get({k, j}));
                }
            }
        }
        return res;
    }

    if (ndim() == 1 && other.ndim() == 2) {
        Tensor a = reshape({1, shape_[0]});
        Tensor res = a.matmul(other);
        return res.reshape({other.shape_[1]});
    }

    if (ndim() == 2 && other.ndim() == 1) {
        Tensor b = other.reshape({other.shape_[0], 1});
        Tensor res = matmul(b);
        return res.reshape({shape_[0]});
    }

    throw std::invalid_argument("matmul supports 1D and 2D tensors");
}

// Reductions
Tensor Tensor::sum(int64_t dim, bool keepdims) const {
    if (dim == -1) {
        double s = 0.0;
        for (int64_t i = 0; i < numel_; ++i) s += get_flat(i);
        std::vector<int64_t> out_shape = keepdims ? std::vector<int64_t>(ndim(), 1) : std::vector<int64_t>{1};
        return Tensor(out_shape, {s}, dtype_, device_);
    }

    if (dim < 0) dim += ndim();
    if (dim == 0 && ndim() == 2) {
        Tensor res(keepdims ? std::vector<int64_t>{1, shape_[1]} : std::vector<int64_t>{shape_[1]}, dtype_, device_);
        for (int64_t c = 0; c < shape_[1]; ++c) {
            double s = 0.0;
            for (int64_t r = 0; r < shape_[0]; ++r) s += get({r, c});
            res.set_flat(c, s);
        }
        return res;
    }
    if (dim == 1 && ndim() == 2) {
        Tensor res(keepdims ? std::vector<int64_t>{shape_[0], 1} : std::vector<int64_t>{shape_[0]}, dtype_, device_);
        for (int64_t r = 0; r < shape_[0]; ++r) {
            double s = 0.0;
            for (int64_t c = 0; c < shape_[1]; ++c) s += get({r, c});
            res.set_flat(r, s);
        }
        return res;
    }
    throw std::invalid_argument("Unsupported sum dimension reduction");
}

Tensor Tensor::mean(int64_t dim, bool keepdims) const {
    Tensor s = sum(dim, keepdims);
    double count = (dim == -1) ? static_cast<double>(numel_) : static_cast<double>(shape_[dim < 0 ? dim + ndim() : dim]);
    return s.scalar_mul(1.0 / std::max(1.0, count));
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
    return res;
}

Tensor Tensor::sigmoid() const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) {
        double v = get_flat(i);
        res.set_flat(i, 1.0 / (1.0 + std::exp(-v)));
    }
    return res;
}

Tensor Tensor::tanh() const {
    Tensor res(shape_, dtype_, device_);
    for (int64_t i = 0; i < numel_; ++i) {
        res.set_flat(i, std::tanh(get_flat(i)));
    }
    return res;
}

Tensor Tensor::softmax(int64_t dim) const {
    if (dim == -1 || dim == 1) {
        if (ndim() == 2) {
            Tensor res(shape_, dtype_, device_);
            for (int64_t r = 0; r < shape_[0]; ++r) {
                double max_val = get({r, 0});
                for (int64_t c = 1; c < shape_[1]; ++c) max_val = std::max(max_val, get({r, c}));

                double sum_exp = 0.0;
                for (int64_t c = 0; c < shape_[1]; ++c) sum_exp += std::exp(get({r, c}) - max_val);

                for (int64_t c = 0; c < shape_[1]; ++c) {
                    res.set({r, c}, std::exp(get({r, c}) - max_val) / sum_exp);
                }
            }
            return res;
        }

        if (ndim() == 1) {
            Tensor res(shape_, dtype_, device_);
            double max_val = get_flat(0);
            for (int64_t i = 1; i < numel_; ++i) max_val = std::max(max_val, get_flat(i));

            double sum_exp = 0.0;
            for (int64_t i = 0; i < numel_; ++i) sum_exp += std::exp(get_flat(i) - max_val);

            for (int64_t i = 0; i < numel_; ++i) {
                res.set_flat(i, std::exp(get_flat(i) - max_val) / sum_exp);
            }
            return res;
        }
    }
    throw std::invalid_argument("Unsupported softmax dimension");
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

    std::vector<Value> sh;
    for (int64_t s : shape()) sh.push_back(Value::make_int(s));
    methods["shape"] = Value::make_array(std::move(sh));

    methods["to_string"] = Value::make_native_fn("to_string", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_string(self_tensor->to_string());
    });

    methods["item"] = Value::make_native_fn("item", 0, [self_tensor](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_float(self_tensor->item());
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
        // Extract native tensor from Value if possible or via vector
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

    methods["mul"] = Value::make_native_fn("mul", 1, [self_tensor](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_number()) return self_tensor->scalar_mul(args[0].as_float()).to_value();
        return self_tensor->scalar_mul(1.0).to_value();
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
