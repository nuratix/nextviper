#include "nextviper/ai_optimizer.hpp"
#include <cmath>
#include <iostream>

namespace nextviper {

// ============================================================================
// Optimizer Base Class
// ============================================================================

Optimizer::Optimizer(std::vector<std::shared_ptr<Parameter>> params, double lr, double weight_decay)
    : params_(std::move(params)), lr_(lr), weight_decay_(weight_decay), step_count_(0) {}

void Optimizer::zero_grad() {
    for (auto& p : params_) {
        if (p) p->zero_grad();
    }
}

Value Optimizer::to_value() {
    auto self_opt = std::make_shared<Optimizer*>(this);
    std::map<std::string, Value> obj;
    obj["$type"] = Value::make_string("Optimizer");
    obj["name"] = Value::make_string(name());
    obj["lr"] = Value::make_float(lr_);
    obj["weight_decay"] = Value::make_float(weight_decay_);
    return Value::make_object(std::move(obj));
}

// ============================================================================
// SGD Implementation
// ============================================================================

SGD::SGD(std::vector<std::shared_ptr<Parameter>> params, double lr, double momentum, double weight_decay)
    : Optimizer(std::move(params), lr, weight_decay), momentum_(momentum) {}

void SGD::step() {
    step_count_++;
    if (velocity_.size() != params_.size()) {
        velocity_.clear();
        for (const auto& p : params_) {
            velocity_.push_back(Tensor::zeros(p->data().shape(), p->data().dtype(), p->data().device()));
        }
    }

    for (size_t i = 0; i < params_.size(); ++i) {
        auto& p = params_[i];
        if (!p || !p->grad()) continue;

        Tensor& data = p->data();
        const Tensor& grad = *p->grad();
        Tensor& v = velocity_[i];

        for (int64_t j = 0; j < data.numel(); ++j) {
            double g = grad.get_flat(j);
            double theta = data.get_flat(j);

            if (weight_decay_ != 0.0) {
                g += weight_decay_ * theta;
            }

            if (momentum_ > 0.0) {
                v.set_flat(j, momentum_ * v.get_flat(j) + g);
                data.set_flat(j, theta - lr_ * v.get_flat(j));
            } else {
                data.set_flat(j, theta - lr_ * g);
            }
        }
    }
}

// ============================================================================
// Momentum Implementation
// ============================================================================

MomentumOptimizer::MomentumOptimizer(std::vector<std::shared_ptr<Parameter>> params, double lr, double momentum, double weight_decay)
    : Optimizer(std::move(params), lr, weight_decay), momentum_(momentum) {}

void MomentumOptimizer::step() {
    step_count_++;
    if (velocity_.size() != params_.size()) {
        velocity_.clear();
        for (const auto& p : params_) {
            velocity_.push_back(Tensor::zeros(p->data().shape(), p->data().dtype(), p->data().device()));
        }
    }

    for (size_t i = 0; i < params_.size(); ++i) {
        auto& p = params_[i];
        if (!p || !p->grad()) continue;

        Tensor& data = p->data();
        const Tensor& grad = *p->grad();
        Tensor& v = velocity_[i];

        for (int64_t j = 0; j < data.numel(); ++j) {
            double g = grad.get_flat(j);
            double theta = data.get_flat(j);

            if (weight_decay_ != 0.0) g += weight_decay_ * theta;
            v.set_flat(j, momentum_ * v.get_flat(j) + g);
            data.set_flat(j, theta - lr_ * v.get_flat(j));
        }
    }
}

// ============================================================================
// Adam Implementation
// ============================================================================

Adam::Adam(std::vector<std::shared_ptr<Parameter>> params, double lr, double beta1, double beta2, double eps, double weight_decay)
    : Optimizer(std::move(params), lr, weight_decay), beta1_(beta1), beta2_(beta2), eps_(eps) {}

void Adam::step() {
    step_count_++;
    if (m_.size() != params_.size()) {
        m_.clear();
        v_.clear();
        for (const auto& p : params_) {
            m_.push_back(Tensor::zeros(p->data().shape(), p->data().dtype(), p->data().device()));
            v_.push_back(Tensor::zeros(p->data().shape(), p->data().dtype(), p->data().device()));
        }
    }

    double bias_correction1 = 1.0 - std::pow(beta1_, step_count_);
    double bias_correction2 = 1.0 - std::pow(beta2_, step_count_);

    for (size_t i = 0; i < params_.size(); ++i) {
        auto& p = params_[i];
        if (!p || !p->grad()) continue;

        Tensor& data = p->data();
        const Tensor& grad = *p->grad();
        Tensor& m = m_[i];
        Tensor& v = v_[i];

        for (int64_t j = 0; j < data.numel(); ++j) {
            double g = grad.get_flat(j);
            double theta = data.get_flat(j);

            if (weight_decay_ != 0.0) {
                g += weight_decay_ * theta;
            }

            double m_new = beta1_ * m.get_flat(j) + (1.0 - beta1_) * g;
            double v_new = beta2_ * v.get_flat(j) + (1.0 - beta2_) * g * g;

            m.set_flat(j, m_new);
            v.set_flat(j, v_new);

            double m_hat = m_new / bias_correction1;
            double v_hat = v_new / bias_correction2;

            data.set_flat(j, theta - lr_ * m_hat / (std::sqrt(v_hat) + eps_));
        }
    }
}

// ============================================================================
// AdamW Implementation
// ============================================================================

AdamW::AdamW(std::vector<std::shared_ptr<Parameter>> params, double lr, double beta1, double beta2, double eps, double weight_decay)
    : Optimizer(std::move(params), lr, weight_decay), beta1_(beta1), beta2_(beta2), eps_(eps) {}

void AdamW::step() {
    step_count_++;
    if (m_.size() != params_.size()) {
        m_.clear();
        v_.clear();
        for (const auto& p : params_) {
            m_.push_back(Tensor::zeros(p->data().shape(), p->data().dtype(), p->data().device()));
            v_.push_back(Tensor::zeros(p->data().shape(), p->data().dtype(), p->data().device()));
        }
    }

    double bias_correction1 = 1.0 - std::pow(beta1_, step_count_);
    double bias_correction2 = 1.0 - std::pow(beta2_, step_count_);

    for (size_t i = 0; i < params_.size(); ++i) {
        auto& p = params_[i];
        if (!p || !p->grad()) continue;

        Tensor& data = p->data();
        const Tensor& grad = *p->grad();
        Tensor& m = m_[i];
        Tensor& v = v_[i];

        for (int64_t j = 0; j < data.numel(); ++j) {
            double g = grad.get_flat(j);
            double theta = data.get_flat(j);

            // Decoupled weight decay
            if (weight_decay_ != 0.0) {
                theta -= lr_ * weight_decay_ * theta;
            }

            double m_new = beta1_ * m.get_flat(j) + (1.0 - beta1_) * g;
            double v_new = beta2_ * v.get_flat(j) + (1.0 - beta2_) * g * g;

            m.set_flat(j, m_new);
            v.set_flat(j, v_new);

            double m_hat = m_new / bias_correction1;
            double v_hat = v_new / bias_correction2;

            data.set_flat(j, theta - lr_ * m_hat / (std::sqrt(v_hat) + eps_));
        }
    }
}

} // namespace nextviper
