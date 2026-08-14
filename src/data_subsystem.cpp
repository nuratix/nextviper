#include "nextviper/data_subsystem.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <random>
#include <iomanip>
#include <stdexcept>
#include <cctype>

namespace nextviper {

// ============================================================================
// DataKind & Schema Implementation
// ============================================================================

std::string data_kind_to_string(DataKind kind) {
    switch (kind) {
        case DataKind::INT64: return "int64";
        case DataKind::FLOAT64: return "float64";
        case DataKind::STRING: return "string";
        case DataKind::BOOL: return "bool";
        case DataKind::NULL_VAL: return "null";
    }
    return "unknown";
}

DataKind infer_data_kind(const Value& val) {
    if (val.is_int()) return DataKind::INT64;
    if (val.is_float()) return DataKind::FLOAT64;
    if (val.is_string()) return DataKind::STRING;
    if (val.is_bool()) return DataKind::BOOL;
    return DataKind::NULL_VAL;
}

std::string ColumnSchema::to_string() const {
    return name + ": " + data_kind_to_string(kind) + (nullable ? " (nullable)" : "");
}

Schema::Schema(std::vector<ColumnSchema> columns)
    : columns_(std::move(columns)) {
    for (size_t i = 0; i < columns_.size(); ++i) {
        column_indices_[columns_[i].name] = i;
    }
}

bool Schema::has_column(const std::string& name) const {
    return column_indices_.find(name) != column_indices_.end();
}

const ColumnSchema* Schema::get_column(const std::string& name) const {
    auto it = column_indices_.find(name);
    if (it != column_indices_.end()) {
        return &columns_[it->second];
    }
    return nullptr;
}

int Schema::get_column_index(const std::string& name) const {
    auto it = column_indices_.find(name);
    if (it != column_indices_.end()) {
        return static_cast<int>(it->second);
    }
    return -1;
}

Value Schema::to_value() const {
    std::map<std::string, Value> obj;
    std::vector<Value> cols_arr;
    for (const auto& col : columns_) {
        std::map<std::string, Value> col_obj;
        col_obj["name"] = Value::make_string(col.name);
        col_obj["type"] = Value::make_string(data_kind_to_string(col.kind));
        col_obj["nullable"] = Value::make_bool(col.nullable);
        cols_arr.push_back(Value::make_object(std::move(col_obj)));
    }
    obj["columns"] = Value::make_array(std::move(cols_arr));
    return Value::make_object(std::move(obj));
}

// ============================================================================
// Column Implementation
// ============================================================================

Column::Column(std::string name, DataKind kind, std::vector<Value> values)
    : name_(std::move(name)), kind_(kind), values_(std::move(values)) {}

const Value& Column::get(size_t index) const {
    if (index >= values_.size()) {
        static Value nil_val = Value::make_nil();
        return nil_val;
    }
    return values_[index];
}

bool Column::is_null(size_t index) const {
    if (index >= values_.size()) return true;
    return values_[index].is_nil();
}

double Column::as_float(size_t index) const {
    const auto& v = get(index);
    if (v.is_float()) return v.as_float();
    if (v.is_int()) return static_cast<double>(v.as_int());
    if (v.is_string()) {
        try { return std::stod(v.as_string()); } catch (...) { return 0.0; }
    }
    return 0.0;
}

int64_t Column::as_int(size_t index) const {
    const auto& v = get(index);
    if (v.is_int()) return v.as_int();
    if (v.is_float()) return static_cast<int64_t>(v.as_float());
    if (v.is_string()) {
        try { return std::stoll(v.as_string()); } catch (...) { return 0; }
    }
    return 0;
}

std::string Column::as_string(size_t index) const {
    const auto& v = get(index);
    if (v.is_string()) return v.as_string();
    return v.to_string();
}

bool Column::as_bool(size_t index) const {
    const auto& v = get(index);
    return v.is_truthy();
}

double Column::mean() const {
    double total = 0.0;
    size_t count = 0;
    for (size_t i = 0; i < values_.size(); ++i) {
        if (!is_null(i)) {
            total += as_float(i);
            count++;
        }
    }
    return count > 0 ? (total / count) : 0.0;
}

double Column::sum() const {
    double total = 0.0;
    for (size_t i = 0; i < values_.size(); ++i) {
        if (!is_null(i)) total += as_float(i);
    }
    return total;
}

double Column::min() const {
    double min_val = std::numeric_limits<double>::infinity();
    bool found = false;
    for (size_t i = 0; i < values_.size(); ++i) {
        if (!is_null(i)) {
            min_val = std::min(min_val, as_float(i));
            found = true;
        }
    }
    return found ? min_val : 0.0;
}

double Column::max() const {
    double max_val = -std::numeric_limits<double>::infinity();
    bool found = false;
    for (size_t i = 0; i < values_.size(); ++i) {
        if (!is_null(i)) {
            max_val = std::max(max_val, as_float(i));
            found = true;
        }
    }
    return found ? max_val : 0.0;
}

double Column::variance() const {
    double avg = mean();
    double sum_sq = 0.0;
    size_t count = 0;
    for (size_t i = 0; i < values_.size(); ++i) {
        if (!is_null(i)) {
            double diff = as_float(i) - avg;
            sum_sq += diff * diff;
            count++;
        }
    }
    return count > 1 ? (sum_sq / (count - 1)) : 0.0;
}

double Column::std_dev() const {
    return std::sqrt(variance());
}

size_t Column::count_nulls() const {
    size_t count = 0;
    for (const auto& v : values_) {
        if (v.is_nil()) count++;
    }
    return count;
}

std::vector<Value> Column::unique() const {
    std::vector<Value> unq;
    for (const auto& v : values_) {
        if (std::find(unq.begin(), unq.end(), v) == unq.end()) {
            unq.push_back(v);
        }
    }
    return unq;
}

Column Column::cast(DataKind new_kind) const {
    std::vector<Value> new_vals;
    new_vals.reserve(values_.size());
    for (size_t i = 0; i < values_.size(); ++i) {
        if (is_null(i)) {
            new_vals.push_back(Value::make_nil());
            continue;
        }
        switch (new_kind) {
            case DataKind::FLOAT64: new_vals.push_back(Value::make_float(as_float(i))); break;
            case DataKind::INT64: new_vals.push_back(Value::make_int(as_int(i))); break;
            case DataKind::STRING: new_vals.push_back(Value::make_string(as_string(i))); break;
            case DataKind::BOOL: new_vals.push_back(Value::make_bool(as_bool(i))); break;
            case DataKind::NULL_VAL: new_vals.push_back(Value::make_nil()); break;
        }
    }
    return Column(name_, new_kind, std::move(new_vals));
}

Column Column::normalize() const {
    double min_v = min();
    double max_v = max();
    double range = max_v - min_v;
    if (std::abs(range) < 1e-12) range = 1.0;

    std::vector<Value> norm_vals;
    norm_vals.reserve(values_.size());
    for (size_t i = 0; i < values_.size(); ++i) {
        if (is_null(i)) {
            norm_vals.push_back(Value::make_nil());
        } else {
            norm_vals.push_back(Value::make_float((as_float(i) - min_v) / range));
        }
    }
    return Column(name_, DataKind::FLOAT64, std::move(norm_vals));
}

Column Column::standardize() const {
    double avg = mean();
    double sd = std_dev();
    if (std::abs(sd) < 1e-12) sd = 1.0;

    std::vector<Value> std_vals;
    std_vals.reserve(values_.size());
    for (size_t i = 0; i < values_.size(); ++i) {
        if (is_null(i)) {
            std_vals.push_back(Value::make_nil());
        } else {
            std_vals.push_back(Value::make_float((as_float(i) - avg) / sd));
        }
    }
    return Column(name_, DataKind::FLOAT64, std::move(std_vals));
}

Value Column::to_value() const {
    auto self = std::make_shared<Column>(*this);
    std::map<std::string, Value> obj;
    obj["name"] = Value::make_string(name_);
    obj["type"] = Value::make_string(data_kind_to_string(kind_));
    obj["values"] = Value::make_array(values_);
    obj["size"] = Value::make_native_fn("size", 0, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_int(static_cast<int64_t>(self->values_.size()));
    });
    obj["mean"] = Value::make_native_fn("mean", 0, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->mean());
    });
    obj["sum"] = Value::make_native_fn("sum", 0, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->sum());
    });
    obj["min"] = Value::make_native_fn("min", 0, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->min());
    });
    obj["max"] = Value::make_native_fn("max", 0, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->max());
    });
    return Value::make_object(std::move(obj));
}

