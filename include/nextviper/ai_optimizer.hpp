#pragma once

#include "nextviper/ai_layers.hpp"
#include "nextviper/value.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace nextviper {

class Optimizer {
public:
    explicit Optimizer(std::vector<std::shared_ptr<Parameter>> params, double lr = 0.001, double weight_decay = 0.0);
    virtual ~Optimizer() = default;

    virtual std::string name() const = 0;
    virtual void step() = 0;
    virtual void zero_grad();

    double lr() const { return lr_; }
    void set_lr(double lr) { lr_ = lr; }
    double weight_decay() const { return weight_decay_; }
    void set_weight_decay(double wd) { weight_decay_ = wd; }

    const std::vector<std::shared_ptr<Parameter>>& parameters() const { return params_; }
    void set_parameters(std::vector<std::shared_ptr<Parameter>> params) { params_ = std::move(params); }

    virtual Value to_value();

protected:
    std::vector<std::shared_ptr<Parameter>> params_;
    double lr_;
    double weight_decay_;
    int64_t step_count_ = 0;
};

class SGD : public Optimizer {
public:
    explicit SGD(std::vector<std::shared_ptr<Parameter>> params = {},
                 double lr = 0.01,
                 double momentum = 0.0,
                 double weight_decay = 0.0);

    std::string name() const override { return "SGD"; }
    void step() override;

private:
    double momentum_;
    std::vector<Tensor> velocity_;
};

class MomentumOptimizer : public Optimizer {
public:
    explicit MomentumOptimizer(std::vector<std::shared_ptr<Parameter>> params = {},
                               double lr = 0.01,
                               double momentum = 0.9,
                               double weight_decay = 0.0);

    std::string name() const override { return "Momentum"; }
    void step() override;

private:
    double momentum_;
    std::vector<Tensor> velocity_;
};

class Adam : public Optimizer {
public:
    explicit Adam(std::vector<std::shared_ptr<Parameter>> params = {},
                  double lr = 0.001,
                  double beta1 = 0.9,
                  double beta2 = 0.999,
                  double eps = 1e-8,
                  double weight_decay = 0.0);

    std::string name() const override { return "Adam"; }
    void step() override;

private:
    double beta1_;
    double beta2_;
    double eps_;
    std::vector<Tensor> m_; // 1st moment vector
    std::vector<Tensor> v_; // 2nd moment vector
};

class AdamW : public Optimizer {
public:
    explicit AdamW(std::vector<std::shared_ptr<Parameter>> params = {},
                   double lr = 0.001,
                   double beta1 = 0.9,
                   double beta2 = 0.999,
                   double eps = 1e-8,
                   double weight_decay = 0.01);

    std::string name() const override { return "AdamW"; }
    void step() override;

private:
    double beta1_;
    double beta2_;
    double eps_;
    std::vector<Tensor> m_;
    std::vector<Tensor> v_;
};

} // namespace nextviper
