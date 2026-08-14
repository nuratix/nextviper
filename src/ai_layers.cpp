#include "nextviper/ai_layers.hpp"
#include <cmath>
#include <random>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace nextviper {

// ============================================================================
// Parameter Implementation
// ============================================================================

Parameter::Parameter(std::string name, Tensor data, bool trainable)
    : name_(std::move(name)), data_(std::move(data)), trainable_(trainable) {
    if (trainable_) {
        data_.set_requires_grad(true);
        data_.set_is_leaf(true);
    }
}

// ============================================================================
// Module Base Class Implementation
// ============================================================================

std::vector<std::shared_ptr<Parameter>> Module::parameters() {
    return params_;
}

std::vector<std::shared_ptr<Parameter>> Module::trainable_parameters() {
    std::vector<std::shared_ptr<Parameter>> res;
    for (const auto& p : parameters()) {
        if (p && p->is_trainable()) res.push_back(p);
    }
    return res;
}

void Module::zero_grad() {
    for (auto& p : parameters()) {
        if (p) p->zero_grad();
    }
}

size_t Module::count_parameters() const {
    size_t total = 0;
    for (const auto& p : const_cast<Module*>(this)->parameters()) {
        if (p) total += static_cast<size_t>(p->data().numel());
    }
    return total;
}

std::string Module::to_json() const {
    return "{\"type\": \"" + type_name() + "\"}";
}

void Module::from_json(const std::string&) {}

Value Module::to_value() {
    std::map<std::string, Value> obj;
    obj["$type"] = Value::make_string(type_name());
    obj["name"] = Value::make_string(name());
    obj["params_count"] = Value::make_int(static_cast<int64_t>(count_parameters()));
    return Value::make_object(std::move(obj));
}

// ============================================================================
// Dense Layer Implementation
// ============================================================================

Dense::Dense(int64_t in_features, int64_t out_features, bool has_bias, const std::string& activation)
    : in_features_(in_features), out_features_(out_features), has_bias_(has_bias), activation_name_(activation), initialized_(false) {
    if (in_features_ > 0 && out_features_ > 0) {
        initialize_weights(in_features_);
    }
}

Dense::Dense(int64_t out_features, const std::string& activation)
    : in_features_(0), out_features_(out_features), has_bias_(true), activation_name_(activation), initialized_(false) {}

void Dense::initialize_weights(int64_t in_features) {
    in_features_ = in_features;
    // Xavier / Glorot uniform initialization
    double limit = std::sqrt(6.0 / static_cast<double>(in_features_ + out_features_));
    Tensor w = Tensor::uniform({out_features_, in_features_}, -limit, limit);
    w.set_requires_grad(true);
    weight_param_ = std::make_shared<Parameter>("weight", std::move(w), true);
    params_.push_back(weight_param_);

    if (has_bias_) {
        Tensor b = Tensor::zeros({out_features_});
        b.set_requires_grad(true);
        bias_param_ = std::make_shared<Parameter>("bias", std::move(b), true);
        params_.push_back(bias_param_);
    }
    initialized_ = true;
}

Tensor Dense::forward(const Tensor& input) {
    if (!initialized_) {
        int64_t inferred_in = input.ndim() >= 2 ? input.shape()[1] : input.shape()[0];
        initialize_weights(inferred_in);
    }

    Tensor x = input;
    if (input.ndim() == 1) {
        x = input.reshape({1, input.shape()[0]});
    }

    // Z = X @ W.T
    Tensor z = x.matmul(weight_param_->data().T());

    // Add bias with broadcasting
    if (has_bias_ && bias_param_) {
        z = z.add(bias_param_->data());
    }

    // Apply activation if any
    Tensor out = z;
    if (activation_name_ == "relu" || activation_name_ == "ReLU") {
        out = z.relu();
    } else if (activation_name_ == "sigmoid" || activation_name_ == "Sigmoid") {
        out = z.sigmoid();
    } else if (activation_name_ == "tanh" || activation_name_ == "Tanh") {
        out = z.tanh();
    } else if (activation_name_ == "softmax" || activation_name_ == "Softmax") {
        out = z.softmax(1);
    }

    if (input.ndim() == 1) {
        return out.reshape({out_features_});
    }
    return out;
}

std::string Dense::to_json() const {
    std::ostringstream ss;
    ss << "{\"type\": \"Dense\", \"in_features\": " << in_features_
       << ", \"out_features\": " << out_features_
       << ", \"has_bias\": " << (has_bias_ ? "true" : "false")
       << ", \"activation\": \"" << activation_name_ << "\"}";
    return ss.str();
}

void Dense::from_json(const std::string&) {}

// ============================================================================
// Dropout Layer Implementation
// ============================================================================

Dropout::Dropout(double rate) : rate_(std::clamp(rate, 0.0, 0.999)) {}

Tensor Dropout::forward(const Tensor& input) {
    if (!training_ || rate_ <= 0.0) {
        return input;
    }
    double keep_prob = 1.0 - rate_;
    double scale = 1.0 / keep_prob;
    Tensor mask = Tensor::zeros(input.shape(), input.dtype(), input.device());

    std::random_device rd;
    std::mt19937 gen(rd());
    std::bernoulli_distribution dist(keep_prob);

    for (int64_t i = 0; i < input.numel(); ++i) {
        mask.set_flat(i, dist(gen) ? scale : 0.0);
    }
    return input.mul(mask);
}

std::string Dropout::to_json() const {
    return "{\"type\": \"Dropout\", \"rate\": " + std::to_string(rate_) + "}";
}

// ============================================================================
// Flatten Layer Implementation
// ============================================================================

Flatten::Flatten(int64_t start_dim, int64_t end_dim)
    : start_dim_(start_dim), end_dim_(end_dim) {}

Tensor Flatten::forward(const Tensor& input) {
    if (input.ndim() <= 1) return input;
    int64_t batch = input.shape()[0];
    int64_t feat = input.numel() / std::max<int64_t>(1, batch);
    return input.reshape({batch, feat});
}

std::string Flatten::to_json() const {
    return "{\"type\": \"Flatten\", \"start_dim\": " + std::to_string(start_dim_) + ", \"end_dim\": " + std::to_string(end_dim_) + "}";
}

// ============================================================================
// Activation Layers Implementations
// ============================================================================

Tensor ReLULayer::forward(const Tensor& input) {
    return input.relu();
}

Tensor SigmoidLayer::forward(const Tensor& input) {
    return input.sigmoid();
}

Tensor TanhLayer::forward(const Tensor& input) {
    return input.tanh();
}

Tensor SoftmaxLayer::forward(const Tensor& input) {
    return input.softmax(dim_);
}

} // namespace nextviper