// ============================================================================
// DataArray Implementation
// ============================================================================

DataArray::DataArray() : shape_{0} {}

DataArray::DataArray(std::vector<double> data, std::vector<size_t> shape)
    : data_(std::move(data)), shape_(std::move(shape)) {
    if (shape_.empty()) {
        shape_ = {data_.size()};
    }
}

DataArray DataArray::from_values(const std::vector<Value>& values) {
    std::vector<double> d;
    d.reserve(values.size());
    for (const auto& v : values) {
        if (v.is_float()) d.push_back(v.as_float());
        else if (v.is_int()) d.push_back(static_cast<double>(v.as_int()));
        else d.push_back(0.0);
    }
    return DataArray(std::move(d));
}

DataArray DataArray::zeros(const std::vector<size_t>& shape) {
    size_t total = 1;
    for (size_t dim : shape) total *= dim;
    return DataArray(std::vector<double>(total, 0.0), shape);
}

DataArray DataArray::ones(const std::vector<size_t>& shape) {
    size_t total = 1;
    for (size_t dim : shape) total *= dim;
    return DataArray(std::vector<double>(total, 1.0), shape);
}

DataArray DataArray::arange(double start, double stop, double step) {
    std::vector<double> d;
    if (step > 0) {
        for (double v = start; v < stop; v += step) d.push_back(v);
    } else if (step < 0) {
        for (double v = start; v > stop; v += step) d.push_back(v);
    }
    return DataArray(std::move(d));
}

DataArray DataArray::linspace(double start, double stop, size_t num) {
    if (num == 0) return DataArray();
    if (num == 1) return DataArray({start});
    std::vector<double> d(num);
    double step = (stop - start) / static_cast<double>(num - 1);
    for (size_t i = 0; i < num; ++i) {
        d[i] = start + i * step;
    }
    return DataArray(std::move(d));
}

double DataArray::get(size_t index) const {
    if (index >= data_.size()) return 0.0;
    return data_[index];
}

void DataArray::set(size_t index, double value) {
    if (index < data_.size()) {
        data_[index] = value;
    }
}

double DataArray::mean() const {
    if (data_.empty()) return 0.0;
    return sum() / static_cast<double>(data_.size());
}

double DataArray::sum() const {
    return std::accumulate(data_.begin(), data_.end(), 0.0);
}

double DataArray::min() const {
    if (data_.empty()) return 0.0;
    return *std::min_element(data_.begin(), data_.end());
}

double DataArray::max() const {
    if (data_.empty()) return 0.0;
    return *std::max_element(data_.begin(), data_.end());
}

double DataArray::variance() const {
    if (data_.size() <= 1) return 0.0;
    double avg = mean();
    double sum_sq = 0.0;
    for (double v : data_) {
        double d = v - avg;
        sum_sq += d * d;
    }
    return sum_sq / static_cast<double>(data_.size() - 1);
}

double DataArray::std_dev() const {
    return std::sqrt(variance());
}

