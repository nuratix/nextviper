#pragma once

#include "nextviper/common.hpp"
#include "nextviper/value.hpp"
#include "nextviper/tensor.hpp"
#include "nextviper/dataset.hpp"
#include <string>
#include <vector>
#include <memory>

namespace nextviper {

enum class ActivationKind {
    NONE,
    RELU,
    SIGMOID,
    TANH,
    SOFTMAX
};

class Layer {
public:
    virtual ~Layer() = default;
    virtual std::string type_name() const = 0;
    virtual Tensor forward(const Tensor& input) = 0;
    virtual Tensor backward(const Tensor& grad_output, double lr) = 0;
    virtual std::string serialize() const = 0;
};

class LinearLayer : public Layer {
public:
    LinearLayer(int64_t in_features, int64_t out_features, bool has_bias = true);

    std::string type_name() const override { return "Linear"; }
    Tensor forward(const Tensor& input) override;
    Tensor backward(const Tensor& grad_output, double lr) override;
    std::string serialize() const override;

    int64_t in_features() const { return in_features_; }
    int64_t out_features() const { return out_features_; }
    const Tensor& weights() const { return weights_; }
    const Tensor& bias() const { return bias_; }

    void set_weights(Tensor w) { weights_ = std::move(w); }
    void set_bias(Tensor b) { bias_ = std::move(b); }

private:
    int64_t in_features_;
    int64_t out_features_;
    bool has_bias_;
    Tensor weights_;
    Tensor bias_;
    Tensor last_input_;
};

class ActivationLayer : public Layer {
public:
    explicit ActivationLayer(ActivationKind kind);

    std::string type_name() const override;
    Tensor forward(const Tensor& input) override;
    Tensor backward(const Tensor& grad_output, double lr) override;
    std::string serialize() const override;

    ActivationKind kind() const { return kind_; }

private:
    ActivationKind kind_;
    Tensor last_output_;
};

class AIModel {
public:
    AIModel();

    void add_layer(std::shared_ptr<Layer> layer);
    Tensor forward(const Tensor& x);
    Tensor predict(const Tensor& x);

    // Single gradient descent training step
    double train_step(const Tensor& x, const Tensor& y, double lr = 0.01);

    // End-to-end dataset fitting
    void fit(const Dataset& dataset,
             const std::vector<std::string>& feature_cols,
             const std::vector<std::string>& target_cols,
             int epochs = 10,
             double lr = 0.01,
             size_t batch_size = 16);

    // Serialization
    void save(const std::string& path) const;
    static AIModel load(const std::string& path);

    const std::vector<std::shared_ptr<Layer>>& layers() const { return layers_; }

    Value to_value() const;

private:
    std::vector<std::shared_ptr<Layer>> layers_;
};

} // namespace nextviper
