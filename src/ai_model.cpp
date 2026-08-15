#include "nextviper/ai_model.hpp"
#include "nextviper/ai_serialization.hpp"
#include "nextviper/autograd.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/gpu_backend.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <numeric>
#include <algorithm>

namespace nextviper {

// ============================================================================
// History Implementation
// ============================================================================

Value History::to_value() const {
    std::map<std::string, Value> obj;
    obj["$type"] = Value::make_string("History");

    std::vector<Value> loss_arr;
    for (double l : loss) loss_arr.push_back(Value::make_float(l));
    obj["loss"] = Value::make_array(std::move(loss_arr));

    std::map<std::string, Value> m_map;
    for (const auto& [name, vals] : metrics) {
        std::vector<Value> v_arr;
        for (double v : vals) v_arr.push_back(Value::make_float(v));
        m_map[name] = Value::make_array(std::move(v_arr));
        if (name == "accuracy" || name == "acc") {
            obj["accuracy"] = Value::make_array(v_arr);
        }
    }
    obj["metrics"] = Value::make_object(std::move(m_map));

    std::vector<Value> vloss_arr;
    for (double l : val_loss) vloss_arr.push_back(Value::make_float(l));
    obj["val_loss"] = Value::make_array(std::move(vloss_arr));

    obj["epochs"] = Value::make_int(epochs);
    return Value::make_object(std::move(obj));
}

// ============================================================================
// Sequential Container Implementation
// ============================================================================

Sequential::Sequential(std::vector<std::shared_ptr<Module>> layers)
    : layers_(std::move(layers)) {}

void Sequential::add(std::shared_ptr<Module> layer) {
    if (layer) layers_.push_back(layer);
}

Tensor Sequential::forward(const Tensor& input) {
    Tensor out = input;
    for (auto& layer : layers_) {
        out = layer->forward(out);
    }
    return out;
}

std::vector<std::shared_ptr<Parameter>> Sequential::parameters() {
    std::vector<std::shared_ptr<Parameter>> res;
    for (auto& layer : layers_) {
        auto layer_params = layer->parameters();
        res.insert(res.end(), layer_params.begin(), layer_params.end());
    }
    return res;
}

std::vector<std::shared_ptr<Parameter>> Sequential::trainable_parameters() {
    std::vector<std::shared_ptr<Parameter>> res;
    for (auto& layer : layers_) {
        auto layer_params = layer->trainable_parameters();
        res.insert(res.end(), layer_params.begin(), layer_params.end());
    }
    return res;
}

void Sequential::train(bool mode) {
    training_ = mode;
    for (auto& layer : layers_) {
        layer->train(mode);
    }
}

void Sequential::eval() {
    train(false);
}

void Sequential::zero_grad() {
    for (auto& layer : layers_) {
        layer->zero_grad();
    }
}

void Sequential::to(Device dev) {
    if (dev == Device::AUTO) {
        dev = GPUTensorBackend::is_gpu_available() ? Device::GPU : Device::CPU;
    }
    if (dev == Device::GPU && !GPUTensorBackend::is_gpu_available()) {
        throw std::runtime_error("GPU unavailable: No compatible GPU or Vulkan compute device found for model execution");
    }
    device_ = dev;
    for (auto& layer : layers_) {
        if (layer) layer->to(dev);
    }
    if (optimizer_) {
        optimizer_->set_parameters(trainable_parameters());
    }
}

void Sequential::compile(std::shared_ptr<Optimizer> optimizer,
                         std::shared_ptr<Loss> loss,
                         std::vector<std::string> metrics) {
    optimizer_ = std::move(optimizer);
    loss_fn_ = std::move(loss);
    metric_names_ = std::move(metrics);

    if (optimizer_) {
        optimizer_->set_parameters(trainable_parameters());
    }
}

History Sequential::fit(const Tensor& x_train,
                        const Tensor& y_train,
                        int epochs,
                        size_t batch_size,
                        bool do_shuffle,
                        const Tensor& val_x,
                        const Tensor& val_y) {
    if (!loss_fn_) {
        loss_fn_ = std::make_shared<MSELoss>();
    }
    if (!optimizer_) {
        optimizer_ = std::make_shared<Adam>(trainable_parameters(), 0.001);
    } else {
        optimizer_->set_parameters(trainable_parameters());
    }

    History hist;
    hist.epochs = epochs;

    int64_t num_samples = x_train.shape()[0];
    std::vector<size_t> indices(num_samples);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 g(42);

    for (int epoch = 1; epoch <= epochs; ++epoch) {
        train(true);
        if (do_shuffle) {
            std::shuffle(indices.begin(), indices.end(), g);
        }

        double epoch_loss = 0.0;
        size_t num_batches = 0;

        for (int64_t i = 0; i < num_samples; i += batch_size) {
            int64_t cur_batch_size = std::min<int64_t>(batch_size, num_samples - i);
            
            // Build batch tensors
            std::vector<int64_t> x_batch_shape = x_train.shape();
            x_batch_shape[0] = cur_batch_size;
            Tensor x_batch(x_batch_shape, x_train.dtype(), x_train.device());

            std::vector<int64_t> y_batch_shape = y_train.shape();
            y_batch_shape[0] = cur_batch_size;
            Tensor y_batch(y_batch_shape, y_train.dtype(), y_train.device());

            int64_t x_features = x_train.numel() / num_samples;
            int64_t y_features = y_train.numel() / num_samples;

            for (int64_t b = 0; b < cur_batch_size; ++b) {
                size_t src_idx = indices[i + b];
                for (int64_t f = 0; f < x_features; ++f) {
                    x_batch.set_flat(b * x_features + f, x_train.get_flat(src_idx * x_features + f));
                }
                for (int64_t f = 0; f < y_features; ++f) {
                    y_batch.set_flat(b * y_features + f, y_train.get_flat(src_idx * y_features + f));
                }
            }

            // 1. Zero gradients
            optimizer_->zero_grad();

            // 2. Forward pass
            Tensor pred = forward(x_batch);

            // 3. Loss calculation
            Tensor loss = loss_fn_->forward(pred, y_batch);
            double loss_val = loss.item();
            epoch_loss += loss_val;
            num_batches++;

            // 4. Backward pass
            loss.backward();

            // 5. Optimizer step
            optimizer_->step();
        }

        double avg_loss = num_batches > 0 ? (epoch_loss / num_batches) : 0.0;
        hist.loss.push_back(avg_loss);

        // Compute training metrics
        eval();
        Tensor full_pred = forward(x_train);
        for (const auto& metric : metric_names_) {
            double m_val = Metrics::compute(metric, full_pred, y_train);
            hist.metrics[metric].push_back(m_val);
        }

        // Validation pass
        if (val_x.numel() > 0 && val_y.numel() > 0) {
            Tensor val_pred = forward(val_x);
            Tensor v_loss = loss_fn_->forward(val_pred, val_y);
            hist.val_loss.push_back(v_loss.item());
            for (const auto& metric : metric_names_) {
                double m_val = Metrics::compute(metric, val_pred, val_y);
                hist.val_metrics[metric].push_back(m_val);
            }
        }
    }

    eval();
    return hist;
}

Tensor Sequential::predict(const Tensor& input) {
    eval();
    AutogradContext::NoGradGuard guard;
    return forward(input);
}

std::map<std::string, double> Sequential::evaluate(const Tensor& x, const Tensor& y) {
    eval();
    AutogradContext::NoGradGuard guard;
    Tensor pred = forward(x);
    std::map<std::string, double> results;

    if (loss_fn_) {
        Tensor loss = loss_fn_->forward(pred, y);
        results["loss"] = loss.item();
    }

    for (const auto& metric : metric_names_) {
        results[metric] = Metrics::compute(metric, pred, y);
    }
    return results;
}

std::string Sequential::summary() const {
    std::ostringstream ss;
    ss << "=================================================================\n";
    ss << "Layer (type)                 Output Shape              Param #   \n";
    ss << "=================================================================\n";

    size_t total_params = 0;
    size_t trainable_params = 0;

    for (const auto& layer : layers_) {
        size_t p_count = layer->count_parameters();
        total_params += p_count;
        trainable_params += layer->trainable_parameters().size() > 0 ? p_count : 0;

        ss << std::left << std::setw(29) << layer->name()
           << std::setw(26) << "Multiple"
           << std::setw(10) << p_count << "\n";
    }

    ss << "=================================================================\n";
    ss << "Total params: " << total_params << "\n";
    ss << "Trainable params: " << trainable_params << "\n";
    ss << "Non-trainable params: " << (total_params - trainable_params) << "\n";
    ss << "=================================================================\n";

    return ss.str();
}

void Sequential::save(const std::string& path) const {
    ModelSerializer::save_sequential(*this, path);
}

std::shared_ptr<Sequential> Sequential::load(const std::string& path) {
    return ModelSerializer::load_sequential(path);
}

Value Sequential::to_value() {
    auto self_model = std::make_shared<Sequential>(*this);
    std::map<std::string, Value> obj;

    obj["$type"] = Value::make_string("Sequential");
    obj["layers_count"] = Value::make_int(static_cast<int64_t>(layers_.size()));
    obj["params_count"] = Value::make_int(static_cast<int64_t>(count_parameters()));

    obj["add"] = Value::make_native_fn("add", 1, [self_model](const std::vector<Value>& args, SourceSpan span) -> Value {
        (void)args; (void)span;
        return Value::make_nil();
    });

    obj["summary"] = Value::make_native_fn("summary", 0, [self_model](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_string(self_model->summary());
    });

    obj["predict"] = Value::make_native_fn("predict", 1, [self_model](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_object()) throw RuntimeError("predict requires a Tensor", span);
        auto obj = args[0].as_object();
        auto to_list = obj->find("to_list");
        if (to_list == obj->end()) throw RuntimeError("Expected Tensor", span);
        Value arr_val = to_list->second.as_native_fn()->func({}, span);
        std::vector<double> vals;
        for (const auto& v : *arr_val.as_array()) vals.push_back(v.as_float());
        std::vector<int64_t> o_shape;
        for (const auto& s : *(*obj)["shape"].as_array()) o_shape.push_back(s.as_int());
        Tensor input(o_shape, vals);
        return self_model->predict(input).to_value();
    });

