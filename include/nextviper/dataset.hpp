#pragma once

#include "nextviper/common.hpp"
#include "nextviper/value.hpp"
#include "nextviper/tensor.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace nextviper {

class Dataset {
public:
    Dataset();
    Dataset(std::vector<std::string> columns, std::vector<std::vector<Value>> rows);

    // Factories
    static Dataset load(const std::string& path);
    static Dataset from_csv(const std::string& csv_content);
    static Dataset from_rows(std::vector<std::string> columns, std::vector<std::vector<Value>> rows);

    // Properties
    size_t num_rows() const { return rows_.size(); }
    size_t num_cols() const { return columns_.size(); }
    const std::vector<std::string>& columns() const { return columns_; }
    const std::vector<std::vector<Value>>& rows() const { return rows_; }

    // Tabular operations
    Dataset clean(bool drop_nulls = true, const std::string& fill_strategy = "mean") const;
    Dataset shuffle(uint32_t seed = 42) const;
    std::pair<Dataset, Dataset> split(double train_ratio = 0.8, uint32_t seed = 42) const;
    Dataset select(const std::vector<std::string>& column_names) const;
    Dataset head(size_t n = 5) const;
    std::map<std::string, std::map<std::string, double>> describe() const;

    // Tensor conversion for AI pipeline
    Tensor to_tensor(const std::vector<std::string>& column_names = {}) const;

    // NextViper Value integration
    Value to_value() const;

private:
    std::vector<std::string> columns_;
    std::vector<std::vector<Value>> rows_;
    int get_column_index(const std::string& col_name) const;
};

class DataLoader {
public:
    DataLoader(Dataset dataset, size_t batch_size = 32, bool shuffle = true, bool drop_last = false);

    size_t batch_size() const { return batch_size_; }
    size_t num_batches() const;
    std::vector<Dataset> batches() const;

    Value to_value() const;

private:
    Dataset dataset_;
    size_t batch_size_ = 32;
    bool shuffle_ = true;
    bool drop_last_ = false;
};

} // namespace nextviper