double DataArray::median() const {
    if (data_.empty()) return 0.0;
    std::vector<double> copy = data_;
    std::sort(copy.begin(), copy.end());
    size_t mid = copy.size() / 2;
    if (copy.size() % 2 == 0) {
        return (copy[mid - 1] + copy[mid]) / 2.0;
    }
    return copy[mid];
}

size_t DataArray::argmax() const {
    if (data_.empty()) return 0;
    return std::distance(data_.begin(), std::max_element(data_.begin(), data_.end()));
}

size_t DataArray::argmin() const {
    if (data_.empty()) return 0;
    return std::distance(data_.begin(), std::min_element(data_.begin(), data_.end()));
}

DataArray DataArray::add(const DataArray& other) const {
    size_t sz = std::min(data_.size(), other.data_.size());
    std::vector<double> res(sz);
    for (size_t i = 0; i < sz; ++i) res[i] = data_[i] + other.data_[i];
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::sub(const DataArray& other) const {
    size_t sz = std::min(data_.size(), other.data_.size());
    std::vector<double> res(sz);
    for (size_t i = 0; i < sz; ++i) res[i] = data_[i] - other.data_[i];
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::mul(const DataArray& other) const {
    size_t sz = std::min(data_.size(), other.data_.size());
    std::vector<double> res(sz);
    for (size_t i = 0; i < sz; ++i) res[i] = data_[i] * other.data_[i];
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::div(const DataArray& other) const {
    size_t sz = std::min(data_.size(), other.data_.size());
    std::vector<double> res(sz);
    for (size_t i = 0; i < sz; ++i) {
        res[i] = (other.data_[i] != 0.0) ? (data_[i] / other.data_[i]) : 0.0;
    }
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::add_scalar(double scalar) const {
    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) res[i] = data_[i] + scalar;
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::mul_scalar(double scalar) const {
    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) res[i] = data_[i] * scalar;
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::pow(double exponent) const {
    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) res[i] = std::pow(data_[i], exponent);
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::abs() const {
    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) res[i] = std::abs(data_[i]);
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::exp() const {
    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) res[i] = std::exp(data_[i]);
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::log() const {
    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) res[i] = std::log(std::max(1e-12, data_[i]));
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::sqrt() const {
    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) res[i] = std::sqrt(std::max(0.0, data_[i]));
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::cumsum() const {
    std::vector<double> res(data_.size());
    double acc = 0.0;
    for (size_t i = 0; i < data_.size(); ++i) {
        acc += data_[i];
        res[i] = acc;
    }
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::normalize(double min_val, double max_val) const {
    double cur_min = min();
    double cur_max = max();
    double range = cur_max - cur_min;
    if (std::abs(range) < 1e-12) range = 1.0;
    double target_range = max_val - min_val;

    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) {
        res[i] = min_val + ((data_[i] - cur_min) / range) * target_range;
    }
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::standardize() const {
    double avg = mean();
    double sd = std_dev();
    if (std::abs(sd) < 1e-12) sd = 1.0;

    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) {
        res[i] = (data_[i] - avg) / sd;
    }
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::clip(double low, double high) const {
    std::vector<double> res(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) {
        res[i] = std::clamp(data_[i], low, high);
    }
    return DataArray(std::move(res), shape_);
}

DataArray DataArray::reshape(std::vector<size_t> new_shape) const {
    size_t total = 1;
    for (size_t d : new_shape) total *= d;
    if (total != data_.size()) {
        return *this;
    }
    return DataArray(data_, std::move(new_shape));
}

Tensor DataArray::to_tensor() const {
    std::vector<int64_t> shp;
    for (size_t s : shape_) shp.push_back(static_cast<int64_t>(s));
    return Tensor(shp, data_);
}

Value DataArray::to_value() const {
    auto self = std::make_shared<DataArray>(*this);
    std::map<std::string, Value> obj;
    std::vector<Value> shape_vals;
    for (size_t s : shape_) shape_vals.push_back(Value::make_int(static_cast<int64_t>(s)));
    obj["shape"] = Value::make_array(std::move(shape_vals));
    obj["size"] = Value::make_int(static_cast<int64_t>(data_.size()));

    std::vector<Value> d_vals;
    d_vals.reserve(data_.size());
    for (double d : data_) d_vals.push_back(Value::make_float(d));
    obj["data"] = Value::make_array(std::move(d_vals));

    obj["mean"] = Value::make_native_fn("mean", -1, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->mean());
    });
    obj["sum"] = Value::make_native_fn("sum", -1, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->sum());
    });
    obj["min"] = Value::make_native_fn("min", -1, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->min());
    });
    obj["max"] = Value::make_native_fn("max", -1, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->max());
    });
    obj["std"] = Value::make_native_fn("std", -1, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->std_dev());
    });
    obj["var"] = Value::make_native_fn("var", -1, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->variance());
    });
    obj["median"] = Value::make_native_fn("median", -1, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_float(self->median());
    });
    obj["normalize"] = Value::make_native_fn("normalize", -1, [self](const std::vector<Value>& args, SourceSpan) {
        double lo = args.size() > 0 ? (args[0].is_float() ? args[0].as_float() : args[0].as_int()) : 0.0;
        double hi = args.size() > 1 ? (args[1].is_float() ? args[1].as_float() : args[1].as_int()) : 1.0;
        return self->normalize(lo, hi).to_value();
    });
    obj["standardize"] = Value::make_native_fn("standardize", -1, [self](const std::vector<Value>&, SourceSpan) {
        return self->standardize().to_value();
    });
    obj["to_tensor"] = Value::make_native_fn("to_tensor", -1, [self](const std::vector<Value>&, SourceSpan) {
        return self->to_tensor().to_value();
    });

    return Value::make_object(std::move(obj));
}

// ============================================================================
// DataFrame Implementation
// ============================================================================

DataFrame::DataFrame() {}

