#include "nextviper/ai_subsystem.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/gpu_backend.hpp"
#include <map>
#include <atomic>
#include <mutex>

namespace nextviper {

// Global Registries for bridging C++ objects with NextViper Value dictionaries
static std::mutex g_registry_mutex;
static std::atomic<int64_t> g_next_id{1};

static std::map<int64_t, std::shared_ptr<Module>> g_module_map;
static std::map<int64_t, std::shared_ptr<Optimizer>> g_optimizer_map;
static std::map<int64_t, std::shared_ptr<Loss>> g_loss_map;

static Value wrap_module(std::shared_ptr<Module> mod) {
    if (!mod) return Value::make_nil();
    int64_t id = g_next_id++;
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        g_module_map[id] = mod;
    }

    std::map<std::string, Value> obj;
    obj["$type"] = Value::make_string(mod->type_name());
    obj["__module_id__"] = Value::make_int(id);
    obj["name"] = Value::make_string(mod->name());
    obj["params_count"] = Value::make_int(static_cast<int64_t>(mod->count_parameters()));

    obj["to"] = Value::make_native_fn("to", 1, [mod](const std::vector<Value>& args, SourceSpan) -> Value {
        Device d = string_to_device(args[0].as_string());
        mod->to(d);
        return wrap_module(mod);
    });

    obj["device"] = Value::make_native_fn("device", 0, [mod](const std::vector<Value>&, SourceSpan) -> Value {
        if (auto seq = std::dynamic_pointer_cast<Sequential>(mod)) {
            return Value::make_string(device_to_string(seq->device()));
        }
        return Value::make_string("cpu");
    });

    obj["forward"] = Value::make_native_fn("forward", 1, [mod](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_object()) throw RuntimeError("forward requires a Tensor", span);
        auto obj = args[0].as_object();
        auto to_list = obj->find("to_list");
        if (to_list == obj->end()) throw RuntimeError("Expected Tensor", span);
        Value arr_val = to_list->second.as_native_fn()->func({}, span);
        std::vector<double> vals;
        for (const auto& v : *arr_val.as_array()) vals.push_back(v.as_float());
        std::vector<int64_t> o_shape;
        for (const auto& s : *(*obj)["shape"].as_array()) o_shape.push_back(s.as_int());
        Tensor input(o_shape, vals);
        return mod->forward(input).to_value();
    });

    if (auto seq = std::dynamic_pointer_cast<Sequential>(mod)) {
        obj["summary"] = Value::make_native_fn("summary", 0, [seq](const std::vector<Value>&, SourceSpan) -> Value {
            return Value::make_string(seq->summary());
        });

        obj["compile"] = Value::make_native_fn("compile", -1, [seq](const std::vector<Value>& args, SourceSpan span) -> Value {
            if (args.size() < 2) throw RuntimeError("compile requires (optimizer, loss, [metrics])", span);
            
            // Extract optimizer
            std::shared_ptr<Optimizer> opt;
            if (args[0].is_object() && args[0].as_object()->find("__optimizer_id__") != args[0].as_object()->end()) {
                int64_t opt_id = (*args[0].as_object())["__optimizer_id__"].as_int();
                std::lock_guard<std::mutex> lock(g_registry_mutex);
                opt = g_optimizer_map[opt_id];
            } else {
                opt = std::make_shared<Adam>(seq->trainable_parameters(), 0.001);
            }

            // Extract loss
            std::shared_ptr<Loss> loss_fn;
            if (args[1].is_object() && args[1].as_object()->find("__loss_id__") != args[1].as_object()->end()) {
                int64_t loss_id = (*args[1].as_object())["__loss_id__"].as_int();
                std::lock_guard<std::mutex> lock(g_registry_mutex);
                loss_fn = g_loss_map[loss_id];
            } else {
                loss_fn = std::make_shared<MSELoss>();
            }

            std::vector<std::string> metrics;
            if (args.size() >= 3 && args[2].is_array()) {
                for (const auto& m : *args[2].as_array()) {
                    if (m.is_string()) metrics.push_back(m.as_string());
                }
            }

            seq->compile(opt, loss_fn, metrics);
            return Value::make_nil();
        });

        obj["fit"] = Value::make_native_fn("fit", -1, [seq](const std::vector<Value>& args, SourceSpan span) -> Value {
            if (args.size() < 2) throw RuntimeError("fit requires (x_train, y_train, [epochs, batch_size])", span);
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

            History h = seq->fit(x, y, epochs, batch_size);
            return h.to_value();
        });

        obj["predict"] = Value::make_native_fn("predict", 1, [seq](const std::vector<Value>& args, SourceSpan span) -> Value {
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
            return seq->predict(input).to_value();
        });

        obj["evaluate"] = Value::make_native_fn("evaluate", 2, [seq](const std::vector<Value>& args, SourceSpan span) -> Value {
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
            auto eval_map = seq->evaluate(x, y);
            std::map<std::string, Value> res_obj;
            for (const auto& [k, v] : eval_map) res_obj[k] = Value::make_float(v);
            return Value::make_object(std::move(res_obj));
        });

        obj["save"] = Value::make_native_fn("save", 1, [seq](const std::vector<Value>& args, SourceSpan) -> Value {
            std::string path = args[0].as_string();
            seq->save(path);
            return Value::make_nil();
        });

        obj["train"] = Value::make_native_fn("train", 0, [seq](const std::vector<Value>&, SourceSpan) -> Value {
            seq->train(true);
            return Value::make_nil();
        });

        obj["eval"] = Value::make_native_fn("eval", 0, [seq](const std::vector<Value>&, SourceSpan) -> Value {
            seq->eval();
            return Value::make_nil();
        });
    }

    return Value::make_object(std::move(obj));
}

