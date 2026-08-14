#include "nextviper/dataset.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <cmath>
#include <iostream>

namespace nextviper {

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"");
    return str.substr(first, (last - first + 1));
}

static Value parse_csv_value(const std::string& raw) {
    std::string s = trim(raw);
    if (s.empty() || s == "null" || s == "nil" || s == "NA" || s == "NaN" || s == "None") {
        return Value::make_nil();
    }
    if (s == "true" || s == "True") return Value::make_bool(true);
    if (s == "false" || s == "False") return Value::make_bool(false);

    // Try parsing integer
    char* end = nullptr;
    errno = 0;
    long long ival = std::strtoll(s.c_str(), &end, 10);
    if (end != s.c_str() && *end == '\0' && errno == 0) {
        return Value::make_int(ival);
    }

    // Try parsing float
    errno = 0;
    double fval = std::strtod(s.c_str(), &end);
    if (end != s.c_str() && *end == '\0' && errno == 0) {
        return Value::make_float(fval);
    }

    return Value::make_string(s);
}

Dataset::Dataset() {}

Dataset::Dataset(std::vector<std::string> columns, std::vector<std::vector<Value>> rows)
    : columns_(std::move(columns)), rows_(std::move(rows)) {}

Dataset Dataset::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open dataset file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return from_csv(buffer.str());
}

Dataset Dataset::from_csv(const std::string& csv_content) {
    std::istringstream stream(csv_content);
    std::string line;
    std::vector<std::string> headers;
    std::vector<std::vector<Value>> rows;

    // Read header row
    if (std::getline(stream, line)) {
        std::istringstream h_stream(line);
        std::string col;
        while (std::getline(h_stream, col, ',')) {
            headers.push_back(trim(col));
        }
    }

    // Read data rows
    while (std::getline(stream, line)) {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue; // skip empty line
        std::istringstream r_stream(line);
        std::string field;
        std::vector<Value> row;
        while (std::getline(r_stream, field, ',')) {
            row.push_back(parse_csv_value(field));
        }
        // Pad row if needed
        while (row.size() < headers.size()) {
            row.push_back(Value::make_nil());
        }
        rows.push_back(std::move(row));
    }

    return Dataset(std::move(headers), std::move(rows));
}

Dataset Dataset::from_rows(std::vector<std::string> columns, std::vector<std::vector<Value>> rows) {
    return Dataset(std::move(columns), std::move(rows));
}

int Dataset::get_column_index(const std::string& col_name) const {
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i] == col_name) return static_cast<int>(i);
    }
    return -1;
}

Dataset Dataset::clean(bool drop_nulls, const std::string& fill_strategy) const {
    if (drop_nulls) {
        std::vector<std::vector<Value>> clean_rows;
        for (const auto& row : rows_) {
            bool has_null = false;
            for (const auto& v : row) {
                if (v.is_nil()) {
                    has_null = true;
                    break;
                }
            }
            if (!has_null) {
                clean_rows.push_back(row);
            }
        }
        return Dataset(columns_, std::move(clean_rows));
    }

    // Fill missing values
    std::vector<double> means(columns_.size(), 0.0);
    std::vector<int64_t> counts(columns_.size(), 0);

    for (const auto& row : rows_) {
        for (size_t c = 0; c < columns_.size(); ++c) {
            if (row[c].is_number()) {
                means[c] += row[c].as_float();
                counts[c]++;
            }
        }
    }
    for (size_t c = 0; c < columns_.size(); ++c) {
        if (counts[c] > 0) means[c] /= static_cast<double>(counts[c]);
    }

    std::vector<std::vector<Value>> filled_rows = rows_;
    for (auto& row : filled_rows) {
        for (size_t c = 0; c < columns_.size(); ++c) {
            if (row[c].is_nil()) {
                if (fill_strategy == "mean") {
                    row[c] = Value::make_float(means[c]);
                } else {
                    row[c] = Value::make_float(0.0);
                }
            }
        }
    }
    return Dataset(columns_, std::move(filled_rows));
}

Dataset Dataset::shuffle(uint32_t seed) const {
    std::vector<std::vector<Value>> shuffled = rows_;
    std::mt19937 g(seed);
    std::shuffle(shuffled.begin(), shuffled.end(), g);
    return Dataset(columns_, std::move(shuffled));
}