DataFrame::DataFrame(std::vector<std::string> column_names, std::vector<std::vector<Value>> rows)
    : columns_(std::move(column_names)), rows_(std::move(rows)) {
    build_schema();
}

DataFrame::DataFrame(Schema schema, std::vector<std::vector<Value>> rows)
    : schema_(std::move(schema)), rows_(std::move(rows)) {
    for (const auto& col : schema_.columns()) {
        columns_.push_back(col.name);
    }
}

void DataFrame::build_schema() {
    std::vector<ColumnSchema> cols;
    for (size_t c = 0; c < columns_.size(); ++c) {
        DataKind kind = DataKind::FLOAT64;
        bool has_val = false;
        for (size_t r = 0; r < rows_.size(); ++r) {
            if (c < rows_[r].size() && !rows_[r][c].is_nil()) {
                kind = infer_data_kind(rows_[r][c]);
                has_val = true;
                break;
            }
        }
        cols.emplace_back(columns_[c], has_val ? kind : DataKind::FLOAT64, true);
    }
    schema_ = Schema(std::move(cols));
}

int DataFrame::get_column_index(const std::string& col_name) const {
    return schema_.get_column_index(col_name);
}

static Value parse_typed_csv_cell(const std::string& cell) {
    std::string trimmed = cell;
    size_t first = trimmed.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return Value::make_nil();
    size_t last = trimmed.find_last_not_of(" \t\r\n");
    trimmed = trimmed.substr(first, last - first + 1);

    if (trimmed == "null" || trimmed == "nil" || trimmed == "NaN" || trimmed == "NA" || trimmed.empty()) {
        return Value::make_nil();
    }
    if (trimmed == "true" || trimmed == "True" || trimmed == "TRUE") return Value::make_bool(true);
    if (trimmed == "false" || trimmed == "False" || trimmed == "FALSE") return Value::make_bool(false);

    char* end_int = nullptr;
    long long int_val = std::strtoll(trimmed.c_str(), &end_int, 10);
    if (end_int && *end_int == '\0') {
        return Value::make_int(static_cast<int64_t>(int_val));
    }

    char* end_flt = nullptr;
    double flt_val = std::strtod(trimmed.c_str(), &end_flt);
    if (end_flt && *end_flt == '\0') {
        return Value::make_float(flt_val);
    }

    return Value::make_string(trimmed);
}

DataFrame DataFrame::from_csv(const std::string& csv_content, bool has_header, char delimiter) {
    std::vector<std::string> headers;
    std::vector<std::vector<Value>> rows;

    std::stringstream ss(csv_content);
    std::string line;
    bool is_first = true;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> tokens;
        std::string cur = "";
        bool in_quotes = false;

        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                    cur += '"';
                    i++;
                } else {
                    in_quotes = !in_quotes;
                }
            } else if (c == delimiter && !in_quotes) {
                tokens.push_back(cur);
                cur = "";
            } else {
                cur += c;
            }
        }
        tokens.push_back(cur);

        if (is_first && has_header) {
            for (const auto& t : tokens) {
                std::string h = t;
                size_t f = h.find_first_not_of(" \t\r\n");
                size_t l = h.find_last_not_of(" \t\r\n");
                if (f != std::string::npos) h = h.substr(f, l - f + 1);
                headers.push_back(h);
            }
            is_first = false;
            continue;
        }

        if (is_first && !has_header) {
            for (size_t k = 0; k < tokens.size(); ++k) {
                headers.push_back("col_" + std::to_string(k));
            }
            is_first = false;
        }

        std::vector<Value> row_vals;
        for (const auto& t : tokens) {
            row_vals.push_back(parse_typed_csv_cell(t));
        }
        rows.push_back(std::move(row_vals));
    }

    return DataFrame(std::move(headers), std::move(rows));
}

DataFrame DataFrame::from_json(const std::string& json_content) {
    std::vector<std::string> cols;
    std::vector<std::vector<Value>> rows;

    std::stringstream ss(json_content);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '[' || line[0] == ']') continue;
        if (line.back() == ',') line.pop_back();

        // JSON record key/value extraction
        std::map<std::string, Value> row_map;
        size_t pos = 0;
        while ((pos = line.find('"', pos)) != std::string::npos) {
            size_t end_k = line.find('"', pos + 1);
            if (end_k == std::string::npos) break;
            std::string key = line.substr(pos + 1, end_k - pos - 1);
            size_t colon = line.find(':', end_k);
            if (colon == std::string::npos) break;

            size_t comma = line.find_first_of(",}", colon);
            if (comma == std::string::npos) comma = line.size();

            std::string raw_val = line.substr(colon + 1, comma - colon - 1);
            size_t f = raw_val.find_first_not_of(" \t\"");
            size_t l = raw_val.find_last_not_of(" \t\"");
            if (f != std::string::npos) raw_val = raw_val.substr(f, l - f + 1);

            row_map[key] = parse_typed_csv_cell(raw_val);
            pos = comma + 1;
        }

        if (!row_map.empty()) {
            if (cols.empty()) {
                for (const auto& [k, v] : row_map) cols.push_back(k);
            }
            std::vector<Value> row;
            for (const auto& c : cols) {
                row.push_back(row_map.count(c) ? row_map[c] : Value::make_nil());
            }
            rows.push_back(std::move(row));
        }
    }

    return DataFrame(std::move(cols), std::move(rows));
}

DataFrame DataFrame::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return DataFrame();
    }
    std::stringstream buf;
    buf << file.rdbuf();
    std::string content = buf.str();

    if (path.rfind(".json") != std::string::npos || path.rfind(".jsonl") != std::string::npos) {
        return from_json(content);
    }
    return from_csv(content);
}

