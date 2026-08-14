#pragma once

#include "nextviper/ai_model.hpp"
#include <string>
#include <memory>

namespace nextviper {

class ModelSerializer {
public:
    static void save_sequential(const Sequential& model, const std::string& filepath);
    static std::shared_ptr<Sequential> load_sequential(const std::string& filepath);
};

} // namespace nextviper
