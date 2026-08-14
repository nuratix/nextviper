#include "nextviper/ai_model.hpp"
#include "nextviper/interpreter.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <iostream>

namespace nextviper {

// --- LinearLayer Implementation ---

LinearLayer::LinearLayer(int64_t in_features, int64_t out_features, bool has_bias)
    : in_features_(in_features), out_features_(out_features), has_bias_(has_bias) {
    // Xavier / Glorot uniform initialization
    double limit = std::sqrt(6.0 / static_cast<double>(in_features + out_features));
    weights_ = Tensor::uniform({out_features, in_features}, -limit, limit);
    bias_ = has_bias ? Tensor::zeros({out_features}) : Tensor::zeros({1});
}

Tensor LinearLayer::forward(const Tensor& input) {
    last_input_ = input;
    // input is [N, in_features] or [in_features]
    if (input.ndim() == 2) {
        Tensor out = input.matmul(weights_.T()); // [N, out_features]
        if (has_bias_) {
            out = out.add(bias_);
        }
        return out;
    } else if (input.ndim() == 1) {
        Tensor in2 = input.reshape({1, input.shape()[0]});
        Tensor out = in2.matmul(weights_.T());
        if (has_bias_) out = out.add(bias_);
        return out.reshape({out_features_});
    }
    throw std::invalid_argument("LinearLayer input must be 1D or 2D Tensor");
}

Tensor LinearLayer::backward(const Tensor& grad_output, double lr) {
    // grad_output is [N, out_features]
    Tensor grad_out_2d = grad_output.ndim() == 1 ? grad_output.reshape({1, grad_output.shape()[0]}) : grad_output;
    Tensor last_in_2d = last_input_.ndim() == 1 ? last_input_.reshape({1, last_input_.shape()[0]}) : last_input_;

    // dW = grad_output.T @ input -> [out_features, in_features]
    Tensor grad_w = grad_out_2d.T().matmul(last_in_2d);

    // Update weights
    weights_ = weights_.sub(grad_w.scalar_mul(lr));

    // Update bias
    if (has_bias_) {
        Tensor grad_b = grad_out_2d.sum(0); // sum across batch dimension
        bias_ = bias_.sub(grad_b.scalar_mul(lr));
    }

    // dX = grad_output @ weights -> [N, in_features]
    Tensor grad_input = grad_out_2d.matmul(weights_);
    if (last_input_.ndim() == 1) {
        return grad_input.reshape({in_features_});
    }
    return grad_input;
}

std::string LinearLayer::serialize() const {
    std::ostringstream ss;
    ss << std::setprecision(17);
    ss << "LAYER Linear " << in_features_ << " " << out_features_ << " " << (has_bias_ ? 1 : 0) << "\n";
    ss << "WEIGHTS ";
    auto w_vec = weights_.to_vector();
    for (size_t i = 0; i < w_vec.size(); ++i) {
        if (i > 0) ss << " ";
        ss << w_vec[i];
    }
    ss << "\nBIAS ";
    auto b_vec = bias_.to_vector();
    for (size_t i = 0; i < b_vec.size(); ++i) {
        if (i > 0) ss << " ";
        ss << b_vec[i];
    }
    ss << "\n";
    return ss.str();
}

// --- ActivationLayer Implementation ---

ActivationLayer::ActivationLayer(ActivationKind kind) : kind_(kind) {}

std::string ActivationLayer::type_name() const {
    switch (kind_) {
        case ActivationKind::RELU: return "ReLU";
        case ActivationKind::SIGMOID: return "Sigmoid";
        case ActivationKind::TANH: return "Tanh";
        case ActivationKind::SOFTMAX: return "Softmax";
        case ActivationKind::NONE: return "Identity";
    }
    return "Activation";
}

Tensor ActivationLayer::forward(const Tensor& input) {
    switch (kind_) {
        case ActivationKind::RELU: last_output_ = input.relu(); break;
        case ActivationKind::SIGMOID: last_output_ = input.sigmoid(); break;
        case ActivationKind::TANH: last_output_ = input.tanh(); break;
        case ActivationKind::SOFTMAX: last_output_ = input.softmax(input.ndim() == 2 ? 1 : -1); break;
        case ActivationKind::NONE: last_output_ = input.clone(); break;
    }
    return last_output_;
}

Tensor ActivationLayer::backward(const Tensor& grad_output, double) {
    if (kind_ == ActivationKind::RELU) {
        Tensor grad(grad_output.shape(), grad_output.dtype(), grad_output.device());
        for (int64_t i = 0; i < grad_output.numel(); ++i) {
            grad.set_flat(i, last_output_.get_flat(i) > 0.0 ? grad_output.get_flat(i) : 0.0);
        }
        return grad;
    }
    if (kind_ == ActivationKind::SIGMOID) {
        Tensor grad(grad_output.shape(), grad_output.dtype(), grad_output.device());
        for (int64_t i = 0; i < grad_output.numel(); ++i) {
            double s = last_output_.get_flat(i);
            grad.set_flat(i, grad_output.get_flat(i) * s * (1.0 - s));
        }
        return grad;
    }
    // Default pass through for softmax/identity in standard cross-entropy / MSE
    return grad_output;
}

std::string ActivationLayer::serialize() const {
    std::ostringstream ss;
    ss << "LAYER " << type_name() << "\n";
    return ss.str();
}

// --- AIModel Implementation ---

AIModel::AIModel() {}

void AIModel::add_layer(std::shared_ptr<Layer> layer) {
    layers_.push_back(std::move(layer));
}

Tensor AIModel::forward(const Tensor& x) {
    Tensor cur = x;
    for (auto& layer : layers_) {
        cur = layer->forward(cur);
    }
    return cur;
}

Tensor AIModel::predict(const Tensor& x) {
    return forward(x);
}

double AIModel::train_step(const Tensor& x, const Tensor& y, double lr) {
    // Forward pass
    Tensor pred = forward(x);

    // MSE loss computation: loss = 0.5 * mean((pred - y)^2)
    Tensor diff = pred.sub(y);
    double loss = 0.5 * (diff.mul(diff).mean().item());

    // Backward pass
    // Initial gradient: dLoss/dPred = (pred - y) / N
    double scale = 1.0 / static_cast<double>(std::max<int64_t>(1, x.shape()[0]));
    Tensor grad = diff.scalar_mul(scale);

    for (int i = static_cast<int>(layers_.size()) - 1; i >= 0; --i) {
        grad = layers_[i]->backward(grad, lr);
    }

    return loss;
}

void AIModel::fit(const Dataset& dataset,
                  const std::vector<std::string>& feature_cols,
                  const std::vector<std::string>& target_cols,
                  int epochs,
                  double lr,
                  size_t batch_size) {
    DataLoader loader(dataset, batch_size, true);

    for (int ep = 0; ep < epochs; ++ep) {
        double ep_loss = 0.0;
        int batch_count = 0;

        for (const auto& batch : loader.batches()) {
            Tensor bx = batch.to_tensor(feature_cols);
            Tensor by = batch.to_tensor(target_cols);

            double loss = train_step(bx, by, lr);
            ep_loss += loss;
            batch_count++;
        }

        if (batch_count > 0 && ((ep + 1) % 5 == 0 || ep == 0 || ep + 1 == epochs)) {
            std::cout << "Epoch " << (ep + 1) << "/" << epochs << " - Loss: " << (ep_loss / batch_count) << "\n";
        }
    }
}

void AIModel::save(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Could not write model file: " + path);
    }
    out << "NEXTVIPER_AI_MODEL_V1\n";
    out << "NUM_LAYERS " << layers_.size() << "\n";
    for (const auto& l : layers_) {
        out << l->serialize();
    }
    out.close();
}