    obj["fit"] = Value::make_native_fn("fit", -1, [self_model](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.size() < 2) throw RuntimeError("fit requires x_train and y_train tensors", span);
        auto extract_tensor = [](const Value& v, SourceSpan s) -> Tensor {
            auto obj = v.as_object();
            auto to_list = obj->find("to_list");
            if (to_list == obj->end()) throw RuntimeError("Expected Tensor", s);
            Value arr_val = to_list->second.as_native_fn()->func({}, s);
            std::vector<double> vals;
            for (const auto& el : *arr_val.as_array()) vals.push_back(el.as_float());
            std::vector<int64_t> o_shape;
            for (const auto& el : *(*obj)["shape"].as_array()) o_shape.push_back(el.as_int());
            return Tensor(o_shape, vals);
        };
        Tensor x = extract_tensor(args[0], span);
        Tensor y = extract_tensor(args[1], span);
        int epochs = (args.size() >= 3 && args[2].is_int()) ? static_cast<int>(args[2].as_int()) : 10;
        size_t batch_size = (args.size() >= 4 && args[3].is_int()) ? static_cast<size_t>(args[3].as_int()) : 32;
        History h = self_model->fit(x, y, epochs, batch_size);
        return h.to_value();
    });

    obj["to"] = Value::make_native_fn("to", 1, [self_model](const std::vector<Value>& args, SourceSpan) -> Value {
        Device d = string_to_device(args[0].as_string());
        self_model->to(d);
        return self_model->to_value();
    });

    obj["device"] = Value::make_native_fn("device", 0, [self_model](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_string(device_to_string(self_model->device()));
    });

    obj["save"] = Value::make_native_fn("save", 1, [self_model](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string path = args[0].as_string();
        self_model->save(path);
        return Value::make_nil();
    });

    return Value::make_object(std::move(obj));
}

