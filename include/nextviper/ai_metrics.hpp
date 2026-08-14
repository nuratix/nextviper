#pragma once

#include "nextviper/tensor.hpp"
#include <string>

namespace nextviper {

class Metrics {
public:
    static double accuracy(const Tensor& pred, const Tensor& target);
    static double precision(const Tensor& pred, const Tensor& target);
    static double recall(const Tensor& pred, const Tensor& target);
    static double f1_score(const Tensor& pred, const Tensor& target);
    static double mae(const Tensor& pred, const Tensor& target);
    static double mse(const Tensor& pred, const Tensor& target);

    static double compute(const std::string& name, const Tensor& pred, const Tensor& target);
};

} // namespace nextviper
