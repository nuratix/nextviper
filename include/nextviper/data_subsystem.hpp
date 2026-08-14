#pragma once

#include "nextviper/common.hpp"
#include "nextviper/value.hpp"
#include "nextviper/tensor.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <functional>

namespace nextviper {

// ============================================================================
// Data Subsystem: Type System & Schema
// ============================================================================

enum class DataKind {
    INT64,
    FLOAT64,
    STRING,
    BOOL,
    NULL_VAL
};

std::string data_kind_to_string(DataKind kind);
DataKind infer_data_kind(const Value& val);

struct ColumnSchema {
    std::string name;
    DataKind kind = DataKind::FLOAT64;
    bool nullable = true;

    ColumnSchema() = default;
    ColumnSchema(std::string name, DataKind kind, bool nullable = true)
        : name(std::move(name)), kind(kind), nullable(nullable) {}

    std::string to_string() const;
};

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<ColumnSchema> columns);

    const std::vector<ColumnSchema>& columns() const { return columns_; }
    size_t num_columns() const { return columns_.size(); }
    bool has_column(const std::string& name) const;
    const ColumnSchema* get_column(const std::string& name) const;
    int get_column_index(const std::string& name) const;

    Value to_value() const;

private:
    std::vector<ColumnSchema> columns_;
    std::map<std::string, size_t> column_indices_;
};

// ============================================================================
// Data Subsystem: Column
// ============================================================================

class Column {
public:
    Column(std::string name, DataKind kind, std::vector<Value> values);

    const std::string& name() const { return name_; }
    DataKind kind() const { return kind_; }
    size_t size() const { return values_.size(); }
    const std::vector<Value>& values() const { return values_; }

    const Value& get(size_t index) const;
    bool is_null(size_t index) const;

    double as_float(size_t index) const;
    int64_t as_int(size_t index) const;
    std::string as_string(size_t index) const;
    bool as_bool(size_t index) const;

    // Reductions & Stats
    double mean() const;
    double sum() const;
    double min() const;
    double max() const;
    double std_dev() const;
    double variance() const;
    size_t count_nulls() const;
    std::vector<Value> unique() const;

    // Transformations
    Column cast(DataKind new_kind) const;
    Column normalize() const;
    Column standardize() const;

    Value to_value() const;

private:
    std::string name_;
    DataKind kind_;
    std::vector<Value> values_;
};

// ============================================================================
// Data Subsystem: Numerical Array
// ============================================================================

class DataArray {
public:
    DataArray();
    explicit DataArray(std::vector<double> data, std::vector<size_t> shape = {});
    static DataArray from_values(const std::vector<Value>& values);
    static DataArray zeros(const std::vector<size_t>& shape);
    static DataArray ones(const std::vector<size_t>& shape);
    static DataArray arange(double start, double stop, double step = 1.0);
    static DataArray linspace(double start, double stop, size_t num);

    // Properties
    size_t size() const { return data_.size(); }
    const std::vector<size_t>& shape() const { return shape_; }
    size_t ndim() const { return shape_.size(); }
    const std::vector<double>& data() const { return data_; }
    std::vector<double>& data() { return data_; }

    double get(size_t index) const;
    void set(size_t index, double value);

    // Reductions & Summary
    double mean() const;
    double sum() const;
    double min() const;
    double max() const;
    double std_dev() const;
    double variance() const;
    double median() const;
    size_t argmax() const;
    size_t argmin() const;

    // Vectorized arithmetic
    DataArray add(const DataArray& other) const;
    DataArray sub(const DataArray& other) const;
    DataArray mul(const DataArray& other) const;
    DataArray div(const DataArray& other) const;
    DataArray add_scalar(double scalar) const;
    DataArray mul_scalar(double scalar) const;
    DataArray pow(double exponent) const;
    DataArray abs() const;
    DataArray exp() const;
    DataArray log() const;
    DataArray sqrt() const;
    DataArray cumsum() const;

    // Preprocessing
    DataArray normalize(double min_val = 0.0, double max_val = 1.0) const;
    DataArray standardize() const;
    DataArray clip(double low, double high) const;
    DataArray reshape(std::vector<size_t> new_shape) const;

    Value to_value() const;
    Tensor to_tensor() const;

private:
    std::vector<double> data_;
    std::vector<size_t> shape_;
};

// ============================================================================
// Data Subsystem: DataFrame & Dataset
// ============================================================================

class DataFrame {
public:
    DataFrame();
    DataFrame(std::vector<std::string> column_names, std::vector<std::vector<Value>> rows);
    DataFrame(Schema schema, std::vector<std::vector<Value>> rows);

    // Factories & I/O
    static DataFrame load(const std::string& path);
    static DataFrame from_csv(const std::string& csv_content, bool has_header = true, char delimiter = ',');
    static DataFrame from_json(const std::string& json_content);
    static DataFrame from_records(const std::vector<Value>& records);

    std::string to_csv(char delimiter = ',') const;
    std::string to_json(int indent = 0) const;

    // Properties
    size_t num_rows() const { return rows_.size(); }
    size_t num_cols() const { return columns_.size(); }
    std::vector<size_t> shape() const { return {num_rows(), num_cols()}; }
    const std::vector<std::string>& columns() const { return columns_; }
    const Schema& schema() const { return schema_; }
    const std::vector<std::vector<Value>>& rows() const { return rows_; }

    // Column & Row Access
    std::optional<Column> get_column(const std::string& name) const;
    std::vector<Value> get_row(size_t index) const;
    Value get(size_t row, const std::string& col) const;

    // Projection & Slicing
    DataFrame select(const std::vector<std::string>& column_names) const;
    DataFrame drop(const std::vector<std::string>& column_names) const;
    DataFrame head(size_t n = 5) const;
    DataFrame tail(size_t n = 5) const;
    DataFrame slice(size_t start, size_t end) const;

    // Filtering & Transformation
    DataFrame filter(const std::function<bool(const std::map<std::string, Value>&)>& predicate) const;
    DataFrame sort(const std::string& column_name, bool ascending = true) const;
    DataFrame rename_column(const std::string& old_name, const std::string& new_name) const;

    // Preprocessing
    DataFrame clean(bool drop_nulls = true, const std::string& fill_strategy = "mean") const;
    DataFrame drop_missing(const std::vector<std::string>& columns = {}) const;
    DataFrame fill_missing(const std::string& column_name, const Value& value) const;
    DataFrame normalize(const std::vector<std::string>& columns = {}) const;
    DataFrame standardize(const std::vector<std::string>& columns = {}) const;
    DataFrame shuffle(uint32_t seed = 42) const;

    // Splitting & Sampling
    std::pair<DataFrame, DataFrame> split(double train_ratio = 0.8, uint32_t seed = 42) const;
    DataFrame sample(size_t n, uint32_t seed = 42) const;
    DataFrame sample_fraction(double frac, uint32_t seed = 42) const;
    std::vector<DataFrame> batches(size_t batch_size = 32, bool shuffle = false) const;

    // Summary Statistics
    std::map<std::string, std::map<std::string, double>> describe() const;
    DataArray column_to_array(const std::string& column_name) const;
    Tensor to_tensor(const std::vector<std::string>& column_names = {}) const;

    Value to_value() const;

private:
    std::vector<std::string> columns_;
    Schema schema_;
    std::vector<std::vector<Value>> rows_;

    void build_schema();
    int get_column_index(const std::string& col_name) const;
};

// ============================================================================
// Data Module Factory & Registration
// ============================================================================

Value create_data_module();

} // namespace nextviper