AIModel AIModel::load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Could not read model file: " + path);
    }

    std::string header;
    in >> header;
    if (header != "NEXTVIPER_AI_MODEL_V1") {
        throw std::runtime_error("Invalid NextViper model file format: " + path);
    }

    std::string token;
    size_t num_layers = 0;
    in >> token >> num_layers;

    AIModel model;
    for (size_t l = 0; l < num_layers; ++l) {
        std::string layer_tag, layer_type;
        in >> layer_tag >> layer_type;

        if (layer_type == "Linear") {
            int64_t in_f = 0, out_f = 0;
            int has_b = 1;
            in >> in_f >> out_f >> has_b;
            auto linear = std::make_shared<LinearLayer>(in_f, out_f, has_b != 0);

            std::string weights_tag;
            in >> weights_tag;
            std::vector<double> w_vals(in_f * out_f);
            for (size_t i = 0; i < w_vals.size(); ++i) in >> w_vals[i];
            linear->set_weights(Tensor({out_f, in_f}, w_vals));

            std::string bias_tag;
            in >> bias_tag;
            std::vector<double> b_vals(has_b ? out_f : 1);
            for (size_t i = 0; i < b_vals.size(); ++i) in >> b_vals[i];
            linear->set_bias(Tensor({out_f}, b_vals));

            model.add_layer(linear);
        } else if (layer_type == "ReLU") {
            model.add_layer(std::make_shared<ActivationLayer>(ActivationKind::RELU));
        } else if (layer_type == "Sigmoid") {
            model.add_layer(std::make_shared<ActivationLayer>(ActivationKind::SIGMOID));
        } else if (layer_type == "Tanh") {
            model.add_layer(std::make_shared<ActivationLayer>(ActivationKind::TANH));
        } else if (layer_type == "Softmax") {
            model.add_layer(std::make_shared<ActivationLayer>(ActivationKind::SOFTMAX));
        }
    }

    return model;
}

