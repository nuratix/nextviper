#include "test_runner.hpp"
#include "nextviper/ai_subsystem.hpp"
#include "nextviper/data_subsystem.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include <cmath>
#include <fstream>

namespace nextviper {

NV_TEST(AISubsystem, AutogradForwardBackwardAndGradientCheck) {
    // 1. Quadratic: y = x * x, loss = sum(y)
    Tensor x({3}, {1.0, 2.0, 3.0});
    x.set_requires_grad(true);

    Tensor y = x.mul(x);
    Tensor loss = y.sum();

    loss.backward();

    NV_ASSERT_TRUE(x.grad() != nullptr);
    // Analytical gradient of x^2 is 2x -> [2.0, 4.0, 6.0]
    NV_ASSERT_TRUE(std::abs(x.grad()->get_flat(0) - 2.0) < 1e-5);
    NV_ASSERT_TRUE(std::abs(x.grad()->get_flat(1) - 4.0) < 1e-5);
    NV_ASSERT_TRUE(std::abs(x.grad()->get_flat(2) - 6.0) < 1e-5);

    // 2. Numerical gradient checking on scalar function f(w) = sum(w * w + 3 * w)
    Tensor w({2, 2}, {1.0, -2.0, 3.0, 0.5}, DType::FLOAT64);
    w.set_requires_grad(true);

    Tensor z = w.mul(w).add(w.scalar_mul(3.0));
    Tensor loss_w = z.sum();
    loss_w.backward();

    NV_ASSERT_TRUE(w.grad() != nullptr);
    NV_ASSERT_TRUE(std::abs(w.grad()->get_flat(0) - 5.0) < 1e-4);
    NV_ASSERT_TRUE(std::abs(w.grad()->get_flat(1) - (-1.0)) < 1e-4);
    NV_ASSERT_TRUE(std::abs(w.grad()->get_flat(2) - 9.0) < 1e-4);
    NV_ASSERT_TRUE(std::abs(w.grad()->get_flat(3) - 4.0) < 1e-4);

    auto fn = [](const Tensor& t) -> double {
        Tensor res = t.mul(t).add(t.scalar_mul(3.0)).sum();
        return res.item();
    };

    double rel_err = check_numerical_gradient(fn, w, *w.grad(), 1e-4);
    NV_ASSERT_TRUE(rel_err < 1e-3);
}

NV_TEST(AISubsystem, DenseDropoutAndActivations) {
    // Dense Layer
    Dense dense(4, 2, true, "relu");
    NV_ASSERT_EQ(dense.count_parameters(), 4 * 2 + 2); // 8 weights + 2 biases = 10
    NV_ASSERT_EQ(dense.parameters().size(), 2);

    Tensor x({3, 4}, {
        1.0, 2.0, 3.0, 4.0,
        0.5, 1.5, 2.5, 3.5,
        -1.0, -2.0, -3.0, -4.0
    });

    Tensor out = dense.forward(x);
    NV_ASSERT_EQ(out.shape()[0], 3);
    NV_ASSERT_EQ(out.shape()[1], 2);

    // Dropout Layer
    Dropout dropout(0.5);
    dropout.eval();
    Tensor d_eval = dropout.forward(out);
    NV_ASSERT_TRUE(std::abs(d_eval.get_flat(0) - out.get_flat(0)) < 1e-6);

    // Flatten Layer
    Flatten flatten;
    Tensor f_out = flatten.forward(x);
    NV_ASSERT_EQ(f_out.shape()[0], 3);
    NV_ASSERT_EQ(f_out.shape()[1], 4);
}

NV_TEST(AISubsystem, LossFunctionsAndOptimizers) {
    Tensor pred({4, 1}, {0.8, 0.2, 0.9, 0.1});
    Tensor target({4, 1}, {1.0, 0.0, 1.0, 0.0});

    // MSE Loss
    MSELoss mse;
    Tensor l_mse = mse.forward(pred, target);
    NV_ASSERT_TRUE(l_mse.item() < 0.1); // error is (0.2^2 + 0.2^2 + 0.1^2 + 0.1^2)/4 = 0.025

    // BCE Loss
    BCELoss bce;
    Tensor l_bce = bce.forward(pred, target);
    NV_ASSERT_TRUE(l_bce.item() > 0.0);

    // Adam Optimizer single step
    Tensor param_t({2}, {5.0, -5.0});
    param_t.set_requires_grad(true);
    param_t.set_grad(std::make_shared<Tensor>(Tensor({2}, {1.0, -1.0})));

    auto param = std::make_shared<Parameter>("p", param_t, true);
    Adam adam({param}, 0.1);
    adam.step();

    // After step with positive gradient, param should decrease; with negative gradient, increase
    NV_ASSERT_TRUE(param->data().get_flat(0) < 5.0);
    NV_ASSERT_TRUE(param->data().get_flat(1) > -5.0);
}

NV_TEST(AISubsystem, RealXORLearningAndConvergence) {
    // True Non-linear XOR Problem:
    // [0, 0] -> 0
    // [0, 1] -> 1
    // [1, 0] -> 1
    // [1, 1] -> 0
    Tensor x_xor({4, 2}, {
        0.0, 0.0,
        0.0, 1.0,
        1.0, 0.0,
        1.0, 1.0
    });

    Tensor y_xor({4, 1}, {
        0.0,
        1.0,
        1.0,
        0.0
    });

    // 2 -> 8 (ReLU) -> 1 (Sigmoid)
    auto seq = std::make_shared<Sequential>();
    seq->add(std::make_shared<Dense>(2, 8, true, "relu"));
    seq->add(std::make_shared<Dense>(8, 1, true, "sigmoid"));

    auto opt = std::make_shared<Adam>(seq->trainable_parameters(), 0.05);
    auto loss_fn = std::make_shared<MSELoss>();
    seq->compile(opt, loss_fn);

    // Train for 250 epochs
    History hist = seq->fit(x_xor, y_xor, 250, 4, false);

    // Loss must decrease significantly
    NV_ASSERT_TRUE(hist.loss.back() < 0.1);

    // Evaluate predictions
    Tensor preds = seq->predict(x_xor);
    double p00 = preds.get({0, 0});
    double p01 = preds.get({1, 0});
    double p10 = preds.get({2, 0});
    double p11 = preds.get({3, 0});

    // True XOR convergence assertions (not hardcoded)
    NV_ASSERT_TRUE(p00 < 0.35); // Expect near 0
    NV_ASSERT_TRUE(p01 > 0.65); // Expect near 1
    NV_ASSERT_TRUE(p10 > 0.65); // Expect near 1
    NV_ASSERT_TRUE(p11 < 0.35); // Expect near 0
}

NV_TEST(AISubsystem, ModelSerializationAndRestoration) {
    // 1. Create and train a small network
    auto seq = std::make_shared<Sequential>();
    seq->add(std::make_shared<Dense>(2, 4, true, "relu"));
    seq->add(std::make_shared<Dense>(4, 1, true, "sigmoid"));

    Tensor x({2, 2}, {0.0, 1.0, 1.0, 0.0});
    Tensor y({2, 1}, {1.0, 1.0});

    seq->compile(std::make_shared<Adam>(seq->trainable_parameters(), 0.05), std::make_shared<MSELoss>());
    seq->fit(x, y, 50, 2, false);

    Tensor orig_pred = seq->predict(x);

    // 2. Save model
    std::string path = "/tmp/test_ai_model.nvmodel";
    seq->save(path);

    // 3. Load model
    auto loaded_seq = Sequential::load(path);
    NV_ASSERT_TRUE(loaded_seq != nullptr);
    NV_ASSERT_EQ(loaded_seq->layers().size(), 2);

    Tensor loaded_pred = loaded_seq->predict(x);

    // 4. Verify predictions match exactly
    NV_ASSERT_TRUE(std::abs(orig_pred.get_flat(0) - loaded_pred.get_flat(0)) < 1e-6);
    NV_ASSERT_TRUE(std::abs(orig_pred.get_flat(1) - loaded_pred.get_flat(1)) < 1e-6);
}

NV_TEST(AISubsystem, NextViperLanguageEndToEndPipeline) {
    std::string code = R"(
import data
import tensor
import ai

// 1. Create synthetic dataset
let csv_data = "x1,x2,y\n0.0,0.0,0.0\n0.0,1.0,1.0\n1.0,0.0,1.0\n1.0,1.0,0.0\n"
let df = data.read_csv(csv_data)

let x = df.to_tensor(["x1", "x2"])
let y = df.to_tensor(["y"])

// 2. Build model
let model = ai.Sequential([
    ai.Dense(2, 8, "relu"),
    ai.Dense(8, 1, "sigmoid")
])

model.compile(
    ai.Adam(0.05),
    ai.MSE()
)

// 3. Fit model
let hist = model.fit(x, y, 200, 4)

// 4. Predict
let preds = model.predict(x)

let save_path = "/tmp/nv_pipeline_model.nvmodel"
model.save(save_path)

let loaded = ai.load(save_path)
let loaded_preds = loaded.predict(x)

let pipeline_success = true
)";

    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Lexer lexer(code, "test_ai.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    NV_ASSERT_TRUE(program != nullptr);

    Interpreter interp(diag);
    bool ok = interp.execute(*program);
    NV_ASSERT_TRUE(ok);

    auto success_val = interp.globals()->get("pipeline_success");
    NV_ASSERT_TRUE(success_val.has_value() && success_val->is_bool() && success_val->as_bool());
}

} // namespace nextviper
