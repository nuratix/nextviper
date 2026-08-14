#include "nextviper/ai_metrics.hpp"
#include <cmath>
#include <algorithm>

namespace nextviper {

double Metrics::accuracy(const Tensor& pred, const Tensor& target) {
    if (pred.numel() == 0 || target.numel() == 0) return 0.0;

    int64_t correct = 0;
    int64_t total = 0;

    if (pred.ndim() == 2 && target.ndim() == 1) {
        Tensor pred_cls = pred.argmax(1);
        total = pred.shape()[0];
        for (int64_t i = 0; i < total; ++i) {
            if (static_cast<int64_t>(pred_cls.get_flat(i)) == static_cast<int64_t>(target.get_flat(i))) {
                correct++;
            }
        }
    } else if (pred.ndim() == 2 && target.ndim() == 2) {
        if (target.shape()[1] > 1) {
            Tensor pred_cls = pred.argmax(1);
            Tensor target_cls = target.argmax(1);
            total = pred.shape()[0];
            for (int64_t i = 0; i < total; ++i) {
                if (static_cast<int64_t>(pred_cls.get_flat(i)) == static_cast<int64_t>(target_cls.get_flat(i))) {
                    correct++;
                }
            }
        } else {
            total = pred.shape()[0];
            for (int64_t i = 0; i < total; ++i) {
                double p = pred.get_flat(i) >= 0.5 ? 1.0 : 0.0;
                double y = target.get_flat(i) >= 0.5 ? 1.0 : 0.0;
                if (p == y) correct++;
            }
        }
    } else {
        total = std::min(pred.numel(), target.numel());
        for (int64_t i = 0; i < total; ++i) {
            double p = pred.get_flat(i) >= 0.5 ? 1.0 : 0.0;
            double y = target.get_flat(i) >= 0.5 ? 1.0 : 0.0;
            if (p == y) correct++;
        }
    }

    return total > 0 ? (static_cast<double>(correct) / static_cast<double>(total)) : 0.0;
}

double Metrics::precision(const Tensor& pred, const Tensor& target) {
    int64_t tp = 0;
    int64_t fp = 0;
    int64_t n = std::min(pred.numel(), target.numel());

    for (int64_t i = 0; i < n; ++i) {
        bool p = pred.get_flat(i) >= 0.5;
        bool y = target.get_flat(i) >= 0.5;
        if (p && y) tp++;
        else if (p && !y) fp++;
    }

    return (tp + fp) > 0 ? (static_cast<double>(tp) / static_cast<double>(tp + fp)) : 0.0;
}

double Metrics::recall(const Tensor& pred, const Tensor& target) {
    int64_t tp = 0;
    int64_t fn = 0;
    int64_t n = std::min(pred.numel(), target.numel());

    for (int64_t i = 0; i < n; ++i) {
        bool p = pred.get_flat(i) >= 0.5;
        bool y = target.get_flat(i) >= 0.5;
        if (p && y) tp++;
        else if (!p && y) fn++;
    }

    return (tp + fn) > 0 ? (static_cast<double>(tp) / static_cast<double>(tp + fn)) : 0.0;
}

double Metrics::f1_score(const Tensor& pred, const Tensor& target) {
    double p = precision(pred, target);
    double r = recall(pred, target);
    return (p + r) > 0.0 ? (2.0 * p * r / (p + r)) : 0.0;
}

double Metrics::mae(const Tensor& pred, const Tensor& target) {
    int64_t n = std::min(pred.numel(), target.numel());
    if (n == 0) return 0.0;
    double total = 0.0;
    for (int64_t i = 0; i < n; ++i) {
        total += std::abs(pred.get_flat(i) - target.get_flat(i));
    }
    return total / static_cast<double>(n);
}

double Metrics::mse(const Tensor& pred, const Tensor& target) {
    int64_t n = std::min(pred.numel(), target.numel());
    if (n == 0) return 0.0;
    double total = 0.0;
    for (int64_t i = 0; i < n; ++i) {
        double diff = pred.get_flat(i) - target.get_flat(i);
        total += diff * diff;
    }
    return total / static_cast<double>(n);
}

double Metrics::compute(const std::string& name, const Tensor& pred, const Tensor& target) {
    if (name == "accuracy" || name == "acc") return accuracy(pred, target);
    if (name == "precision") return precision(pred, target);
    if (name == "recall") return recall(pred, target);
    if (name == "f1" || name == "f1_score") return f1_score(pred, target);
    if (name == "mae") return mae(pred, target);
    if (name == "mse") return mse(pred, target);
    return 0.0;
}

} // namespace nextviper