static Value wrap_optimizer(std::shared_ptr<Optimizer> opt) {
    if (!opt) return Value::make_nil();
    int64_t id = g_next_id++;
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        g_optimizer_map[id] = opt;
    }
    std::map<std::string, Value> obj;
    obj["$type"] = Value::make_string("Optimizer");
    obj["__optimizer_id__"] = Value::make_int(id);
    obj["name"] = Value::make_string(opt->name());
    obj["lr"] = Value::make_float(opt->lr());
    return Value::make_object(std::move(obj));
}

static Value wrap_loss(std::shared_ptr<Loss> loss_fn) {
    if (!loss_fn) return Value::make_nil();
    int64_t id = g_next_id++;
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        g_loss_map[id] = loss_fn;
    }
    std::map<std::string, Value> obj;
    obj["$type"] = Value::make_string("Loss");
    obj["__loss_id__"] = Value::make_int(id);
    obj["name"] = Value::make_string(loss_fn->name());
    return Value::make_object(std::move(obj));
}

// ============================================================================
// AI Subsystem Module Definition
// ============================================================================

Value create_ai_subsystem_module() {
    std::map<std::string, Value> exports;

    // Model Creators
    exports["Sequential"] = Value::make_native_fn("Sequential", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_array()) throw RuntimeError("Sequential requires a list of layers", span);
        auto seq = std::make_shared<Sequential>();
        for (const auto& layer_val : *args[0].as_array()) {
            if (layer_val.is_object() && layer_val.as_object()->find("__module_id__") != layer_val.as_object()->end()) {
                int64_t mid = (*layer_val.as_object())["__module_id__"].as_int();
                std::lock_guard<std::mutex> lock(g_registry_mutex);
                auto mod = g_module_map[mid];
                if (mod) seq->add(mod);
            }
        }
        return wrap_module(seq);
    });
    exports["sequential"] = exports["Sequential"];

    // Layer Creators
    exports["Dense"] = Value::make_native_fn("Dense", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("Dense requires units or (in_features, out_features)", span);
        if (args.size() == 1) {
            int64_t units = args[0].as_int();
            return wrap_module(std::make_shared<Dense>(units, "none"));
        }
        if (args.size() == 2) {
            if (args[1].is_string()) {
                int64_t units = args[0].as_int();
                std::string act = args[1].as_string();
                return wrap_module(std::make_shared<Dense>(units, act));
            } else {
                int64_t in_f = args[0].as_int();
                int64_t out_f = args[1].as_int();
                return wrap_module(std::make_shared<Dense>(in_f, out_f, true, "none"));
            }
        }
        int64_t in_f = args[0].as_int();
        int64_t out_f = args[1].as_int();
        std::string act = args.size() >= 3 && args[2].is_string() ? args[2].as_string() : "none";
        bool bias = args.size() >= 4 ? args[3].as_bool() : true;
        return wrap_module(std::make_shared<Dense>(in_f, out_f, bias, act));
    });
    exports["dense"] = exports["Dense"];

    exports["Dropout"] = Value::make_native_fn("Dropout", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double rate = !args.empty() ? args[0].as_float() : 0.5;
        return wrap_module(std::make_shared<Dropout>(rate));
    });
    exports["dropout"] = exports["Dropout"];

    exports["Flatten"] = Value::make_native_fn("Flatten", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return wrap_module(std::make_shared<Flatten>());
    });
    exports["flatten"] = exports["Flatten"];

    exports["ReLU"] = Value::make_native_fn("ReLU", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return wrap_module(std::make_shared<ReLULayer>());
    });
    exports["relu"] = exports["ReLU"];

    exports["Sigmoid"] = Value::make_native_fn("Sigmoid", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return wrap_module(std::make_shared<SigmoidLayer>());
    });
    exports["sigmoid"] = exports["Sigmoid"];

    exports["Tanh"] = Value::make_native_fn("Tanh", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return wrap_module(std::make_shared<TanhLayer>());
    });
    exports["tanh"] = exports["Tanh"];

    exports["Softmax"] = Value::make_native_fn("Softmax", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t dim = !args.empty() ? args[0].as_int() : -1;
        return wrap_module(std::make_shared<SoftmaxLayer>(dim));
    });
    exports["softmax"] = exports["Softmax"];

    // Optimizer Creators
    exports["Adam"] = Value::make_native_fn("Adam", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double lr = !args.empty() ? args[0].as_float() : 0.001;
        double beta1 = args.size() >= 2 ? args[1].as_float() : 0.9;
        double beta2 = args.size() >= 3 ? args[2].as_float() : 0.999;
        double eps = args.size() >= 4 ? args[3].as_float() : 1e-8;
        double wd = args.size() >= 5 ? args[4].as_float() : 0.0;
        return wrap_optimizer(std::make_shared<Adam>(std::vector<std::shared_ptr<Parameter>>{}, lr, beta1, beta2, eps, wd));
    });
    exports["adam"] = exports["Adam"];

    exports["AdamW"] = Value::make_native_fn("AdamW", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double lr = !args.empty() ? args[0].as_float() : 0.001;
        double beta1 = args.size() >= 2 ? args[1].as_float() : 0.9;
        double beta2 = args.size() >= 3 ? args[2].as_float() : 0.999;
        double eps = args.size() >= 4 ? args[3].as_float() : 1e-8;
        double wd = args.size() >= 5 ? args[4].as_float() : 0.01;
        return wrap_optimizer(std::make_shared<AdamW>(std::vector<std::shared_ptr<Parameter>>{}, lr, beta1, beta2, eps, wd));
    });
    exports["adamw"] = exports["AdamW"];

    exports["SGD"] = Value::make_native_fn("SGD", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double lr = !args.empty() ? args[0].as_float() : 0.01;
        double momentum = args.size() >= 2 ? args[1].as_float() : 0.0;
        double wd = args.size() >= 3 ? args[2].as_float() : 0.0;
        return wrap_optimizer(std::make_shared<SGD>(std::vector<std::shared_ptr<Parameter>>{}, lr, momentum, wd));
    });
    exports["sgd"] = exports["SGD"];

    exports["Momentum"] = Value::make_native_fn("Momentum", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double lr = !args.empty() ? args[0].as_float() : 0.01;
        double momentum = args.size() >= 2 ? args[1].as_float() : 0.9;
        double wd = args.size() >= 3 ? args[2].as_float() : 0.0;
        return wrap_optimizer(std::make_shared<MomentumOptimizer>(std::vector<std::shared_ptr<Parameter>>{}, lr, momentum, wd));
    });
    exports["momentum"] = exports["Momentum"];

    // Loss Creators
    exports["MSE"] = Value::make_native_fn("MSE", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return wrap_loss(std::make_shared<MSELoss>());
    });
    exports["mse"] = exports["MSE"];
    exports["MSELoss"] = exports["MSE"];

    exports["MAE"] = Value::make_native_fn("MAE", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return wrap_loss(std::make_shared<MAELoss>());
    });
    exports["mae"] = exports["MAE"];
    exports["MAELoss"] = exports["MAE"];

    exports["CrossEntropy"] = Value::make_native_fn("CrossEntropy", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return wrap_loss(std::make_shared<CrossEntropyLoss>());
    });
    exports["cross_entropy"] = exports["CrossEntropy"];
    exports["CrossEntropyLoss"] = exports["CrossEntropy"];

    exports["BinaryCrossEntropy"] = Value::make_native_fn("BinaryCrossEntropy", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return wrap_loss(std::make_shared<BCELoss>());
    });
    exports["binary_cross_entropy"] = exports["BinaryCrossEntropy"];
    exports["BCELoss"] = exports["BinaryCrossEntropy"];

    // Model Loading
    exports["load"] = Value::make_native_fn("load", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        try {
            auto seq = Sequential::load(args[0].as_string());
            return wrap_module(seq);
        } catch (const std::exception& e) {
            throw RuntimeError(std::string("ai.load failed: ") + e.what(), span);
        }
    });

    // Backward compatibility
    exports["linear"] = Value::make_native_fn("linear", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.size() < 2) throw RuntimeError("ai.linear requires (in_features, out_features, [bias])", span);
        int64_t in_f = args[0].as_int();
        int64_t out_f = args[1].as_int();
        bool bias = args.size() >= 3 ? args[2].as_bool() : true;
        auto seq = std::make_shared<Sequential>();
        seq->add(std::make_shared<Dense>(in_f, out_f, bias, "none"));
        return wrap_module(seq);
    });

    exports["tensor"] = Value::make_native_fn("tensor", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("ai.tensor requires arguments", span);
        if (args.size() == 1 && args[0].is_array()) {
            const auto& arr = *args[0].as_array();
            std::vector<double> vals;
            for (const auto& v : arr) vals.push_back(v.as_float());
            return Tensor({static_cast<int64_t>(vals.size())}, vals).to_value();
        }
        if (args.size() >= 2 && args[0].is_array() && args[1].is_array()) {
            std::vector<int64_t> shape;
            for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
            std::vector<double> vals;
            for (const auto& v : *args[1].as_array()) vals.push_back(v.as_float());
            return Tensor(shape, vals).to_value();
        }
        throw RuntimeError("ai.tensor invalid arguments", span);
    });

    exports["zeros"] = Value::make_native_fn("zeros", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<int64_t> shape;
        if (args[0].is_array()) for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
        else shape.push_back(args[0].as_int());
        return Tensor::zeros(shape).to_value();
    });

    exports["ones"] = Value::make_native_fn("ones", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<int64_t> shape;
        if (args[0].is_array()) for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
        else shape.push_back(args[0].as_int());
        return Tensor::ones(shape).to_value();
    });

    exports["randn"] = Value::make_native_fn("randn", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<int64_t> shape;
        if (args[0].is_array()) for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
        else shape.push_back(args[0].as_int());
        double mean = args.size() >= 2 ? args[1].as_float() : 0.0;
        double stddev = args.size() >= 3 ? args[2].as_float() : 1.0;
        return Tensor::randn(shape, mean, stddev).to_value();
    });

    exports["uniform"] = Value::make_native_fn("uniform", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<int64_t> shape;
        if (args[0].is_array()) for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
        else shape.push_back(args[0].as_int());
        double low = args.size() >= 2 ? args[1].as_float() : 0.0;
        double high = args.size() >= 3 ? args[2].as_float() : 1.0;
        return Tensor::uniform(shape, low, high).to_value();
    });

    exports["is_gpu_available"] = Value::make_native_fn("is_gpu_available", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_bool(GPUTensorBackend::is_gpu_available());
    });

    exports["device_count"] = Value::make_native_fn("device_count", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_int(GPUTensorBackend::is_gpu_available() ? GPUTensorBackend::instance().device_count() : 1);
    });

    exports["device_name"] = Value::make_native_fn("device_name", -1, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_string(GPUTensorBackend::is_gpu_available() ? GPUTensorBackend::get_gpu_name() : "CPU (Host Vectorized Backend)");
    });

    exports["default_device"] = Value::make_native_fn("default_device", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_string(device_to_string(GPUTensorBackend::get_default_device()));
    });

    exports["set_default_device"] = Value::make_native_fn("set_default_device", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        GPUTensorBackend::set_default_device(string_to_device(args[0].as_string()));
        return Value::make_nil();
    });

    return Value::make_object(std::move(exports));
}