std::string DataFrame::to_csv(char delimiter) const {
    std::ostringstream ss;
    for (size_t i = 0; i < columns_.size(); ++i) {
        ss << columns_[i];
        if (i + 1 < columns_.size()) ss << delimiter;
    }
    ss << "\n";

    for (const auto& row : rows_) {
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i < row.size()) {
                if (row[i].is_string()) ss << "\"" << row[i].as_string() << "\"";
                else if (row[i].is_nil()) ss << "";
                else ss << row[i].to_string();
            }
            if (i + 1 < columns_.size()) ss << delimiter;
        }
        ss << "\n";
    }
    return ss.str();
}

std::string DataFrame::to_json(int indent) const {
    std::ostringstream ss;
    ss << "[\n";
    for (size_t r = 0; r < rows_.size(); ++r) {
        if (indent > 0) ss << std::string(indent, ' ');
        ss << "{";
        for (size_t c = 0; c < columns_.size(); ++c) {
            ss << "\"" << columns_[c] << "\": ";
            if (c < rows_[r].size()) {
                if (rows_[r][c].is_string()) ss << "\"" << rows_[r][c].as_string() << "\"";
                else if (rows_[r][c].is_nil()) ss << "null";
                else ss << rows_[r][c].to_string();
            } else {
                ss << "null";
            }
            if (c + 1 < columns_.size()) ss << ", ";
        }
        ss << "}";
        if (r + 1 < rows_.size()) ss << ",";
        ss << "\n";
    }
    ss << "]";
    return ss.str();
}

std::optional<Column> DataFrame::get_column(const std::string& name) const {
    int idx = get_column_index(name);
    if (idx < 0) return std::nullopt;

    std::vector<Value> vals;
    vals.reserve(rows_.size());
    for (const auto& row : rows_) {
        vals.push_back(static_cast<size_t>(idx) < row.size() ? row[static_cast<size_t>(idx)] : Value::make_nil());
    }

    const auto* col_sch = schema_.get_column(name);
    DataKind k = col_sch ? col_sch->kind : DataKind::FLOAT64;
    return Column(name, k, std::move(vals));
}

std::vector<Value> DataFrame::get_row(size_t index) const {
    if (index >= rows_.size()) return {};
    return rows_[index];
}

Value DataFrame::get(size_t row, const std::string& col) const {
    int idx = get_column_index(col);
    if (idx < 0 || row >= rows_.size()) return Value::make_nil();
    if (static_cast<size_t>(idx) < rows_[row].size()) return rows_[row][static_cast<size_t>(idx)];
    return Value::make_nil();
}

DataFrame DataFrame::select(const std::vector<std::string>& column_names) const {
    std::vector<std::string> valid_cols;
    std::vector<int> col_indices;

    for (const auto& c : column_names) {
        int idx = get_column_index(c);
        if (idx >= 0) {
            valid_cols.push_back(c);
            col_indices.push_back(idx);
        }
    }

    std::vector<std::vector<Value>> new_rows;
    new_rows.reserve(rows_.size());

    for (const auto& r : rows_) {
        std::vector<Value> new_r;
        new_r.reserve(col_indices.size());
        for (int idx : col_indices) {
            new_r.push_back(static_cast<size_t>(idx) < r.size() ? r[static_cast<size_t>(idx)] : Value::make_nil());
        }
        new_rows.push_back(std::move(new_r));
    }

    return DataFrame(std::move(valid_cols), std::move(new_rows));
}

DataFrame DataFrame::drop(const std::vector<std::string>& column_names) const {
    std::vector<std::string> keep_cols;
    for (const auto& c : columns_) {
        if (std::find(column_names.begin(), column_names.end(), c) == column_names.end()) {
            keep_cols.push_back(c);
        }
    }
    return select(keep_cols);
}

DataFrame DataFrame::head(size_t n) const {
    size_t limit = std::min(n, rows_.size());
    std::vector<std::vector<Value>> new_rows(rows_.begin(), rows_.begin() + limit);
    return DataFrame(columns_, std::move(new_rows));
}

DataFrame DataFrame::tail(size_t n) const {
    size_t start = (rows_.size() > n) ? (rows_.size() - n) : 0;
    std::vector<std::vector<Value>> new_rows(rows_.begin() + start, rows_.end());
    return DataFrame(columns_, std::move(new_rows));
}

DataFrame DataFrame::slice(size_t start, size_t end) const {
    size_t s = std::min(start, rows_.size());
    size_t e = std::min(end, rows_.size());
    if (s >= e) return DataFrame(columns_, {});
    std::vector<std::vector<Value>> new_rows(rows_.begin() + s, rows_.begin() + e);
    return DataFrame(columns_, std::move(new_rows));
}

DataFrame DataFrame::filter(const std::function<bool(const std::map<std::string, Value>&)>& predicate) const {
    std::vector<std::vector<Value>> matched;
    for (const auto& r : rows_) {
        std::map<std::string, Value> row_map;
        for (size_t c = 0; c < columns_.size(); ++c) {
            row_map[columns_[c]] = (c < r.size()) ? r[c] : Value::make_nil();
        }
        if (predicate(row_map)) {
            matched.push_back(r);
        }
    }
    return DataFrame(columns_, std::move(matched));
}

DataFrame DataFrame::sort(const std::string& column_name, bool ascending) const {
    int idx = get_column_index(column_name);
    if (idx < 0) return *this;

    std::vector<std::vector<Value>> sorted_rows = rows_;
    std::stable_sort(sorted_rows.begin(), sorted_rows.end(), [&](const std::vector<Value>& a, const std::vector<Value>& b) {
        Value val_a = static_cast<size_t>(idx) < a.size() ? a[static_cast<size_t>(idx)] : Value::make_nil();
        Value val_b = static_cast<size_t>(idx) < b.size() ? b[static_cast<size_t>(idx)] : Value::make_nil();
        if (val_a.is_nil()) return false;
        if (val_b.is_nil()) return true;

        if (val_a.is_float() || val_a.is_int()) {
            double fa = val_a.is_float() ? val_a.as_float() : val_a.as_int();
            double fb = val_b.is_float() ? val_b.as_float() : val_b.as_int();
            return ascending ? (fa < fb) : (fa > fb);
        }
        return ascending ? (val_a.to_string() < val_b.to_string()) : (val_a.to_string() > val_b.to_string());
    });

    return DataFrame(columns_, std::move(sorted_rows));
}

