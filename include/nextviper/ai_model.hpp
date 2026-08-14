#pragma once

#include "nextviper/ai_layers.hpp"
#include "nextviper/ai_loss.hpp"
#include "nextviper/ai_optimizer.hpp"
#include "nextviper/ai_metrics.hpp"
#include "nextviper/dataset.hpp"
#include "nextviper/value.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>

namespace nextviper {

// Training History
struct History {
    std::vector<double> loss;
    std::map<std::string, std::vector<double>> metrics;
    std::vector<double> val_loss;
    std::map<std::string, std::vector<double>> val_metrics;
    int epochs = 0;

    Value to_value() const;
};

// Sequential Container Model
class Sequential : public Module {
public:
    Sequential() = default;
    explicit Sequential(std::vector<std::shared_ptr<Module>> layers);

    std::string type_name() const override { return "Sequential"; }

    void add(std::shared_ptr<Module> layer);

    Tensor forward(const Tensor& input) override;

    std::vector<std::shared_ptr<Parameter>> parameters() override;
    std::vector<std::shared_ptr<Parameter>> trainable_parameters() override;

    void train(bool mode = true) override;
    void eval() override;
    void zero_grad() override;

    const std::vector<std::shared_ptr<Module>>& layers() const { return layers_; }

    void compile(std::shared_ptr<Optimizer> optimizer,
                 std::shared_ptr<Loss> loss,
                 std::vector<std::string> metrics = {});

    History fit(const Tensor& x_train,
                const Tensor& y_train,
                int epochs = 10,
                size_t batch_size = 32,
                bool shuffle = true,
                const Tensor& val_x = Tensor(),
                const Tensor& val_y = Tensor());

    Tensor predict(const Tensor& input);

    std::map<std::string, double> evaluate(const Tensor& x, const Tensor& y);

    std::string summary() const;

    void save(const std::string& path) const;
    static std::shared_ptr<Sequential> load(const std::string& path);

    Value to_value() override;

private:
    std::vector<std::shared_ptr<Module>> layers_;
    std::shared_ptr<Optimizer> optimizer_;
    std::shared_ptr<Loss> loss_fn_;
    std::vector<std::string> metric_names_;
};

// Backward-compatible AIModel wrapper
class AIModel {
public:
    AIModel();
    explicit AIModel(std::shared_ptr<Sequential> seq);

    void add_layer(std::shared_ptr<Module> layer);
    Tensor forward(const Tensor& x);
    Tensor predict(const Tensor& x);

    double train_step(const Tensor& x, const Tensor& y, double lr = 0.01);

    void fit(const Dataset& dataset,
             const std::vector<std::string>& feature_cols,
             const std::vector<std::string>& target_cols,
             int epochs = 10,
             double lr = 0.01,
             size_t batch_size = 16);

    void save(const std::string& path) const;
    static AIModel load(const std::string& path);

    std::shared_ptr<Sequential> sequential() const { return seq_; }

    Value to_value() const;

private:
    std::shared_ptr<Sequential> seq_;
};

} // namespace nextviper
