#pragma once

#include "io/ColumnarReader.h"
#include "column/ColumnBuilder.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>

class GlobalAggregationFunction;
class GroupedAggregationFunction;
class FilterFunction;
class ScalarFunction;

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
    ScanOperator(const std::string& table_path, const std::vector<std::string>& column_names,
                 std::shared_ptr<const MetadataTable> metadata);

    std::unique_ptr<RecordBatch> Run() override;

private:
    ColumnarReader reader_;
    std::shared_ptr<const MetadataTable> metadata_;
    std::vector<size_t> column_indices_;
    size_t current_chunk_ = 0;
    size_t total_chunks_ = 0;
    bool finished_empty_scan_ = false;
};

class AggregationOperator : public Operator {
public:
    AggregationOperator(
        std::unique_ptr<Operator> child,
        std::vector<std::unique_ptr<GlobalAggregationFunction>> aggregation_functions);

    std::unique_ptr<RecordBatch> Run() override;

private:
    std::unique_ptr<Operator> child_;
    std::vector<std::unique_ptr<GlobalAggregationFunction>> aggregation_functions_;
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
                    std::vector<std::shared_ptr<ColumnBuilder>> key_builders,
                    std::vector<std::unique_ptr<GroupedAggregationFunction>> agg_funcs);

    std::unique_ptr<RecordBatch> Run() override;

private:
    std::unique_ptr<Operator> child_;
    std::vector<size_t> group_by_col_indices_;
    std::vector<std::shared_ptr<ColumnBuilder>> key_builders_;
    std::vector<std::unique_ptr<GroupedAggregationFunction>> agg_funcs_;
    bool accumulated_ = false;
    size_t num_groups_ = 0;

    std::unordered_map<std::string, uint32_t> hash_table_;
};

class OrderByOperator : public Operator {
public:
    OrderByOperator(std::unique_ptr<Operator> child,
                    std::vector<std::pair<size_t, bool>> sort_columns, std::optional<size_t> limit,
                    std::optional<size_t> offset);

    std::unique_ptr<RecordBatch> Run() override;

private:
    void TrimCurBatch(bool is_final);

    std::unique_ptr<Operator> child_;
    std::vector<std::pair<size_t, bool>> sort_columns_;
    std::unique_ptr<RecordBatch> accumulated_batch_;
    std::vector<std::shared_ptr<ColumnBuilder>> accum_builders_;
    std::vector<ColumnType> accum_types_;
    size_t accum_num_rows_ = 0;
    std::vector<size_t> indices_;
    size_t current_idx_ = 0;
    std::optional<size_t> limit_;
    std::optional<size_t> offset_;
    std::optional<size_t> total_limit_;
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

class ScalarOperator : public Operator {
public:
    ScalarOperator(std::unique_ptr<Operator> child,
                   std::vector<std::unique_ptr<ScalarFunction>> scalar_functions);

    std::unique_ptr<RecordBatch> Run() override;

private:
    std::unique_ptr<Operator> child_;
    std::vector<std::unique_ptr<ScalarFunction>> scalar_functions_;
};

class DropOperator : public Operator {
public:
    DropOperator(std::unique_ptr<Operator> child, std::vector<size_t> column_stay_indices);

    std::unique_ptr<RecordBatch> Run() override;

private:
    std::unique_ptr<Operator> child_;
    std::vector<size_t> column_stay_indices_;
};

class ReorderOperator : public Operator {
public:
    ReorderOperator(std::unique_ptr<Operator> child, std::vector<size_t> new_indices)
        : child_(std::move(child)), new_indices_(std::move(new_indices)) {
    }

    std::unique_ptr<RecordBatch> Run() override {
        auto batch = child_->Run();
        if (!batch) {
            return nullptr;
        }

        auto new_batch = std::make_unique<RecordBatch>();
        new_batch->num_rows = batch->num_rows;
        new_batch->selection_vector = batch->selection_vector;

        new_batch->columns.reserve(new_indices_.size());
        for (size_t ind : new_indices_) {
            new_batch->columns.push_back(batch->columns[ind]);
        }

        return new_batch;
    }

private:
    std::unique_ptr<Operator> child_;
    std::vector<size_t> new_indices_;
};