DataFrame DataFrame::rename_column(const std::string& old_name, const std::string& new_name) const {
    std::vector<std::string> new_cols = columns_;
    for (auto& c : new_cols) {
        if (c == old_name) c = new_name;
    }
    return DataFrame(std::move(new_cols), rows_);
}

DataFrame DataFrame::clean(bool drop_nulls, const std::string& /*fill_strategy*/) const {
    if (drop_nulls) {
        return drop_missing();
    }
    return *this;
}

DataFrame DataFrame::drop_missing(const std::vector<std::string>& target_columns) const {
    std::vector<int> check_indices;
    if (target_columns.empty()) {
        for (size_t i = 0; i < columns_.size(); ++i) check_indices.push_back(static_cast<int>(i));
    } else {
        for (const auto& col : target_columns) {
            int idx = get_column_index(col);
            if (idx >= 0) check_indices.push_back(idx);
        }
    }

    std::vector<std::vector<Value>> clean_rows;
    for (const auto& r : rows_) {
        bool has_null = false;
        for (int idx : check_indices) {
            if (static_cast<size_t>(idx) >= r.size() || r[static_cast<size_t>(idx)].is_nil()) {
                has_null = true;
                break;
            }
        }
        if (!has_null) {
            clean_rows.push_back(r);
        }
    }
    return DataFrame(columns_, std::move(clean_rows));
}

DataFrame DataFrame::fill_missing(const std::string& column_name, const Value& value) const {
    int idx = get_column_index(column_name);
    if (idx < 0) return *this;

    std::vector<std::vector<Value>> updated_rows = rows_;
    for (auto& r : updated_rows) {
        if (static_cast<size_t>(idx) >= r.size() || r[static_cast<size_t>(idx)].is_nil()) {
            if (static_cast<size_t>(idx) >= r.size()) r.resize(static_cast<size_t>(idx) + 1, Value::make_nil());
            r[static_cast<size_t>(idx)] = value;
        }
    }
    return DataFrame(columns_, std::move(updated_rows));
}

DataFrame DataFrame::normalize(const std::vector<std::string>& target_columns) const {
    std::vector<std::string> cols = target_columns.empty() ? columns_ : target_columns;
    std::vector<std::vector<Value>> norm_rows = rows_;

    for (const auto& col_name : cols) {
        auto col_opt = get_column(col_name);
        if (!col_opt) continue;
        int idx = get_column_index(col_name);
        if (idx < 0) continue;

        Column norm_col = col_opt->normalize();
        for (size_t r = 0; r < norm_rows.size(); ++r) {
            if (static_cast<size_t>(idx) < norm_rows[r].size()) {
                norm_rows[r][static_cast<size_t>(idx)] = norm_col.get(r);
            }
        }
    }
    return DataFrame(columns_, std::move(norm_rows));
}

DataFrame DataFrame::standardize(const std::vector<std::string>& target_columns) const {
    std::vector<std::string> cols = target_columns.empty() ? columns_ : target_columns;
    std::vector<std::vector<Value>> std_rows = rows_;

    for (const auto& col_name : cols) {
        auto col_opt = get_column(col_name);
        if (!col_opt) continue;
        int idx = get_column_index(col_name);
        if (idx < 0) continue;

        Column std_col = col_opt->standardize();
        for (size_t r = 0; r < std_rows.size(); ++r) {
            if (static_cast<size_t>(idx) < std_rows[r].size()) {
                std_rows[r][static_cast<size_t>(idx)] = std_col.get(r);
            }
        }
    }
    return DataFrame(columns_, std::move(std_rows));
}

DataFrame DataFrame::shuffle(uint32_t seed) const {
    std::vector<std::vector<Value>> shuffled = rows_;
    std::mt19937 g(seed);
    std::shuffle(shuffled.begin(), shuffled.end(), g);
    return DataFrame(columns_, std::move(shuffled));
}

std::pair<DataFrame, DataFrame> DataFrame::split(double train_ratio, uint32_t seed) const {
    DataFrame shuf = shuffle(seed);
    size_t train_size = static_cast<size_t>(std::round(shuf.num_rows() * train_ratio));
    train_size = std::clamp(train_size, size_t(0), shuf.num_rows());

    std::vector<std::vector<Value>> train_rows(shuf.rows().begin(), shuf.rows().begin() + train_size);
    std::vector<std::vector<Value>> test_rows(shuf.rows().begin() + train_size, shuf.rows().end());

    return {DataFrame(columns_, std::move(train_rows)), DataFrame(columns_, std::move(test_rows))};
}

DataFrame DataFrame::sample(size_t n, uint32_t seed) const {
    if (rows_.empty()) return DataFrame(columns_, {});
    DataFrame shuf = shuffle(seed);
    return shuf.head(n);
}

DataFrame DataFrame::sample_fraction(double frac, uint32_t seed) const {
    size_t count = static_cast<size_t>(std::round(rows_.size() * std::clamp(frac, 0.0, 1.0)));
    return sample(count, seed);
}

std::vector<DataFrame> DataFrame::batches(size_t batch_size, bool do_shuffle) const {
    std::vector<DataFrame> result;
    if (batch_size == 0 || rows_.empty()) return result;

    DataFrame src = do_shuffle ? shuffle() : *this;
    for (size_t i = 0; i < src.num_rows(); i += batch_size) {
        result.push_back(src.slice(i, i + batch_size));
    }
    return result;
}

