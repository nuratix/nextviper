#include "test_runner.hpp"
#include "nextviper/tensor.hpp"
#include "nextviper/gpu_backend.hpp"
#include "nextviper/ai_layers.hpp"
#include "nextviper/ai_loss.hpp"
#include "nextviper/ai_optimizer.hpp"
#include "nextviper/ai_model.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/diagnostic.hpp"
#include "nextviper/interpreter.hpp"
#include <cmath>
#include <vector>

namespace nextviper {

NV_TEST(GPUSubsystem, DeviceDetectionAndProperties) {
    bool available = GPUTensorBackend::is_gpu_available();
    if (available) {
        std::string name = GPUTensorBackend::get_gpu_name();
        NV_ASSERT_TRUE(!name.empty());
        int count = GPUTensorBackend::instance().device_count();
        NV_ASSERT_TRUE(count >= 1);
        NV_ASSERT_TRUE(GPUTensorBackend::instance().total_memory() > 0);
    }
}

NV_TEST(GPUSubsystem, HostDeviceMemoryTransferRoundtrip) {
    if (!GPUTensorBackend::is_gpu_available()) return;

    Tensor cpu_t({2, 3}, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, DType::FLOAT32, Device::CPU);
    NV_ASSERT_TRUE(cpu_t.device() == Device::CPU);

    Tensor gpu_t = cpu_t.to(Device::GPU);
    NV_ASSERT_TRUE(gpu_t.device() == Device::GPU);
    NV_ASSERT_TRUE(gpu_t.shape() == std::vector<int64_t>({2, 3}));
    NV_ASSERT_TRUE(gpu_t.numel() == 6);

    Tensor back_t = gpu_t.to(Device::CPU);
    NV_ASSERT_TRUE(back_t.device() == Device::CPU);
    NV_ASSERT_TRUE(back_t.shape() == std::vector<int64_t>({2, 3}));
    for (int64_t i = 0; i < 6; ++i) {
        NV_ASSERT_TRUE(std::fabs(back_t.get_flat(i) - (i + 1.0)) < 1e-5);
    }
}

NV_TEST(GPUSubsystem, ElementwiseArithmeticGPUExecution) {
    if (!GPUTensorBackend::is_gpu_available()) return;

    Tensor a = Tensor({4}, {10.0, 20.0, 30.0, 40.0}, DType::FLOAT32, Device::GPU);
    Tensor b = Tensor({4}, {2.0, 4.0, 5.0, 8.0}, DType::FLOAT32, Device::GPU);

    // Addition
    Tensor c_add = a.add(b);
    NV_ASSERT_TRUE(c_add.device() == Device::GPU);
    Tensor c_add_cpu = c_add.to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(c_add_cpu.get_flat(0) - 12.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(c_add_cpu.get_flat(1) - 24.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(c_add_cpu.get_flat(2) - 35.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(c_add_cpu.get_flat(3) - 48.0) < 1e-5);

    // Subtraction
    Tensor c_sub = a.sub(b).to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(c_sub.get_flat(0) - 8.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(c_sub.get_flat(1) - 16.0) < 1e-5);

    // Multiplication
    Tensor c_mul = a.mul(b).to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(c_mul.get_flat(0) - 20.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(c_mul.get_flat(1) - 80.0) < 1e-5);

    // Division
    Tensor c_div = a.div(b).to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(c_div.get_flat(0) - 5.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(c_div.get_flat(1) - 5.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(c_div.get_flat(2) - 6.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(c_div.get_flat(3) - 5.0) < 1e-5);

    // Scalar Ops
    Tensor c_sadd = a.scalar_add(5.0).to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(c_sadd.get_flat(0) - 15.0) < 1e-5);

    Tensor c_smul = a.scalar_mul(2.0).to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(c_smul.get_flat(0) - 20.0) < 1e-5);
}

NV_TEST(GPUSubsystem, MatrixMultiplicationGEMM) {
    if (!GPUTensorBackend::is_gpu_available()) return;

    Tensor a = Tensor({2, 3}, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, DType::FLOAT32, Device::GPU);
    Tensor b = Tensor({3, 2}, {7.0, 8.0, 9.0, 1.0, 2.0, 3.0}, DType::FLOAT32, Device::GPU);

    Tensor c_gpu = a.matmul(b);
    NV_ASSERT_TRUE(c_gpu.device() == Device::GPU);
    NV_ASSERT_TRUE(c_gpu.shape() == std::vector<int64_t>({2, 2}));

    Tensor c_cpu = c_gpu.to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(c_cpu.get({0, 0}) - 31.0) < 1e-4);
    NV_ASSERT_TRUE(std::fabs(c_cpu.get({0, 1}) - 19.0) < 1e-4);
    NV_ASSERT_TRUE(std::fabs(c_cpu.get({1, 0}) - 85.0) < 1e-4);
    NV_ASSERT_TRUE(std::fabs(c_cpu.get({1, 1}) - 55.0) < 1e-4);
}

NV_TEST(GPUSubsystem, ReductionsAndActivations) {
    if (!GPUTensorBackend::is_gpu_available()) return;

    Tensor t = Tensor({4}, {-2.0, 0.0, 3.0, -5.0}, DType::FLOAT32, Device::GPU);

    // Reductions
    Tensor s = t.sum().to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(s.get_flat(0) - (-4.0)) < 1e-5);

    Tensor min_t = t.min().to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(min_t.get_flat(0) - (-5.0)) < 1e-5);

