#pragma once

#include "nextviper/tensor.hpp"
#include "nextviper/value.hpp"
#include <string>
#include <memory>

namespace nextviper {

class Loss {
public:
    virtual ~Loss() = default;
    virtual std::string name() const = 0;
    virtual Tensor forward(const Tensor& pred, const Tensor& target) = 0;
    virtual Value to_value();
};

class MSELoss : public Loss {
public:
    std::string name() const override { return "MSE"; }
    Tensor forward(const Tensor& pred, const Tensor& target) override;
};

class MAELoss : public Loss {
public:
    std::string name() const override { return "MAE"; }
    Tensor forward(const Tensor& pred, const Tensor& target) override;
};

class BCELoss : public Loss {
public:
    explicit BCELoss(double eps = 1e-12) : eps_(eps) {}
    std::string name() const override { return "BinaryCrossEntropy"; }
    Tensor forward(const Tensor& pred, const Tensor& target) override;
private:
    double eps_;
};

class CrossEntropyLoss : public Loss {
public:
    explicit CrossEntropyLoss(double eps = 1e-12) : eps_(eps) {}
    std::string name() const override { return "CrossEntropy"; }
    Tensor forward(const Tensor& pred, const Tensor& target) override;
private:
    double eps_;
};

} // namespace nextviper