std::map<std::string, std::map<std::string, double>> DataFrame::describe() const {
    std::map<std::string, std::map<std::string, double>> stats;
    for (const auto& col_name : columns_) {
        auto col_opt = get_column(col_name);
        if (!col_opt) continue;
        const auto& col = *col_opt;
        stats[col_name]["mean"] = col.mean();
        stats[col_name]["sum"] = col.sum();
        stats[col_name]["min"] = col.min();
        stats[col_name]["max"] = col.max();
        stats[col_name]["std"] = col.std_dev();
        stats[col_name]["nulls"] = static_cast<double>(col.count_nulls());
        stats[col_name]["count"] = static_cast<double>(num_rows() - col.count_nulls());
    }
    return stats;
}

DataArray DataFrame::column_to_array(const std::string& column_name) const {
    auto col_opt = get_column(column_name);
    if (!col_opt) return DataArray();
    std::vector<double> d;
    d.reserve(rows_.size());
    for (size_t i = 0; i < rows_.size(); ++i) {
        d.push_back(col_opt->as_float(i));
    }
    return DataArray(std::move(d));
}

Tensor DataFrame::to_tensor(const std::vector<std::string>& target_columns) const {
    std::vector<std::string> cols = target_columns.empty() ? columns_ : target_columns;
    std::vector<double> data;
    data.reserve(rows_.size() * cols.size());

    for (const auto& r : rows_) {
        for (const auto& col_name : cols) {
            int idx = get_column_index(col_name);
            if (idx >= 0 && static_cast<size_t>(idx) < r.size()) {
                const auto& v = r[static_cast<size_t>(idx)];
                if (v.is_float()) data.push_back(v.as_float());
                else if (v.is_int()) data.push_back(static_cast<double>(v.as_int()));
                else data.push_back(0.0);
            } else {
                data.push_back(0.0);
            }
        }
    }
    std::vector<int64_t> shp = {
        static_cast<int64_t>(rows_.size()),
        static_cast<int64_t>(cols.size())
    };
    return Tensor(shp, data);
}

Value DataFrame::to_value() const {
    auto self = std::make_shared<DataFrame>(*this);
    std::map<std::string, Value> obj;
    
    // Properties
    std::vector<Value> cols_arr;
    for (const auto& c : columns_) cols_arr.push_back(Value::make_string(c));
    obj["columns"] = Value::make_array(cols_arr);

    std::vector<Value> shape_vals = {
        Value::make_int(static_cast<int64_t>(rows_.size())),
        Value::make_int(static_cast<int64_t>(columns_.size()))
    };
    obj["shape"] = Value::make_array(std::move(shape_vals));
    obj["num_rows"] = Value::make_int(static_cast<int64_t>(rows_.size()));
    obj["num_cols"] = Value::make_int(static_cast<int64_t>(columns_.size()));
    obj["schema"] = schema_.to_value();

    std::vector<Value> rows_arr;
    rows_arr.reserve(rows_.size());
    for (const auto& r : rows_) rows_arr.push_back(Value::make_array(r));
    obj["rows"] = Value::make_array(std::move(rows_arr));

    // Methods
    obj["select"] = Value::make_native_fn("select", 1, [self](const std::vector<Value>& args, SourceSpan) {
        if (!args[0].is_array()) return self->to_value();
        std::vector<std::string> cols;
        for (const auto& item : *args[0].as_array()) {
            if (item.is_string()) cols.push_back(item.as_string());
        }
        return self->select(cols).to_value();
    });

    obj["drop"] = Value::make_native_fn("drop", 1, [self](const std::vector<Value>& args, SourceSpan) {
        if (!args[0].is_array()) return self->to_value();
        std::vector<std::string> cols;
        for (const auto& item : *args[0].as_array()) {
            if (item.is_string()) cols.push_back(item.as_string());
        }
        return self->drop(cols).to_value();
    });

    obj["head"] = Value::make_native_fn("head", -1, [self](const std::vector<Value>& args, SourceSpan) {
        size_t n = (args.size() > 0 && args[0].is_int()) ? static_cast<size_t>(args[0].as_int()) : 5;
        return self->head(n).to_value();
    });

    obj["tail"] = Value::make_native_fn("tail", -1, [self](const std::vector<Value>& args, SourceSpan) {
        size_t n = (args.size() > 0 && args[0].is_int()) ? static_cast<size_t>(args[0].as_int()) : 5;
        return self->tail(n).to_value();
    });

    obj["sort"] = Value::make_native_fn("sort", -1, [self](const std::vector<Value>& args, SourceSpan) {
        if (args.empty()) return self->to_value();
        std::string col = args[0].as_string();
        bool asc = (args.size() > 1 && args[1].is_bool()) ? args[1].as_bool() : true;
        return self->sort(col, asc).to_value();
    });

    obj["clean"] = Value::make_native_fn("clean", -1, [self](const std::vector<Value>& args, SourceSpan) {
        bool drop_nulls = args.empty() ? true : args[0].as_bool();
        return self->clean(drop_nulls).to_value();
    });

    obj["drop_missing"] = Value::make_native_fn("drop_missing", -1, [self](const std::vector<Value>&, SourceSpan) {
        return self->drop_missing().to_value();
    });

    obj["shuffle"] = Value::make_native_fn("shuffle", -1, [self](const std::vector<Value>& args, SourceSpan) {
        uint32_t seed = (args.size() > 0 && args[0].is_int()) ? static_cast<uint32_t>(args[0].as_int()) : 42;
        return self->shuffle(seed).to_value();
    });

    obj["split"] = Value::make_native_fn("split", -1, [self](const std::vector<Value>& args, SourceSpan) {
        double ratio = (!args.empty()) ? (args[0].is_float() ? args[0].as_float() : (args[0].is_int() ? static_cast<double>(args[0].as_int()) : 0.8)) : 0.8;
        uint32_t seed = (args.size() > 1 && args[1].is_int()) ? static_cast<uint32_t>(args[1].as_int()) : 42;
        auto [train_df, test_df] = self->split(ratio, seed);
        std::vector<Value> split_res = {train_df.to_value(), test_df.to_value()};
        return Value::make_array(std::move(split_res));
    });

    obj["normalize"] = Value::make_native_fn("normalize", -1, [self](const std::vector<Value>&, SourceSpan) {
        return self->normalize().to_value();
    });

    obj["standardize"] = Value::make_native_fn("standardize", -1, [self](const std::vector<Value>&, SourceSpan) {
        return self->standardize().to_value();
    });

    obj["to_tensor"] = Value::make_native_fn("to_tensor", -1, [self](const std::vector<Value>& args, SourceSpan) {
        if (!args.empty() && args[0].is_array()) {
            std::vector<std::string> cols;
            for (const auto& v : *args[0].as_array()) {
                cols.push_back(v.as_string());
            }
            return self->to_tensor(cols).to_value();
        }
        return self->to_tensor().to_value();
    });

    obj["to_array"] = Value::make_native_fn("to_array", 1, [self](const std::vector<Value>& args, SourceSpan) {
        std::string col = args[0].as_string();
        return self->column_to_array(col).to_value();
    });

    obj["describe"] = Value::make_native_fn("describe", -1, [self](const std::vector<Value>&, SourceSpan) {
        auto desc = self->describe();
        std::map<std::string, Value> desc_obj;
        for (const auto& [col, stats] : desc) {
            std::map<std::string, Value> stat_obj;
            for (const auto& [metric, val] : stats) {
                stat_obj[metric] = Value::make_float(val);
            }
            desc_obj[col] = Value::make_object(std::move(stat_obj));
        }
        return Value::make_object(std::move(desc_obj));
    });

    obj["to_csv"] = Value::make_native_fn("to_csv", -1, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_string(self->to_csv());
    });

    obj["to_json"] = Value::make_native_fn("to_json", -1, [self](const std::vector<Value>&, SourceSpan) {
        return Value::make_string(self->to_json(2));
    });

    return Value::make_object(std::move(obj));
}

