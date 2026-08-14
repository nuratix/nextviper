#include "nextviper/ai_loss.hpp"
#include <cmath>
#include <algorithm>

namespace nextviper {

Value Loss::to_value() {
    std::map<std::string, Value> obj;
    obj["$type"] = Value::make_string(name());
    obj["name"] = Value::make_string(name());
    return Value::make_object(std::move(obj));
}

// ============================================================================
// MSE Loss
// ============================================================================

Tensor MSELoss::forward(const Tensor& pred, const Tensor& target) {
    Tensor diff = pred.sub(target);
    Tensor sq = diff.mul(diff);
    return sq.mean();
}

// ============================================================================
// MAE Loss
// ============================================================================

Tensor MAELoss::forward(const Tensor& pred, const Tensor& target) {
    Tensor diff = pred.sub(target).abs();
    return diff.mean();
}

// ============================================================================
// Binary Cross Entropy Loss
// ============================================================================

Tensor BCELoss::forward(const Tensor& pred, const Tensor& target) {
    // Loss = -mean(y * log(p + eps) + (1 - y) * log(1 - p + eps))
    Tensor p = pred.clamp(eps_, 1.0 - eps_);
    Tensor log_p = p.log(eps_);
    Tensor one_minus_p = p.scalar_mul(-1.0).scalar_add(1.0);
    Tensor log_one_minus_p = one_minus_p.log(eps_);

    Tensor one_minus_y = target.scalar_mul(-1.0).scalar_add(1.0);

    Tensor term1 = target.mul(log_p);
    Tensor term2 = one_minus_y.mul(log_one_minus_p);
    Tensor total = term1.add(term2).scalar_mul(-1.0);

    return total.mean();
}

// ============================================================================
// Categorical Cross Entropy Loss
// ============================================================================

Tensor CrossEntropyLoss::forward(const Tensor& pred, const Tensor& target) {
    // If target is 1D class indices or 2D one-hot
    Tensor probs = pred.softmax(pred.ndim() == 2 ? 1 : -1);
    Tensor log_probs = probs.log(eps_);

    if (target.shape() == pred.shape()) {
        // One-hot encoded target
        Tensor term = target.mul(log_probs).scalar_mul(-1.0);
        return term.sum(pred.ndim() == 2 ? 1 : -1).mean();
    } else if (target.ndim() == 1 && pred.ndim() == 2) {
        // Integer class index target
        int64_t n = pred.shape()[0];
        int64_t c = pred.shape()[1];
        Tensor one_hot = Tensor::zeros({n, c}, pred.dtype(), pred.device());
        for (int64_t i = 0; i < n; ++i) {
            int64_t cls = static_cast<int64_t>(target.get_flat(i));
            if (cls >= 0 && cls < c) {
                one_hot.set({i, cls}, 1.0);
            }
        }
        Tensor term = one_hot.mul(log_probs).scalar_mul(-1.0);
        return term.sum(1).mean();
    }

    Tensor term = target.mul(log_probs).scalar_mul(-1.0);
    return term.mean();
}

} // namespace nextviper
