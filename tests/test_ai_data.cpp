#include "test_runner.hpp"
#include "nextviper/tensor.hpp"
#include "nextviper/dataset.hpp"
#include "nextviper/ai_model.hpp"
#include "nextviper/ai_layers.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/interpreter.hpp"
#include <cmath>

using namespace nextviper;

static bool eval_code(const std::string& src, DiagnosticEngine& diag, Interpreter& interp) {
    Lexer lexer(src, "ai_test.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    if (diag.has_errors() || !program) return false;
    return interp.execute(*program);
}

NV_TEST(AIData, TensorCreationAndLinearAlgebra) {
    // 1. Creation & Reshape
    Tensor a = Tensor::from_vector({1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, {2, 3});
    NV_ASSERT_EQ(a.ndim(), 2);
    NV_ASSERT_EQ(a.shape()[0], 2);
    NV_ASSERT_EQ(a.shape()[1], 3);
    NV_ASSERT_EQ(a.numel(), 6);

    Tensor b = Tensor::from_vector({7.0, 8.0, 9.0, 10.0, 11.0, 12.0}, {3, 2});

    // 2. Matmul: [2, 3] @ [3, 2] -> [2, 2]
    // [1*7 + 2*9 + 3*11,  1*8 + 2*10 + 3*12] = [58, 64]
    // [4*7 + 5*9 + 6*11,  4*8 + 5*10 + 6*12] = [139, 154]
    Tensor c = a.matmul(b);
    NV_ASSERT_EQ(c.shape()[0], 2);
    NV_ASSERT_EQ(c.shape()[1], 2);
    NV_ASSERT_EQ(c.get({0, 0}), 58.0);
    NV_ASSERT_EQ(c.get({0, 1}), 64.0);
    NV_ASSERT_EQ(c.get({1, 0}), 139.0);
    NV_ASSERT_EQ(c.get({1, 1}), 154.0);

    // 3. Transpose & Reductions
    Tensor at = a.T();
    NV_ASSERT_EQ(at.shape()[0], 3);
    NV_ASSERT_EQ(at.shape()[1], 2);
    NV_ASSERT_EQ(at.get({0, 1}), 4.0);

    Tensor s = a.sum();
    NV_ASSERT_EQ(s.item(), 21.0);
    Tensor m = a.mean();
    NV_ASSERT_EQ(m.item(), 3.5);

    // 4. Activations
    Tensor neg({2}, {-2.0, 3.0});
    Tensor rel = neg.relu();
    NV_ASSERT_EQ(rel.get({0}), 0.0);
    NV_ASSERT_EQ(rel.get({1}), 3.0);
}

NV_TEST(AIData, DatasetLoadingCleaningAndSplitting) {
    std::string csv_data = 
        "name,age,score,active\n"
        "Alice,25,88.5,true\n"
        "Bob,null,72.0,false\n"
        "Charlie,30,null,true\n"
        "David,22,95.0,true\n"
        "Eva,28,81.0,false\n";

    Dataset ds = Dataset::from_csv(csv_data);
    NV_ASSERT_EQ(ds.num_rows(), 5);
    NV_ASSERT_EQ(ds.num_cols(), 4);

    // 1. Clean by dropping nulls
    Dataset clean_dropped = ds.clean(true);
    NV_ASSERT_EQ(clean_dropped.num_rows(), 3);

    // 2. Clean by filling mean
    Dataset clean_filled = ds.clean(false, "mean");
    NV_ASSERT_EQ(clean_filled.num_rows(), 5);

    // 3. Split
    auto [train_ds, test_ds] = clean_filled.split(0.8, 42);
    NV_ASSERT_EQ(train_ds.num_rows(), 4);
    NV_ASSERT_EQ(test_ds.num_rows(), 1);

    // 4. Select & Convert to Tensor
    Dataset subset = clean_dropped.select({"age", "score"});
    NV_ASSERT_EQ(subset.num_cols(), 2);
    Tensor t = subset.to_tensor();
    NV_ASSERT_EQ(t.ndim(), 2);
    NV_ASSERT_EQ(t.shape()[0], 3);
    NV_ASSERT_EQ(t.shape()[1], 2);

    // 5. DataLoader Mini-Batching
    DataLoader loader(clean_filled, 2, false);
    NV_ASSERT_EQ(loader.num_batches(), 3);
    auto batches = loader.batches();
    NV_ASSERT_EQ(batches.size(), 3);
    NV_ASSERT_EQ(batches[0].num_rows(), 2);
    NV_ASSERT_EQ(batches[1].num_rows(), 2);
    NV_ASSERT_EQ(batches[2].num_rows(), 1);
}

NV_TEST(AIData, AIModelTrainingInferenceAndSerialization) {
    // 1. Build a 2-layer Neural Network
    AIModel model;
    model.add_layer(std::make_shared<Dense>(2, 4, true, "relu"));
    model.add_layer(std::make_shared<Dense>(4, 1, true, "none"));

    // 2. Training Data (Linear relationship: y = 2*x1 + 3*x2)
    Tensor x({4, 2}, {
        1.0, 1.0,
        2.0, 1.0,
        1.0, 2.0,
        2.0, 2.0
    });
    Tensor y({4, 1}, {
        5.0,
        7.0,
        8.0,
        10.0
    });

    double initial_loss = model.train_step(x, y, 0.01);

    // Train for 50 steps
    for (int i = 0; i < 50; ++i) {
        model.train_step(x, y, 0.01);
    }

    double final_loss = model.train_step(x, y, 0.01);
    NV_ASSERT_TRUE(final_loss <= initial_loss);

    // 3. Serialization and Deserialization
    std::string model_file = "build/test_ai_model.nvmodel";
    model.save(model_file);

    AIModel loaded_model = AIModel::load(model_file);
    NV_ASSERT_EQ(loaded_model.sequential()->layers().size(), 2);

    Tensor pred_orig = model.predict(x);
    Tensor pred_loaded = loaded_model.predict(x);

    for (int64_t i = 0; i < pred_orig.numel(); ++i) {
        NV_ASSERT_TRUE(std::abs(pred_orig.get_flat(i) - pred_loaded.get_flat(i)) < 1e-5);
    }
}

NV_TEST(AIData, NextViperLanguageAIAndDataAPIs) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = 
        "import data\n"
        "import ai\n"
        "import tensor\n"
        "\n"
        "// 1. Data loading & cleaning\n"
        "let csv_text = \"x,y\\n1.0,2.0\\n2.0,null\\n3.0,6.0\\n4.0,8.0\"\n"
        "let dataset = data.from_csv(csv_text)\n"
        "let clean_ds = dataset.clean(true)\n"
        "let num_clean = clean_ds.num_rows\n"
        "\n"
        "// 2. AI Model & Tensor operations\n"
        "let t1 = ai.tensor([2, 2], [1.0, 2.0, 3.0, 4.0])\n"
        "let model = ai.linear(2, 2)\n"
        "let pred = model.predict(t1)\n"
        "let pred_ndim = pred.ndim\n";

    NV_ASSERT_TRUE(eval_code(src, diag, interp));

    auto rows_val = interp.globals()->get("num_clean");
    NV_ASSERT_TRUE(rows_val.has_value() && rows_val->is_int() && rows_val->as_int() == 3);

    auto ndim_val = interp.globals()->get("pred_ndim");
    NV_ASSERT_TRUE(ndim_val.has_value() && ndim_val->is_int() && ndim_val->as_int() == 2);
}