std::pair<Dataset, Dataset> Dataset::split(double train_ratio, uint32_t seed) const {
    Dataset sh = shuffle(seed);
    size_t split_idx = static_cast<size_t>(static_cast<double>(sh.num_rows()) * std::clamp(train_ratio, 0.0, 1.0));

    std::vector<std::vector<Value>> train_rows(sh.rows_.begin(), sh.rows_.begin() + split_idx);
    std::vector<std::vector<Value>> test_rows(sh.rows_.begin() + split_idx, sh.rows_.end());

    return {Dataset(columns_, std::move(train_rows)), Dataset(columns_, std::move(test_rows))};
}

Dataset Dataset::select(const std::vector<std::string>& column_names) const {
    std::vector<int> col_indices;
    std::vector<std::string> valid_cols;
    for (const auto& name : column_names) {
        int idx = get_column_index(name);
        if (idx >= 0) {
            col_indices.push_back(idx);
            valid_cols.push_back(name);
        }
    }

    std::vector<std::vector<Value>> new_rows;
    for (const auto& row : rows_) {
        std::vector<Value> r;
        for (int idx : col_indices) {
            r.push_back(row[idx]);
        }
        new_rows.push_back(std::move(r));
    }
    return Dataset(std::move(valid_cols), std::move(new_rows));
}

Dataset Dataset::head(size_t n) const {
    size_t limit = std::min(n, rows_.size());
    std::vector<std::vector<Value>> subset(rows_.begin(), rows_.begin() + limit);
    return Dataset(columns_, std::move(subset));
}

std::map<std::string, std::map<std::string, double>> Dataset::describe() const {
    std::map<std::string, std::map<std::string, double>> stats;
    for (size_t c = 0; c < columns_.size(); ++c) {
        double sum = 0.0;
        double min_v = 1e18;
        double max_v = -1e18;
        int64_t count = 0;

        for (const auto& row : rows_) {
            if (row[c].is_number()) {
                double v = row[c].as_float();
                sum += v;
                min_v = std::min(min_v, v);
                max_v = std::max(max_v, v);
                count++;
            }
        }

        if (count > 0) {
            double mean = sum / count;
            double var_sum = 0.0;
            for (const auto& row : rows_) {
                if (row[c].is_number()) {
                    double diff = row[c].as_float() - mean;
                    var_sum += diff * diff;
                }
            }
            double std_dev = std::sqrt(var_sum / count);

            stats[columns_[c]] = {
                {"count", static_cast<double>(count)},
                {"mean", mean},
                {"std", std_dev},
                {"min", min_v},
                {"max", max_v}
            };
        }
    }
    return stats;
}

Tensor Dataset::to_tensor(const std::vector<std::string>& column_names) const {
    std::vector<int> col_indices;
    if (column_names.empty()) {
        for (size_t i = 0; i < columns_.size(); ++i) col_indices.push_back(static_cast<int>(i));
    } else {
        for (const auto& name : column_names) {
            int idx = get_column_index(name);
            if (idx >= 0) col_indices.push_back(idx);
        }
    }

    int64_t R = static_cast<int64_t>(rows_.size());
    int64_t C = static_cast<int64_t>(col_indices.size());
    std::vector<double> vals;
    vals.reserve(R * C);

    for (const auto& row : rows_) {
        for (int c_idx : col_indices) {
            vals.push_back(row[c_idx].is_number() ? row[c_idx].as_float() : 0.0);
        }
    }

    return Tensor({R, C}, vals, DType::FLOAT32);
}

