#include "nextviper/autograd.hpp"
#include <cmath>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <iostream>

namespace nextviper {

// ============================================================================
// Autograd Backward Nodes
// ============================================================================

static Tensor reduce_to_shape(Tensor grad, const std::vector<int64_t>& target_shape) {
    if (grad.shape() == target_shape) return grad;
    if (target_shape.empty() || (target_shape.size() == 1 && target_shape[0] == 1)) {
        return grad.sum();
    }
    int64_t target_numel = 1;
    for (int64_t d : target_shape) target_numel *= d;
    if (target_numel == 1) {
        return grad.sum();
    }
    if (target_shape.size() == 1 && grad.ndim() == 2) {
        if (target_shape[0] == grad.shape()[1]) return grad.sum(0);
        if (target_shape[0] == grad.shape()[0]) return grad.sum(1);
    }
    if (target_shape.size() == 2 && grad.ndim() == 2) {
        if (target_shape[1] == 1 && grad.shape()[1] > 1) {
            grad = grad.sum(1, true);
        }
        if (target_shape[0] == 1 && grad.shape()[0] > 1) {
            grad = grad.sum(0, true);
        }
    }
    return grad;
}

AddBackwardNode::AddBackwardNode(const Tensor& a, const Tensor& b)
    : shape_a(a), shape_b(b) {}

std::vector<Tensor> AddBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad_a = reduce_to_shape(grad_output.clone(), shape_a.shape());
    Tensor grad_b = reduce_to_shape(grad_output.clone(), shape_b.shape());
    return {grad_a, grad_b};
}

SubBackwardNode::SubBackwardNode(const Tensor& a, const Tensor& b)
    : shape_a(a), shape_b(b) {}

std::vector<Tensor> SubBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad_a = reduce_to_shape(grad_output.clone(), shape_a.shape());
    Tensor grad_b = reduce_to_shape(grad_output.neg(), shape_b.shape());
    return {grad_a, grad_b};
}

MulBackwardNode::MulBackwardNode(const Tensor& a, const Tensor& b)
    : saved_a(a.clone()), saved_b(b.clone()) {}

std::vector<Tensor> MulBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad_a = reduce_to_shape(grad_output.mul(saved_b), saved_a.shape());
    Tensor grad_b = reduce_to_shape(grad_output.mul(saved_a), saved_b.shape());
    return {grad_a, grad_b};
}

DivBackwardNode::DivBackwardNode(const Tensor& a, const Tensor& b)
    : saved_a(a.clone()), saved_b(b.clone()) {}

std::vector<Tensor> DivBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad_a = reduce_to_shape(grad_output.div(saved_b), saved_a.shape());
    Tensor b_sq = saved_b.mul(saved_b);
    Tensor grad_b = reduce_to_shape(grad_output.mul(saved_a).div(b_sq).neg(), saved_b.shape());
    return {grad_a, grad_b};
}

MatMulBackwardNode::MatMulBackwardNode(const Tensor& a, const Tensor& b)
    : saved_a(a.clone()), saved_b(b.clone()) {}

std::vector<Tensor> MatMulBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad_2d = grad_output.ndim() == 1 ? grad_output.reshape({1, grad_output.shape()[0]}) : grad_output;
    Tensor a_2d = saved_a.ndim() == 1 ? saved_a.reshape({1, saved_a.shape()[0]}) : saved_a;
    Tensor b_2d = saved_b.ndim() == 1 ? saved_b.reshape({saved_b.shape()[0], 1}) : saved_b;

    Tensor grad_a = grad_2d.matmul(b_2d.T());
    Tensor grad_b = a_2d.T().matmul(grad_2d);

    if (saved_a.ndim() == 1) grad_a = grad_a.reshape(saved_a.shape());
    if (saved_b.ndim() == 1) grad_b = grad_b.reshape(saved_b.shape());

    return {grad_a, grad_b};
}

std::vector<Tensor> TransposeBackwardNode::backward(const Tensor& grad_output) {
    return {grad_output.T()};
}

ScalarMulBackwardNode::ScalarMulBackwardNode(double s) : scalar(s) {}

std::vector<Tensor> ScalarMulBackwardNode::backward(const Tensor& grad_output) {
    return {grad_output.scalar_mul(scalar)};
}

ScalarAddBackwardNode::ScalarAddBackwardNode() {}

std::vector<Tensor> ScalarAddBackwardNode::backward(const Tensor& grad_output) {
    return {grad_output.clone()};
}

SumBackwardNode::SumBackwardNode(const std::vector<int64_t>& shape, int64_t d, bool keep)
    : input_shape(shape), dim(d), keepdims(keep) {}

