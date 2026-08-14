#pragma once

#include "nextviper/tensor.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace nextviper {

class Tensor;

// Abstract base class for backward computation graph nodes
struct AutogradNode {
    virtual ~AutogradNode() = default;
    virtual std::string name() const = 0;
    virtual std::vector<Tensor> backward(const Tensor& grad_output) = 0;

    // Weak / Shared references to parent tensors
    std::vector<std::shared_ptr<Tensor>> inputs;
};

// Global Autograd context controls
class AutogradContext {
public:
    static bool is_grad_enabled() { return grad_enabled_; }
    static void set_grad_enabled(bool enabled) { grad_enabled_ = enabled; }

    class NoGradGuard {
    public:
        NoGradGuard() : prev_(AutogradContext::is_grad_enabled()) {
            AutogradContext::set_grad_enabled(false);
        }
        ~NoGradGuard() {
            AutogradContext::set_grad_enabled(prev_);
        }
    private:
        bool prev_;
    };

private:
    static inline bool grad_enabled_ = true;
};

// ============================================================================
// Concrete Autograd Nodes for Operations
// ============================================================================

struct AddBackwardNode : public AutogradNode {
    Tensor shape_a, shape_b;
    AddBackwardNode(const Tensor& a, const Tensor& b);
    std::string name() const override { return "AddBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct SubBackwardNode : public AutogradNode {
    Tensor shape_a, shape_b;
    SubBackwardNode(const Tensor& a, const Tensor& b);
    std::string name() const override { return "SubBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct MulBackwardNode : public AutogradNode {
    Tensor saved_a, saved_b;
    MulBackwardNode(const Tensor& a, const Tensor& b);
    std::string name() const override { return "MulBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct DivBackwardNode : public AutogradNode {
    Tensor saved_a, saved_b;
    DivBackwardNode(const Tensor& a, const Tensor& b);
    std::string name() const override { return "DivBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct MatMulBackwardNode : public AutogradNode {
    Tensor saved_a, saved_b;
    MatMulBackwardNode(const Tensor& a, const Tensor& b);
    std::string name() const override { return "MatMulBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct TransposeBackwardNode : public AutogradNode {
    std::string name() const override { return "TransposeBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct ScalarMulBackwardNode : public AutogradNode {
    double scalar;
    ScalarMulBackwardNode(double s);
    std::string name() const override { return "ScalarMulBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct ScalarAddBackwardNode : public AutogradNode {
    ScalarAddBackwardNode();
    std::string name() const override { return "ScalarAddBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct SumBackwardNode : public AutogradNode {
    std::vector<int64_t> input_shape;
    int64_t dim;
    bool keepdims;
    SumBackwardNode(const std::vector<int64_t>& shape, int64_t d, bool keep);
    std::string name() const override { return "SumBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct MeanBackwardNode : public AutogradNode {
    std::vector<int64_t> input_shape;
    int64_t dim;
    bool keepdims;
    double count;
    MeanBackwardNode(const std::vector<int64_t>& shape, int64_t d, bool keep, double cnt);
    std::string name() const override { return "MeanBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct ReLUBackwardNode : public AutogradNode {
    Tensor saved_input;
    ReLUBackwardNode(const Tensor& in);
    std::string name() const override { return "ReLUBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct SigmoidBackwardNode : public AutogradNode {
    Tensor saved_output;
    SigmoidBackwardNode(const Tensor& out);
    std::string name() const override { return "SigmoidBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct TanhBackwardNode : public AutogradNode {
    Tensor saved_output;
    TanhBackwardNode(const Tensor& out);
    std::string name() const override { return "TanhBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct SoftmaxBackwardNode : public AutogradNode {
    Tensor saved_output;
    int64_t dim;
    SoftmaxBackwardNode(const Tensor& out, int64_t d);
    std::string name() const override { return "SoftmaxBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct LogBackwardNode : public AutogradNode {
    Tensor saved_input;
    double eps;
    LogBackwardNode(const Tensor& in, double epsilon);
    std::string name() const override { return "LogBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct ExpBackwardNode : public AutogradNode {
    Tensor saved_output;
    ExpBackwardNode(const Tensor& out);
    std::string name() const override { return "ExpBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct PowBackwardNode : public AutogradNode {
    Tensor saved_input;
    double exponent;
    PowBackwardNode(const Tensor& in, double exp);
    std::string name() const override { return "PowBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

struct ReshapeBackwardNode : public AutogradNode {
    std::vector<int64_t> orig_shape;
    ReshapeBackwardNode(const std::vector<int64_t>& shape);
    std::string name() const override { return "ReshapeBackward"; }
    std::vector<Tensor> backward(const Tensor& grad_output) override;
};

// Numerical gradient checker for verifying autograd correctness
double check_numerical_gradient(const std::function<double(const Tensor&)>& fn,
                                const Tensor& input,
                                const Tensor& analytic_grad,
                                double eps = 1e-4);

} // namespace nextviper
