#pragma once

#include "nextviper/tensor.hpp"
#include "nextviper/autograd.hpp"
#include "nextviper/ai_layers.hpp"
#include "nextviper/ai_loss.hpp"
#include "nextviper/ai_optimizer.hpp"
#include "nextviper/ai_metrics.hpp"
#include "nextviper/ai_model.hpp"
#include "nextviper/ai_serialization.hpp"

namespace nextviper {

Value create_ai_subsystem_module();
Value create_tensor_subsystem_module();

} // namespace nextviper