Value AIModel::to_value() const {
    auto self_model = std::make_shared<AIModel>(*this);
    std::map<std::string, Value> methods;

    methods["$type"] = Value::make_string("AIModel");

    methods["predict"] = Value::make_native_fn("predict", 1, [self_model](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args[0].is_object()) {
            auto obj = args[0].as_object();
            // If Dataset passed to predict
            if (obj->count("$type") && (*obj)["$type"].as_string() == "Dataset") {
                auto to_t = obj->find("to_tensor");
                if (to_t != obj->end()) {
                    Value t_val = to_t->second.as_native_fn()->func({}, span);
                    auto t_obj = t_val.as_object();
                    auto to_list = (*t_obj)["to_list"].as_native_fn();
                    Value l_val = to_list->func({}, span);
                    std::vector<double> vals;
                    for (const auto& v : *l_val.as_array()) vals.push_back(v.as_float());
                    std::vector<int64_t> sh;
                    for (const auto& s : *(*t_obj)["shape"].as_array()) sh.push_back(s.as_int());
                    Tensor in_tensor(sh, vals);
                    return self_model->predict(in_tensor).to_value();
                }
            }

            // If Tensor passed
            auto to_list = obj->find("to_list");
            if (to_list != obj->end()) {
                Value l_val = to_list->second.as_native_fn()->func({}, span);
                std::vector<double> vals;
                for (const auto& v : *l_val.as_array()) vals.push_back(v.as_float());
                std::vector<int64_t> sh;
                for (const auto& s : *(*obj)["shape"].as_array()) sh.push_back(s.as_int());
                Tensor in_tensor(sh, vals);
                return self_model->predict(in_tensor).to_value();
            }
        }
        throw RuntimeError("predict() requires a Tensor or Dataset", span);
    });

    methods["train_step"] = Value::make_native_fn("train_step", -1, [self_model](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.size() < 2) throw RuntimeError("train_step(x, y, [lr]) requires x and y tensors", span);
        auto x_obj = args[0].as_object();
        auto y_obj = args[1].as_object();
        double lr = args.size() >= 3 ? args[2].as_float() : 0.01;

        // Extract x
        Value lx = (*x_obj)["to_list"].as_native_fn()->func({}, span);
        std::vector<double> vx;
        for (const auto& v : *lx.as_array()) vx.push_back(v.as_float());
        std::vector<int64_t> sx;
        for (const auto& s : *(*x_obj)["shape"].as_array()) sx.push_back(s.as_int());

        // Extract y
        Value ly = (*y_obj)["to_list"].as_native_fn()->func({}, span);
        std::vector<double> vy;
        for (const auto& v : *ly.as_array()) vy.push_back(v.as_float());
        std::vector<int64_t> sy;
        for (const auto& s : *(*y_obj)["shape"].as_array()) sy.push_back(s.as_int());

        double loss = self_model->train_step(Tensor(sx, vx), Tensor(sy, vy), lr);
        return Value::make_float(loss);
    });

    methods["save"] = Value::make_native_fn("save", 1, [self_model](const std::vector<Value>& args, SourceSpan) -> Value {
        self_model->save(args[0].as_string());
        return Value::make_nil();
    });

    return Value::make_object(std::move(methods));
}

} // namespace nextviper
