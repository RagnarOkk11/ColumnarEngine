//
// Created by ragnarokk on 06.01.2026.
//

#ifndef COLUMNAR_ENGINE_OPERATORS_BASE_H
#define COLUMNAR_ENGINE_OPERATORS_BASE_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "ColumnarReader.h"
#include "macro.h"
#include "Types.h"

class AggregationFunction;
class GroupedAggregationFunction;
class FilterFunction;

struct RecordBatch {
    size_t num_rows;
    std::shared_ptr<const std::vector<size_t>> selection_vector;
    std::vector<std::shared_ptr<Column>> columns;
};

class Operator {
public:
    virtual ~Operator() = default;

    virtual std::unique_ptr<RecordBatch> Run() = 0;
};

class ScanOperator : public Operator {
public:
    ScanOperator(const std::string& table_path, const std::vector<std::string>& column_names);

    std::unique_ptr<RecordBatch> Run() override;

private:
    ColumnarReader reader_;
    std::vector<size_t> column_indices_;
    size_t current_chunk_ = 0;
    size_t total_chunks_ = 0;
};

class AggregationOperator : public Operator {
public:
    AggregationOperator(std::unique_ptr<Operator> child,
                        std::vector<std::unique_ptr<AggregationFunction>> aggregation_functions);

    std::unique_ptr<RecordBatch> Run() override;

private:
    std::unique_ptr<Operator> child_;
    std::vector<std::unique_ptr<AggregationFunction>> aggregation_functions_;
    bool finished_ = false;
};

class FilterOperator : public Operator {
public:
    FilterOperator(std::unique_ptr<Operator> child,
                   std::shared_ptr<FilterFunction> filter_function);

    std::unique_ptr<RecordBatch> Run() override;

private:
    std::unique_ptr<Operator> child_;
    std::shared_ptr<FilterFunction> filter_function_;
};

class GroupByOperator : public Operator {
public:
    GroupByOperator(std::unique_ptr<Operator> child, std::vector<size_t> group_by_col_indices,
                    std::vector<std::shared_ptr<Column>> empty_key_columns,
                    std::vector<std::unique_ptr<AggregationFunction>> agg_funcs);

    std::unique_ptr<RecordBatch> Run() override;

private:
    std::unique_ptr<Operator> child_;
    std::vector<size_t> group_by_col_indices_;
    std::vector<std::shared_ptr<Column>> key_columns_;
    std::vector<std::unique_ptr<AggregationFunction>> agg_funcs_;
    bool accumulated_ = false;

    std::unordered_map<std::string, uint32_t> hash_table_;
};

class OrderByOperator : public Operator {
public:
    OrderByOperator(std::unique_ptr<Operator> child,
                    std::vector<std::pair<size_t, bool>> sort_columns,
                    std::optional<uint32_t> limit);

    std::unique_ptr<RecordBatch> Run() override;

private:
    std::unique_ptr<Operator> child_;
    std::vector<std::pair<size_t, bool>> sort_columns_;
    std::unique_ptr<RecordBatch> accumulated_batch_;
    std::vector<size_t> indices_;
    size_t current_idx_ = 0;
    std::optional<size_t> limit_;
    bool accumulated_ = false;
};

class LimitOperator : public Operator {
public:
    LimitOperator(std::unique_ptr<Operator> child, size_t limit);

    std::unique_ptr<RecordBatch> Run() override;

private:
    std::unique_ptr<Operator> child_;
    size_t limit_;
    size_t cur_rows_ = 0;
};

#endif  // COLUMNAR_ENGINE_OPERATORS_BASE_H