std::vector<Tensor> SumBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad = Tensor::zeros(input_shape, grad_output.dtype(), grad_output.device());
    if (dim == -1 || input_shape.size() == 1) {
        double val = grad_output.item();
        for (int64_t i = 0; i < grad.numel(); ++i) grad.set_flat(i, val);
    } else if (dim == 0 && input_shape.size() == 2) {
        for (int64_t r = 0; r < input_shape[0]; ++r) {
            for (int64_t c = 0; c < input_shape[1]; ++c) {
                grad.set({r, c}, grad_output.get_flat(c));
            }
        }
    } else if (dim == 1 && input_shape.size() == 2) {
        for (int64_t r = 0; r < input_shape[0]; ++r) {
            for (int64_t c = 0; c < input_shape[1]; ++c) {
                grad.set({r, c}, grad_output.get_flat(r));
            }
        }
    }
    return {grad};
}

MeanBackwardNode::MeanBackwardNode(const std::vector<int64_t>& shape, int64_t d, bool keep, double cnt)
    : input_shape(shape), dim(d), keepdims(keep), count(cnt) {}

std::vector<Tensor> MeanBackwardNode::backward(const Tensor& grad_output) {
    Tensor sum_grad = SumBackwardNode(input_shape, dim, keepdims).backward(grad_output)[0];
    return {sum_grad.scalar_mul(1.0 / std::max(1.0, count))};
}

ReLUBackwardNode::ReLUBackwardNode(const Tensor& in) : saved_input(in.clone()) {}

std::vector<Tensor> ReLUBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad(saved_input.shape(), saved_input.dtype(), saved_input.device());
    for (int64_t i = 0; i < saved_input.numel(); ++i) {
        double in_val = saved_input.get_flat(i);
        grad.set_flat(i, in_val > 0.0 ? grad_output.get_flat(i) : 0.0);
    }
    return {grad};
}

SigmoidBackwardNode::SigmoidBackwardNode(const Tensor& out) : saved_output(out.clone()) {}

std::vector<Tensor> SigmoidBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad(saved_output.shape(), saved_output.dtype(), saved_output.device());
    for (int64_t i = 0; i < saved_output.numel(); ++i) {
        double y = saved_output.get_flat(i);
        grad.set_flat(i, grad_output.get_flat(i) * y * (1.0 - y));
    }
    return {grad};
}

TanhBackwardNode::TanhBackwardNode(const Tensor& out) : saved_output(out.clone()) {}

std::vector<Tensor> TanhBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad(saved_output.shape(), saved_output.dtype(), saved_output.device());
    for (int64_t i = 0; i < saved_output.numel(); ++i) {
        double y = saved_output.get_flat(i);
        grad.set_flat(i, grad_output.get_flat(i) * (1.0 - y * y));
    }
    return {grad};
}

SoftmaxBackwardNode::SoftmaxBackwardNode(const Tensor& out, int64_t d)
    : saved_output(out.clone()), dim(d) {}

std::vector<Tensor> SoftmaxBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad(saved_output.shape(), saved_output.dtype(), saved_output.device());
    if (saved_output.ndim() == 2) {
        int64_t rows = saved_output.shape()[0];
        int64_t cols = saved_output.shape()[1];
        for (int64_t r = 0; r < rows; ++r) {
            double sum_gy = 0.0;
            for (int64_t c = 0; c < cols; ++c) {
                sum_gy += grad_output.get({r, c}) * saved_output.get({r, c});
            }
            for (int64_t c = 0; c < cols; ++c) {
                double s = saved_output.get({r, c});
                grad.set({r, c}, s * (grad_output.get({r, c}) - sum_gy));
            }
        }
    } else {
        double sum_gy = 0.0;
        for (int64_t i = 0; i < saved_output.numel(); ++i) {
            sum_gy += grad_output.get_flat(i) * saved_output.get_flat(i);
        }
        for (int64_t i = 0; i < saved_output.numel(); ++i) {
            double s = saved_output.get_flat(i);
            grad.set_flat(i, s * (grad_output.get_flat(i) - sum_gy));
        }
    }
    return {grad};
}

LogBackwardNode::LogBackwardNode(const Tensor& in, double epsilon)
    : saved_input(in.clone()), eps(epsilon) {}

std::vector<Tensor> LogBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad(saved_input.shape(), saved_input.dtype(), saved_input.device());
    for (int64_t i = 0; i < saved_input.numel(); ++i) {
        double v = saved_input.get_flat(i);
        grad.set_flat(i, grad_output.get_flat(i) / (v + eps));
    }
    return {grad};
}

ExpBackwardNode::ExpBackwardNode(const Tensor& out) : saved_output(out.clone()) {}

std::vector<Tensor> ExpBackwardNode::backward(const Tensor& grad_output) {
    return {grad_output.mul(saved_output)};
}

PowBackwardNode::PowBackwardNode(const Tensor& in, double exp)
    : saved_input(in.clone()), exponent(exp) {}

