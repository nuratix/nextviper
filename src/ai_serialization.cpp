#include "nextviper/ai_serialization.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>

namespace nextviper {

void ModelSerializer::save_sequential(const Sequential& model, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for saving model: " + filepath);
    }

    file << std::setprecision(17);
    file << "NVMODEL_V1\n";
    file << "LAYERS " << model.layers().size() << "\n";

    for (const auto& layer : model.layers()) {
        if (auto dense = std::dynamic_pointer_cast<Dense>(layer)) {
            file << "LAYER Dense in=" << dense->in_features()
                 << " out=" << dense->out_features()
                 << " bias=" << (dense->has_bias() ? 1 : 0)
                 << " activation=" << dense->activation_name() << "\n";

            if (dense->weight_param()) {
                auto w_vec = dense->weight_param()->data().to_vector();
                file << "WEIGHTS " << w_vec.size();
                for (double w : w_vec) file << " " << w;
                file << "\n";
            }
            if (dense->has_bias() && dense->bias_param()) {
                auto b_vec = dense->bias_param()->data().to_vector();
                file << "BIAS " << b_vec.size();
                for (double b : b_vec) file << " " << b;
                file << "\n";
            }
        } else if (auto drop = std::dynamic_pointer_cast<Dropout>(layer)) {
            file << "LAYER Dropout rate=" << drop->rate() << "\n";
        } else if (std::dynamic_pointer_cast<Flatten>(layer)) {
            file << "LAYER Flatten\n";
        } else if (std::dynamic_pointer_cast<ReLULayer>(layer)) {
            file << "LAYER ReLU\n";
        } else if (std::dynamic_pointer_cast<SigmoidLayer>(layer)) {
            file << "LAYER Sigmoid\n";
        } else if (std::dynamic_pointer_cast<TanhLayer>(layer)) {
            file << "LAYER Tanh\n";
        } else if (std::dynamic_pointer_cast<SoftmaxLayer>(layer)) {
            file << "LAYER Softmax\n";
        }
    }
    file << "NVMODEL_END\n";
}

std::shared_ptr<Sequential> ModelSerializer::load_sequential(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for loading model: " + filepath);
    }

    std::string header;
    file >> header;
    if (header != "NVMODEL_V1") {
        throw std::runtime_error("Invalid model format or magic header: " + header);
    }

    auto seq = std::make_shared<Sequential>();
    std::string token;

    while (file >> token) {
        if (token == "NVMODEL_END") break;
        if (token == "LAYERS") {
            size_t count;
            file >> count;
            continue;
        }

        if (token == "LAYER") {
            std::string layer_type;
            file >> layer_type;

            if (layer_type == "Dense") {
                int64_t in_f = 0, out_f = 0;
                int has_b = 1;
                std::string act = "none";

                std::string kv;
                while (file >> kv) {
                    if (kv.rfind("in=", 0) == 0) in_f = std::stoll(kv.substr(3));
                    else if (kv.rfind("out=", 0) == 0) out_f = std::stoll(kv.substr(4));
                    else if (kv.rfind("bias=", 0) == 0) has_b = std::stoi(kv.substr(5));
                    else if (kv.rfind("activation=", 0) == 0) {
                        act = kv.substr(11);
                        break;
                    }
                }

                auto dense = std::make_shared<Dense>(in_f, out_f, has_b != 0, act);

                // Read weights
                std::string w_token;
                file >> w_token;
                if (w_token == "WEIGHTS") {
                    size_t w_count;
                    file >> w_count;
                    std::vector<double> w_vals(w_count);
                    for (size_t i = 0; i < w_count; ++i) file >> w_vals[i];
                    if (dense->weight_param()) {
                        dense->weight_param()->data() = Tensor({out_f, in_f}, w_vals);
                    }
                }

                if (has_b) {
                    std::string b_token;
                    file >> b_token;
                    if (b_token == "BIAS") {
                        size_t b_count;
                        file >> b_count;
                        std::vector<double> b_vals(b_count);
                        for (size_t i = 0; i < b_count; ++i) file >> b_vals[i];
                        if (dense->bias_param()) {
                            dense->bias_param()->data() = Tensor({out_f}, b_vals);
                        }
                    }
                }

                seq->add(dense);
            } else if (layer_type == "Dropout") {
                std::string rate_kv;
                file >> rate_kv;
                double rate = 0.5;
                if (rate_kv.rfind("rate=", 0) == 0) rate = std::stod(rate_kv.substr(5));
                seq->add(std::make_shared<Dropout>(rate));
            } else if (layer_type == "Flatten") {
                seq->add(std::make_shared<Flatten>());
            } else if (layer_type == "ReLU") {
                seq->add(std::make_shared<ReLULayer>());
            } else if (layer_type == "Sigmoid") {
                seq->add(std::make_shared<SigmoidLayer>());
            } else if (layer_type == "Tanh") {
                seq->add(std::make_shared<TanhLayer>());
            } else if (layer_type == "Softmax") {
                seq->add(std::make_shared<SoftmaxLayer>());
            }
        }
    }

    return seq;
}

} // namespace nextviper