// ============================================================================
// AIModel Wrapper Implementation
// ============================================================================

AIModel::AIModel() : seq_(std::make_shared<Sequential>()) {}

AIModel::AIModel(std::shared_ptr<Sequential> seq) : seq_(std::move(seq)) {}

void AIModel::add_layer(std::shared_ptr<Module> layer) {
    if (seq_ && layer) seq_->add(layer);
}

Tensor AIModel::forward(const Tensor& x) {
    return seq_ ? seq_->forward(x) : Tensor();
}

Tensor AIModel::predict(const Tensor& x) {
    return seq_ ? seq_->predict(x) : Tensor();
}

double AIModel::train_step(const Tensor& x, const Tensor& y, double lr) {
    if (!seq_) return 0.0;
    auto opt = std::make_shared<SGD>(seq_->trainable_parameters(), lr);
    auto loss_fn = std::make_shared<MSELoss>();
    seq_->compile(opt, loss_fn);

    opt->zero_grad();
    Tensor pred = seq_->forward(x);
    Tensor loss = loss_fn->forward(pred, y);
    loss.backward();
    opt->step();
    return loss.item();
}

void AIModel::fit(const Dataset& dataset,
                  const std::vector<std::string>& feature_cols,
                  const std::vector<std::string>& target_cols,
                  int epochs,
                  double lr,
                  size_t batch_size) {
    if (!seq_) return;
    Tensor x = dataset.to_tensor(feature_cols);
    Tensor y = dataset.to_tensor(target_cols);
    auto opt = std::make_shared<Adam>(seq_->trainable_parameters(), lr);
    auto loss_fn = std::make_shared<MSELoss>();
    seq_->compile(opt, loss_fn);
    seq_->fit(x, y, epochs, batch_size);
}

void AIModel::save(const std::string& path) const {
    if (seq_) seq_->save(path);
}

AIModel AIModel::load(const std::string& path) {
    auto seq = Sequential::load(path);
    return AIModel(seq);
}

Value AIModel::to_value() const {
    return seq_ ? seq_->to_value() : Value::make_nil();
}

} // namespace nextviper