// ============================================================================
// Data Module Creation
// ============================================================================

Value create_data_module() {
    std::map<std::string, Value> obj;

    obj["load"] = Value::make_native_fn("load", -1, [](const std::vector<Value>& args, SourceSpan) {
        if (args.empty()) return DataFrame().to_value();
        std::string path = args[0].as_string();
        return DataFrame::load(path).to_value();
    });

    obj["read_csv"] = Value::make_native_fn("read_csv", -1, [](const std::vector<Value>& args, SourceSpan) {
        if (args.empty()) return DataFrame().to_value();
        std::string content = args[0].as_string();
        std::ifstream f(content);
        if (f.good()) {
            return DataFrame::load(content).to_value();
        }
        return DataFrame::from_csv(content).to_value();
    });

    obj["read_json"] = Value::make_native_fn("read_json", -1, [](const std::vector<Value>& args, SourceSpan) {
        if (args.empty()) return DataFrame().to_value();
        std::string content = args[0].as_string();
        std::ifstream f(content);
        if (f.good()) {
            return DataFrame::load(content).to_value();
        }
        return DataFrame::from_json(content).to_value();
    });

    obj["array"] = Value::make_native_fn("array", -1, [](const std::vector<Value>& args, SourceSpan) {
        if (!args.empty() && args[0].is_array()) {
            return DataArray::from_values(*args[0].as_array()).to_value();
        }
        return DataArray().to_value();
    });

    obj["zeros"] = Value::make_native_fn("zeros", -1, [](const std::vector<Value>& args, SourceSpan) {
        std::vector<size_t> shape;
        if (!args.empty() && args[0].is_array()) {
            for (const auto& v : *args[0].as_array()) {
                shape.push_back(static_cast<size_t>(v.is_int() ? v.as_int() : v.as_float()));
            }
        } else if (!args.empty() && args[0].is_int()) {
            shape.push_back(static_cast<size_t>(args[0].as_int()));
        }
        return DataArray::zeros(shape).to_value();
    });

    obj["ones"] = Value::make_native_fn("ones", -1, [](const std::vector<Value>& args, SourceSpan) {
        std::vector<size_t> shape;
        if (!args.empty() && args[0].is_array()) {
            for (const auto& v : *args[0].as_array()) {
                shape.push_back(static_cast<size_t>(v.is_int() ? v.as_int() : v.as_float()));
            }
        } else if (!args.empty() && args[0].is_int()) {
            shape.push_back(static_cast<size_t>(args[0].as_int()));
        }
        return DataArray::ones(shape).to_value();
    });

    obj["arange"] = Value::make_native_fn("arange", -1, [](const std::vector<Value>& args, SourceSpan) {
        if (args.size() < 2) return DataArray().to_value();
        double start = args[0].is_float() ? args[0].as_float() : args[0].as_int();
        double stop = args[1].is_float() ? args[1].as_float() : args[1].as_int();
        double step = (args.size() > 2) ? (args[2].is_float() ? args[2].as_float() : args[2].as_int()) : 1.0;
        return DataArray::arange(start, stop, step).to_value();
    });

    obj["linspace"] = Value::make_native_fn("linspace", -1, [](const std::vector<Value>& args, SourceSpan) {
        if (args.size() < 3) return DataArray().to_value();
        double start = args[0].is_float() ? args[0].as_float() : args[0].as_int();
        double stop = args[1].is_float() ? args[1].as_float() : args[1].as_int();
        size_t num = static_cast<size_t>(args[2].is_int() ? args[2].as_int() : args[2].as_float());
        return DataArray::linspace(start, stop, num).to_value();
    });

    return Value::make_object(std::move(obj));
}

} // namespace nextviper
