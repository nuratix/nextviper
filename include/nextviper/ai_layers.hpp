#pragma once

#include "nextviper/tensor.hpp"
#include "nextviper/value.hpp"
#include <string>
#include <vector>
#include <memory>

namespace nextviper {

// Parameter wrapper for trainable tensors
class Parameter {
public:
    Parameter(std::string name, Tensor data, bool trainable = true);

    const std::string& name() const { return name_; }
    Tensor& data() { return data_; }
    const Tensor& data() const { return data_; }
    bool is_trainable() const { return trainable_; }
    void set_trainable(bool t) { trainable_ = t; }

    std::shared_ptr<Tensor> grad() const { return data_.grad(); }
    void zero_grad() { data_.zero_grad(); }

private:
    std::string name_;
    Tensor data_;
    bool trainable_;
};

// Base Module / Layer class
class Module {
public:
    virtual ~Module() = default;
    virtual std::string type_name() const = 0;
    virtual std::string name() const { return type_name(); }

    virtual Tensor forward(const Tensor& input) = 0;

    virtual std::vector<std::shared_ptr<Parameter>> parameters();
    virtual std::vector<std::shared_ptr<Parameter>> trainable_parameters();

    virtual void train(bool mode = true) { training_ = mode; }
    virtual void eval() { train(false); }
    bool is_training() const { return training_; }

    virtual void zero_grad();

    virtual size_t count_parameters() const;

    virtual std::string to_json() const;
    virtual void from_json(const std::string& json_str);

    virtual Value to_value();

protected:
    bool training_ = true;
    std::vector<std::shared_ptr<Parameter>> params_;
};

// ============================================================================
// Core Layers
// ============================================================================

class Dense : public Module {
public:
    Dense(int64_t in_features, int64_t out_features, bool has_bias = true, const std::string& activation = "none");
    Dense(int64_t out_features, const std::string& activation = "none"); // Lazily inferred in_features

    std::string type_name() const override { return "Dense"; }

    Tensor forward(const Tensor& input) override;

    int64_t in_features() const { return in_features_; }
    int64_t out_features() const { return out_features_; }
    bool has_bias() const { return has_bias_; }
    const std::string& activation_name() const { return activation_name_; }

    std::shared_ptr<Parameter> weight_param() const { return weight_param_; }
    std::shared_ptr<Parameter> bias_param() const { return bias_param_; }

    void initialize_weights(int64_t in_features);

    std::string to_json() const override;
    void from_json(const std::string& json_str) override;

private:
    int64_t in_features_ = 0;
    int64_t out_features_ = 0;
    bool has_bias_ = true;
    std::string activation_name_ = "none";
    bool initialized_ = false;

    std::shared_ptr<Parameter> weight_param_;
    std::shared_ptr<Parameter> bias_param_;
};

class Dropout : public Module {
public:
    explicit Dropout(double rate = 0.5);

    std::string type_name() const override { return "Dropout"; }
    Tensor forward(const Tensor& input) override;
    double rate() const { return rate_; }

    std::string to_json() const override;

private:
    double rate_;
};

class Flatten : public Module {
public:
    Flatten(int64_t start_dim = 1, int64_t end_dim = -1);

    std::string type_name() const override { return "Flatten"; }
    Tensor forward(const Tensor& input) override;

    std::string to_json() const override;

private:
    int64_t start_dim_;
    int64_t end_dim_;
};

class ReLULayer : public Module {
public:
    std::string type_name() const override { return "ReLU"; }
    Tensor forward(const Tensor& input) override;
};

class SigmoidLayer : public Module {
public:
    std::string type_name() const override { return "Sigmoid"; }
    Tensor forward(const Tensor& input) override;
};

class TanhLayer : public Module {
public:
    std::string type_name() const override { return "Tanh"; }
    Tensor forward(const Tensor& input) override;
};

class SoftmaxLayer : public Module {
public:
    explicit SoftmaxLayer(int64_t dim = -1) : dim_(dim) {}
    std::string type_name() const override { return "Softmax"; }
    Tensor forward(const Tensor& input) override;
private:
    int64_t dim_;
};

} // namespace nextviper
