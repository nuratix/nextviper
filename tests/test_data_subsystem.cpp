#include "test_runner.hpp"
#include "nextviper/data_subsystem.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/native_compiler.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/lexer.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

using namespace nextviper;

NV_TEST(DataSubsystem, SchemaAndColumnValidation) {
    std::vector<ColumnSchema> cols = {
        {"id", DataKind::INT64, false},
        {"name", DataKind::STRING, false},
        {"salary", DataKind::FLOAT64, true}
    };
    Schema schema(cols);

    NV_ASSERT_TRUE(schema.has_column("id"));
    NV_ASSERT_TRUE(schema.has_column("salary"));
    NV_ASSERT_FALSE(schema.has_column("missing_col"));
    NV_ASSERT_EQ(schema.get_column_index("name"), 1);

    std::vector<Value> col_vals = {
        Value::make_float(10.0),
        Value::make_float(20.0),
        Value::make_nil(),
        Value::make_float(30.0)
    };
    Column col("salary", DataKind::FLOAT64, col_vals);
    NV_ASSERT_EQ(col.count_nulls(), 1);
    NV_ASSERT_EQ(col.size(), 4);
    NV_ASSERT_TRUE(std::abs(col.mean() - 20.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(col.sum() - 60.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(col.min() - 10.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(col.max() - 30.0) < 1e-6);

    Column norm_col = col.normalize();
    NV_ASSERT_TRUE(norm_col.is_null(2));
    NV_ASSERT_TRUE(std::abs(norm_col.as_float(0) - 0.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(norm_col.as_float(3) - 1.0) < 1e-6);
}

NV_TEST(DataSubsystem, DataArrayOperationsAndVectorization) {
    auto arr = DataArray::arange(1.0, 6.0, 1.0); // [1, 2, 3, 4, 5]
    NV_ASSERT_EQ(arr.size(), 5);
    NV_ASSERT_TRUE(std::abs(arr.mean() - 3.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(arr.sum() - 15.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(arr.min() - 1.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(arr.max() - 5.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(arr.median() - 3.0) < 1e-6);
    NV_ASSERT_EQ(arr.argmax(), 4);
    NV_ASSERT_EQ(arr.argmin(), 0);

    auto scaled = arr.mul_scalar(2.0);
    NV_ASSERT_TRUE(std::abs(scaled.get(0) - 2.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(scaled.get(4) - 10.0) < 1e-6);

    auto added = arr.add(scaled); // [3, 6, 9, 12, 15]
    NV_ASSERT_TRUE(std::abs(added.get(2) - 9.0) < 1e-6);

    auto cumsum_arr = arr.cumsum(); // [1, 3, 6, 10, 15]
    NV_ASSERT_TRUE(std::abs(cumsum_arr.get(4) - 15.0) < 1e-6);

    auto norm = arr.normalize(0.0, 1.0);
    NV_ASSERT_TRUE(std::abs(norm.min() - 0.0) < 1e-6);
    NV_ASSERT_TRUE(std::abs(norm.max() - 1.0) < 1e-6);
}

NV_TEST(DataSubsystem, DataFrameCsvLoadingAndTypeInference) {
    std::string csv_data = 
        "name,age,salary,is_active\n"
        "Alice,30,75000.50,true\n"
        "Bob,25,null,false\n"
        "\"Charlie, Jr.\",40,120000.00,true\n"
        "Diana,35,90000.00,true\n";

    DataFrame df = DataFrame::from_csv(csv_data);
    NV_ASSERT_EQ(df.num_rows(), 4);
    NV_ASSERT_EQ(df.num_cols(), 4);

    NV_ASSERT_EQ(df.get(0, "name").as_string(), "Alice");
    NV_ASSERT_EQ(df.get(0, "age").as_int(), 30);
    NV_ASSERT_TRUE(std::abs(df.get(0, "salary").as_float() - 75000.50) < 1e-3);
    NV_ASSERT_TRUE(df.get(0, "is_active").as_bool());

    NV_ASSERT_TRUE(df.get(1, "salary").is_nil());
    NV_ASSERT_EQ(df.get(2, "name").as_string(), "Charlie, Jr.");
}

NV_TEST(DataSubsystem, DataFrameFilteringSelectingSorting) {
    std::vector<std::string> cols = {"name", "score", "age"};
    std::vector<std::vector<Value>> rows = {
        {Value::make_string("Alice"), Value::make_float(95.0), Value::make_int(22)},
        {Value::make_string("Bob"), Value::make_float(80.0), Value::make_int(28)},
        {Value::make_string("Charlie"), Value::make_float(88.0), Value::make_int(20)},
        {Value::make_string("Diana"), Value::make_float(92.0), Value::make_int(25)}
    };
    DataFrame df(cols, rows);

    // Select
    auto sub = df.select({"name", "score"});
    NV_ASSERT_EQ(sub.num_cols(), 2);
    NV_ASSERT_EQ(sub.num_rows(), 4);

    // Filter score > 85
    auto filtered = df.filter([](const std::map<std::string, Value>& row) {
        auto it = row.find("score");
        return it != row.end() && it->second.as_float() > 85.0;
    });
    NV_ASSERT_EQ(filtered.num_rows(), 3); // Alice (95), Charlie (88), Diana (92)

    // Sort by age ascending
    auto sorted = df.sort("age", true);
    NV_ASSERT_EQ(sorted.get(0, "name").as_string(), "Charlie"); // age 20
    NV_ASSERT_EQ(sorted.get(1, "name").as_string(), "Alice");   // age 22
    NV_ASSERT_EQ(sorted.get(3, "name").as_string(), "Bob");     // age 28
}

NV_TEST(DataSubsystem, DataFramePreprocessingAndMissingValues) {
    std::vector<std::string> cols = {"a", "b"};
    std::vector<std::vector<Value>> rows = {
        {Value::make_float(10.0), Value::make_float(100.0)},
        {Value::make_nil(), Value::make_float(200.0)},
        {Value::make_float(30.0), Value::make_nil()},
        {Value::make_float(40.0), Value::make_float(400.0)}
    };
    DataFrame df(cols, rows);

    auto clean_df = df.drop_missing();
    NV_ASSERT_EQ(clean_df.num_rows(), 2); // row 0 and row 3

    auto filled_df = df.fill_missing("a", Value::make_float(0.0));
    NV_ASSERT_TRUE(std::abs(filled_df.get(1, "a").as_float() - 0.0) < 1e-6);

    auto desc = clean_df.describe();
    NV_ASSERT_TRUE(desc.find("a") != desc.end());
    NV_ASSERT_TRUE(std::abs(desc["a"]["mean"] - 25.0) < 1e-6);
}

NV_TEST(DataSubsystem, DataFrameSplittingAndBatching) {
    std::vector<std::string> cols = {"val"};
    std::vector<std::vector<Value>> rows;
    for (int i = 0; i < 100; ++i) {
        rows.push_back({Value::make_int(i)});
    }
    DataFrame df(cols, rows);

    auto [train, test] = df.split(0.8, 42);
    NV_ASSERT_EQ(train.num_rows(), 80);
    NV_ASSERT_EQ(test.num_rows(), 20);

    auto batches = df.batches(32);
    NV_ASSERT_EQ(batches.size(), 4); // 32, 32, 32, 4
    NV_ASSERT_EQ(batches[0].num_rows(), 32);
    NV_ASSERT_EQ(batches[3].num_rows(), 4);
}

NV_TEST(DataSubsystem, InterpreterDataSubsystemEndToEnd) {
    std::string code = R"(
import data

let arr = data.arange(1.0, 6.0, 1.0)
let m = arr.mean()
let s = arr.sum()

let csv_text = "id,score\n1,10.0\n2,20.0\n3,30.0\n4,40.0\n"
let df = data.read_csv(csv_text)
let norm_df = df.normalize()
let split_pair = df.split(0.75, 42)
let train = split_pair[0]
let test = split_pair[1]

print("mean:", m)
print("sum:", s)
print("rows:", df.shape[0])
print("train_rows:", train.shape[0])
print("test_rows:", test.shape[0])
)";

    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Lexer lexer(code, "test_data.nv", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    NV_ASSERT_TRUE(program != nullptr);

    Interpreter interp(diag);
    std::stringstream ss;
    auto* old_buf = std::cout.rdbuf(ss.rdbuf());
    interp.execute(*program);
    std::cout.rdbuf(old_buf);

    std::string output = ss.str();
    NV_ASSERT_TRUE(output.find("mean: 3.0") != std::string::npos);
    NV_ASSERT_TRUE(output.find("sum: 15.0") != std::string::npos);
    NV_ASSERT_TRUE(output.find("rows: 4") != std::string::npos);
    NV_ASSERT_TRUE(output.find("train_rows: 3") != std::string::npos);
    NV_ASSERT_TRUE(output.find("test_rows: 1") != std::string::npos);
}