Value create_tensor_subsystem_module() {
    std::map<std::string, Value> exports;

    exports["is_gpu_available"] = Value::make_native_fn("is_gpu_available", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_bool(GPUTensorBackend::is_gpu_available());
    });

    exports["device_count"] = Value::make_native_fn("device_count", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_int(GPUTensorBackend::is_gpu_available() ? GPUTensorBackend::instance().device_count() : 1);
    });

    exports["device_name"] = Value::make_native_fn("device_name", -1, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_string(GPUTensorBackend::is_gpu_available() ? GPUTensorBackend::get_gpu_name() : "CPU (Host Vectorized Backend)");
    });

    exports["default_device"] = Value::make_native_fn("default_device", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_string(device_to_string(GPUTensorBackend::get_default_device()));
    });

    exports["set_default_device"] = Value::make_native_fn("set_default_device", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        GPUTensorBackend::set_default_device(string_to_device(args[0].as_string()));
        return Value::make_nil();
    });

    exports["create"] = Value::make_native_fn("create", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("tensor.create requires arguments", span);
        Device dev = GPUTensorBackend::get_default_device();
        if (args.size() == 1 && args[0].is_array()) {
            const auto& arr = *args[0].as_array();
            if (!arr.empty() && arr[0].is_array()) {
                int64_t rows = static_cast<int64_t>(arr.size());
                int64_t cols = static_cast<int64_t>(arr[0].as_array()->size());
                std::vector<double> vals;
                for (const auto& r : arr) {
                    if (r.is_array()) {
                        for (const auto& el : *r.as_array()) vals.push_back(el.as_float());
                    }
                }
                return Tensor({rows, cols}, vals, DType::FLOAT32, dev).to_value();
            } else {
                std::vector<double> vals;
                for (const auto& el : arr) vals.push_back(el.as_float());
                return Tensor({static_cast<int64_t>(vals.size())}, vals, DType::FLOAT32, dev).to_value();
            }
        }
        if (args.size() >= 2 && args[0].is_array() && args[1].is_array()) {
            std::vector<int64_t> shape;
            for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
            std::vector<double> vals;
            for (const auto& v : *args[1].as_array()) vals.push_back(v.as_float());
            if (args.size() >= 3 && args[2].is_string()) dev = string_to_device(args[2].as_string());
            return Tensor(shape, vals, DType::FLOAT32, dev).to_value();
        }
        throw RuntimeError("tensor.create invalid arguments", span);
    });
    exports["tensor"] = exports["create"];

    exports["from"] = Value::make_native_fn("from", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("tensor.from requires arguments", span);
        Device dev = args.size() >= 2 && args[1].is_string() ? string_to_device(args[1].as_string()) : GPUTensorBackend::get_default_device();

        if (args[0].is_object()) {
            auto obj = args[0].as_object();
            auto to_t = obj->find("to_tensor");
            if (to_t != obj->end()) {
                Value res = to_t->second.as_native_fn()->func({}, span);
                if (dev != Device::CPU) {
                    // Check if we can move device
                    auto res_obj = res.as_object();
                    auto to_fn = res_obj->find("to");
                    if (to_fn != res_obj->end()) {
                        return to_fn->second.as_native_fn()->func({Value::make_string(device_to_string(dev))}, span);
                    }
                }
                return res;
            }
            auto to_list = obj->find("to_list");
            if (to_list != obj->end()) {
                Value arr_val = to_list->second.as_native_fn()->func({}, span);
                std::vector<double> vals;
                for (const auto& v : *arr_val.as_array()) vals.push_back(v.as_float());
                std::vector<int64_t> o_shape;
                for (const auto& s : *(*obj)["shape"].as_array()) o_shape.push_back(s.as_int());
                return Tensor(o_shape, vals, DType::FLOAT32, dev).to_value();
            }
        }
        if (args[0].is_array()) {
            const auto& arr = *args[0].as_array();
            if (!arr.empty() && arr[0].is_array()) {
                int64_t rows = static_cast<int64_t>(arr.size());
                int64_t cols = static_cast<int64_t>(arr[0].as_array()->size());
                std::vector<double> vals;
                for (const auto& r : arr) {
                    if (r.is_array()) {
                        for (const auto& el : *r.as_array()) vals.push_back(el.as_float());
                    }
                }
                return Tensor({rows, cols}, vals, DType::FLOAT32, dev).to_value();
            }
            std::vector<double> vals;
            for (const auto& el : arr) vals.push_back(el.as_float());
            return Tensor({static_cast<int64_t>(vals.size())}, vals, DType::FLOAT32, dev).to_value();
        }
        throw RuntimeError("tensor.from expects array or tabular dataset", span);
    });

    exports["zeros"] = Value::make_native_fn("zeros", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<int64_t> shape;
        if (args[0].is_array()) for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
        else shape.push_back(args[0].as_int());
        Device dev = (args.size() >= 2 && args[1].is_string()) ? string_to_device(args[1].as_string()) : GPUTensorBackend::get_default_device();
        return Tensor::zeros(shape, DType::FLOAT32, dev).to_value();
    });

    exports["ones"] = Value::make_native_fn("ones", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<int64_t> shape;
        if (args[0].is_array()) for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
        else shape.push_back(args[0].as_int());
        Device dev = (args.size() >= 2 && args[1].is_string()) ? string_to_device(args[1].as_string()) : GPUTensorBackend::get_default_device();
        return Tensor::ones(shape, DType::FLOAT32, dev).to_value();
    });

    exports["randn"] = Value::make_native_fn("randn", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<int64_t> shape;
        if (args[0].is_array()) for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
        else shape.push_back(args[0].as_int());
        double mean = args.size() >= 2 && args[1].is_number() ? args[1].as_float() : 0.0;
        double stddev = args.size() >= 3 && args[2].is_number() ? args[2].as_float() : 1.0;
        Device dev = (args.size() >= 4 && args[3].is_string()) ? string_to_device(args[3].as_string()) : GPUTensorBackend::get_default_device();
        return Tensor::randn(shape, mean, stddev, dev).to_value();
    });

    exports["random"] = exports["randn"];

    exports["uniform"] = Value::make_native_fn("uniform", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<int64_t> shape;
        if (args[0].is_array()) for (const auto& s : *args[0].as_array()) shape.push_back(s.as_int());
        else shape.push_back(args[0].as_int());
        double low = args.size() >= 2 && args[1].is_number() ? args[1].as_float() : 0.0;
        double high = args.size() >= 3 && args[2].is_number() ? args[2].as_float() : 1.0;
        Device dev = (args.size() >= 4 && args[3].is_string()) ? string_to_device(args[3].as_string()) : GPUTensorBackend::get_default_device();
        return Tensor::uniform(shape, low, high, dev).to_value();
    });

    exports["matmul"] = Value::make_native_fn("matmul", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        try {
            if (!args[0].is_object()) throw RuntimeError("matmul requires a Tensor", span);
            auto a_obj = args[0].as_object();
            auto matmul_fn = a_obj->find("matmul");
            if (matmul_fn != a_obj->end()) {
                return matmul_fn->second.as_native_fn()->func({args[1]}, span);
            }
            throw RuntimeError("Invalid Tensor object for matmul", span);
        } catch (const std::exception& e) {
            throw RuntimeError(std::string("matmul error: ") + e.what(), span);
        }
    });

    return Value::make_object(std::move(exports));
}

} // namespace nextviper