    Tensor max_t = t.max().to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(max_t.get_flat(0) - 3.0) < 1e-5);

    // Activations
    Tensor r = t.relu().to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(r.get_flat(0) - 0.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(r.get_flat(1) - 0.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(r.get_flat(2) - 3.0) < 1e-5);
    NV_ASSERT_TRUE(std::fabs(r.get_flat(3) - 0.0) < 1e-5);

    Tensor sig = t.sigmoid().to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(sig.get_flat(1) - 0.5) < 1e-5);

    Tensor tan = t.tanh().to(Device::CPU);
    NV_ASSERT_TRUE(std::fabs(tan.get_flat(1) - 0.0) < 1e-5);
}

NV_TEST(GPUSubsystem, GPUAutogradAndTrainingConvergence) {
    if (!GPUTensorBackend::is_gpu_available()) return;

    Tensor x = Tensor({2, 2}, {1.0, 2.0, 3.0, 4.0}, DType::FLOAT32, Device::GPU);
    Tensor target = Tensor({2, 1}, {5.0, 11.0}, DType::FLOAT32, Device::GPU);

    auto model = std::make_shared<Sequential>(std::vector<std::shared_ptr<Module>>{
        std::make_shared<Dense>(2, 1, false)
    });
    model->to(Device::GPU);
    NV_ASSERT_TRUE(model->device() == Device::GPU);

    auto opt = std::make_shared<Adam>(model->trainable_parameters(), 0.05);
    auto loss_fn = std::make_shared<MSELoss>();
    model->compile(opt, loss_fn);

    double initial_loss = 0.0;
    double final_loss = 0.0;

    for (int epoch = 0; epoch < 200; ++epoch) {
        model->zero_grad();
        Tensor pred = model->forward(x);
        Tensor loss = loss_fn->forward(pred, target);
        if (epoch == 0) initial_loss = loss.item();
        if (epoch == 199) final_loss = loss.item();
        loss.backward();
        opt->step();
    }

    NV_ASSERT_TRUE(final_loss < initial_loss);
    NV_ASSERT_TRUE(final_loss < 0.1);
}

NV_TEST(GPUSubsystem, AutoDeviceSelectionFallback) {
    Tensor t = Tensor::zeros({2, 2}, DType::FLOAT32, Device::AUTO);
    if (GPUTensorBackend::is_gpu_available()) {
        NV_ASSERT_TRUE(t.device() == Device::GPU);
    } else {
        NV_ASSERT_TRUE(t.device() == Device::CPU);
    }
}

NV_TEST(GPUSubsystem, NextViperInterpreterGPUWorkflow) {
    std::string source = R"(
        import tensor
        import ai

        let gpu_ok = tensor.is_gpu_available()
        print("GPU status:", gpu_ok)
        
        let t_cpu = tensor.create([1.0, 2.0, 3.0, 4.0])
        print("CPU tensor:", t_cpu.device())

        if gpu_ok {
            let t_gpu = t_cpu.to("gpu")
            print("GPU tensor device:", t_gpu.device())
            let t_add = t_gpu.add(t_gpu)
            print("GPU add result:", t_add.device())
        }

        let interp_gpu_success = true
    )";

    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Lexer lexer(source, "test_gpu.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    NV_ASSERT_TRUE(program != nullptr);

    Interpreter interp(diag);
    bool ok = interp.execute(*program);
    NV_ASSERT_TRUE(ok);

    auto success_val = interp.globals()->get("interp_gpu_success");
    NV_ASSERT_TRUE(success_val.has_value() && success_val->is_bool() && success_val->as_bool());
}

} // namespace nextviper