Value Dataset::to_value() const {
    auto self_ds = std::make_shared<Dataset>(*this);
    std::map<std::string, Value> methods;

    methods["$type"] = Value::make_string("Dataset");
    methods["num_rows"] = Value::make_int(static_cast<int64_t>(num_rows()));
    methods["num_cols"] = Value::make_int(static_cast<int64_t>(num_cols()));

    std::vector<Value> col_vals;
    for (const auto& c : columns_) col_vals.push_back(Value::make_string(c));
    methods["columns"] = Value::make_array(std::move(col_vals));

    methods["clean"] = Value::make_native_fn("clean", -1, [self_ds](const std::vector<Value>& args, SourceSpan) -> Value {
        bool drop_nulls = args.empty() ? true : args[0].as_bool();
        std::string strategy = args.size() >= 2 ? args[1].as_string() : "mean";
        return self_ds->clean(drop_nulls, strategy).to_value();
    });

    methods["shuffle"] = Value::make_native_fn("shuffle", -1, [self_ds](const std::vector<Value>& args, SourceSpan) -> Value {
        uint32_t seed = args.empty() ? 42 : static_cast<uint32_t>(args[0].as_int());
        return self_ds->shuffle(seed).to_value();
    });

    methods["split"] = Value::make_native_fn("split", -1, [self_ds](const std::vector<Value>& args, SourceSpan) -> Value {
        double ratio = args.empty() ? 0.8 : args[0].as_float();
        uint32_t seed = args.size() >= 2 ? static_cast<uint32_t>(args[1].as_int()) : 42;
        auto [train_ds, val_ds] = self_ds->split(ratio, seed);
        return Value::make_array({train_ds.to_value(), val_ds.to_value()});
    });

    methods["select"] = Value::make_native_fn("select", 1, [self_ds](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<std::string> cols;
        if (args[0].is_array()) {
            for (const auto& v : *args[0].as_array()) cols.push_back(v.as_string());
        } else {
            cols.push_back(args[0].as_string());
        }
        return self_ds->select(cols).to_value();
    });

    methods["head"] = Value::make_native_fn("head", -1, [self_ds](const std::vector<Value>& args, SourceSpan) -> Value {
        size_t n = args.empty() ? 5 : static_cast<size_t>(args[0].as_int());
        return self_ds->head(n).to_value();
    });

    methods["to_tensor"] = Value::make_native_fn("to_tensor", -1, [self_ds](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<std::string> cols;
        if (!args.empty() && args[0].is_array()) {
            for (const auto& v : *args[0].as_array()) cols.push_back(v.as_string());
        }
        return self_ds->to_tensor(cols).to_value();
    });

    methods["to_string"] = Value::make_native_fn("to_string", 0, [self_ds](const std::vector<Value>&, SourceSpan) -> Value {
        std::ostringstream ss;
        ss << "Dataset(rows=" << self_ds->num_rows() << ", cols=" << self_ds->num_cols() << ", columns=[";
        for (size_t i = 0; i < self_ds->columns().size(); ++i) {
            if (i > 0) ss << ", ";
            ss << self_ds->columns()[i];
        }
        ss << "])";
        return Value::make_string(ss.str());
    });

    return Value::make_object(std::move(methods));
}

// --- DataLoader Implementation ---

DataLoader::DataLoader(Dataset dataset, size_t batch_size, bool shuffle, bool drop_last)
    : dataset_(std::move(dataset)), batch_size_(std::max<size_t>(1, batch_size)), shuffle_(shuffle), drop_last_(drop_last) {}

size_t DataLoader::num_batches() const {
    size_t total = dataset_.num_rows();
    if (drop_last_) return total / batch_size_;
    return (total + batch_size_ - 1) / batch_size_;
}

std::vector<Dataset> DataLoader::batches() const {
    Dataset ds = shuffle_ ? dataset_.shuffle(42) : dataset_;
    std::vector<Dataset> result;
    size_t total = ds.num_rows();

    for (size_t i = 0; i < total; i += batch_size_) {
        size_t end = std::min(i + batch_size_, total);
        if (drop_last_ && (end - i) < batch_size_) break;
        std::vector<std::vector<Value>> batch_rows(ds.rows().begin() + i, ds.rows().begin() + end);
        result.push_back(Dataset::from_rows(ds.columns(), std::move(batch_rows)));
    }
    return result;
}

Value DataLoader::to_value() const {
    auto self_loader = std::make_shared<DataLoader>(*this);
    std::map<std::string, Value> methods;

    methods["$type"] = Value::make_string("DataLoader");
    methods["batch_size"] = Value::make_int(static_cast<int64_t>(batch_size_));
    methods["num_batches"] = Value::make_int(static_cast<int64_t>(num_batches()));

    methods["batches"] = Value::make_native_fn("batches", 0, [self_loader](const std::vector<Value>&, SourceSpan) -> Value {
        auto batch_list = self_loader->batches();
        std::vector<Value> res;
        for (const auto& b : batch_list) {
            res.push_back(b.to_value());
        }
        return Value::make_array(std::move(res));
    });

    return Value::make_object(std::move(methods));
}

} // namespace nextviper