std::vector<Tensor> PowBackwardNode::backward(const Tensor& grad_output) {
    Tensor grad(saved_input.shape(), saved_input.dtype(), saved_input.device());
    for (int64_t i = 0; i < saved_input.numel(); ++i) {
        double x = saved_input.get_flat(i);
        double d = exponent * std::pow(x, exponent - 1.0);
        grad.set_flat(i, grad_output.get_flat(i) * d);
    }
    return {grad};
}

ReshapeBackwardNode::ReshapeBackwardNode(const std::vector<int64_t>& shape)
    : orig_shape(shape) {}

std::vector<Tensor> ReshapeBackwardNode::backward(const Tensor& grad_output) {
    return {grad_output.reshape(orig_shape)};
}

// ============================================================================
// Tensor Autograd Methods
// ============================================================================

void Tensor::backward(const Tensor& grad_output) {
    if (!requires_grad()) return;

    Tensor root_grad = (grad_output.numel() > 0) ? grad_output.clone() : Tensor::ones(shape_, dtype_, device_);
    set_grad(std::make_shared<Tensor>(root_grad));

    if (!grad_fn()) {
        return;
    }

    // Topological sort (DFS post-order) of autograd computation graph
    std::vector<std::shared_ptr<AutogradNode>> topo_order;
    std::unordered_set<AutogradNode*> visited;

    std::function<void(const std::shared_ptr<AutogradNode>&)> dfs = [&](const std::shared_ptr<AutogradNode>& node) {
        if (!node || visited.count(node.get())) return;
        visited.insert(node.get());
        for (const auto& inp : node->inputs) {
            if (inp && inp->grad_fn()) {
                dfs(inp->grad_fn());
            }
        }
        topo_order.push_back(node);
    };

    dfs(grad_fn());
    std::reverse(topo_order.begin(), topo_order.end());

    // Map storing accumulated gradient for each node during backward pass
    std::map<AutogradNode*, Tensor> node_grads;
    node_grads[grad_fn().get()] = root_grad;

    for (const auto& node : topo_order) {
        auto it = node_grads.find(node.get());
        if (it == node_grads.end()) continue;
        Tensor current_grad = it->second;

        // Compute input gradients for this node
        auto input_grads = node->backward(current_grad);

        for (size_t i = 0; i < node->inputs.size() && i < input_grads.size(); ++i) {
            auto& inp = node->inputs[i];
            if (!inp || !inp->requires_grad()) continue;

            const Tensor& g = input_grads[i];

            // 1. If inp is an intermediate node, route gradient to its grad_fn
            if (inp->grad_fn()) {
                AutogradNode* prev_node = inp->grad_fn().get();
                if (node_grads.find(prev_node) == node_grads.end()) {
                    node_grads[prev_node] = g;
                } else {
                    node_grads[prev_node] = node_grads[prev_node].add(g);
                }
            }

            // 2. Accumulate gradient into inp->grad() for leaf tensors
            if (inp->is_leaf() || !inp->grad_fn()) {
                if (!inp->grad()) {
                    inp->set_grad(std::make_shared<Tensor>(g.clone()));
                } else {
                    *inp->grad() = inp->grad()->add(g);
                }
            }
        }
    }
}

void Tensor::zero_grad() {
    if (autograd_meta_) {
        autograd_meta_->grad = nullptr;
    }
}

Tensor Tensor::detach() const {
    Tensor res = clone();
    res.set_requires_grad(false);
    res.set_is_leaf(true);
    res.set_grad_fn(nullptr);
    return res;
}

// ============================================================================
// Numerical Gradient Checker
// ============================================================================

double check_numerical_gradient(const std::function<double(const Tensor&)>& fn,
                                const Tensor& input,
                                const Tensor& analytic_grad,
                                double eps) {
    AutogradContext::NoGradGuard guard;
    double max_rel_err = 0.0;
    Tensor x_plus = input.clone();
    Tensor x_minus = input.clone();
    x_plus.set_requires_grad(false);
    x_minus.set_requires_grad(false);

    for (int64_t i = 0; i < input.numel(); ++i) {
        double orig_val = input.get_flat(i);

        x_plus.set_flat(i, orig_val + eps);
        double f_plus = fn(x_plus);

        x_minus.set_flat(i, orig_val - eps);
        double f_minus = fn(x_minus);

        x_plus.set_flat(i, orig_val);
        x_minus.set_flat(i, orig_val);

        double num_grad = (f_plus - f_minus) / (2.0 * eps);
        double ana_grad = analytic_grad.get_flat(i);

        double denom = std::max({std::abs(num_grad), std::abs(ana_grad), 1.0});
        double rel_err = std::abs(num_grad - ana_grad) / denom;
        max_rel_err = std::max(max_rel_err, rel_err);
    }
    return max_rel_err;
}

} // namespace nextviper
