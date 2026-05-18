#pragma once

#include "execution/Pipeline.h"
#include "io/ColumnarReader.h"
#include "column/ColumnBuilder.h"
#include "utils/Concurrency.h"

#include "absl/container/flat_hash_map.h"

#include <memory>
#include <vector>
#include <optional>

class GlobalAggregationFunction;
class GroupedAggregationFunction;
class FilterFunction;
class ScalarFunction;

// ========================= Source Operators =========================

class ScanOperator : public SourceOperator {
public:
    ScanOperator(const std::string& table_path, const std::vector<std::string>& column_names,
                 std::shared_ptr<const MetadataTable> metadata);

    std::unique_ptr<RecordBatch> GetData() override;

private:
    ColumnarReader reader_;
    std::shared_ptr<const MetadataTable> metadata_;
    std::vector<size_t> column_indices_;

    Atomic<size_t> current_chunk_{0};
    size_t total_chunks_ = 0;
    Atomic<bool> finished_empty_scan_{false};
};

// ========================= Transform Operators =========================

class FilterTransformOperator : public TransformOperator {
public:
    FilterTransformOperator(std::shared_ptr<FilterFunction> filter_function);

    std::unique_ptr<RecordBatch> Execute(std::unique_ptr<RecordBatch> batch) override;

private:
    std::shared_ptr<FilterFunction> filter_function_;
};

class ScalarTransformOperator : public TransformOperator {
public:
    ScalarTransformOperator(std::vector<std::unique_ptr<ScalarFunction>> scalar_functions);

    std::unique_ptr<RecordBatch> Execute(std::unique_ptr<RecordBatch> batch) override;

private:
    std::vector<std::unique_ptr<ScalarFunction>> scalar_functions_;
};

class DropTransformOperator : public TransformOperator {
public:
    DropTransformOperator(std::vector<size_t> column_stay_indices);

    std::unique_ptr<RecordBatch> Execute(std::unique_ptr<RecordBatch> batch) override;

private:
    std::vector<size_t> column_stay_indices_;
};

class ReorderTransformOperator : public TransformOperator {
public:
    ReorderTransformOperator(std::vector<size_t> new_indices);

    std::unique_ptr<RecordBatch> Execute(std::unique_ptr<RecordBatch> batch) override;

private:
    std::vector<size_t> new_indices_;
};

class LimitTransformOperator : public TransformOperator {
public:
    LimitTransformOperator(size_t limit);

    std::unique_ptr<RecordBatch> Execute(std::unique_ptr<RecordBatch> batch) override;

    bool IsPipelineDone() const override {
        return done_.load(std::memory_order_relaxed);
    }

private:
    size_t limit_;
    Atomic<size_t> cur_rows_{0};
    Atomic<bool> done_{false};
};

// ========================= Sink + Source Operators =========================

class AggregationSinkSourceOperator : public SinkOperator, public SourceOperator {
public:
    AggregationSinkSourceOperator(
        std::vector<std::unique_ptr<GlobalAggregationFunction>> aggregation_functions);

    void Sink(std::unique_ptr<RecordBatch> batch, size_t thread_id) override;
    void Finalize() && override;

    std::unique_ptr<RecordBatch> GetData() override;

private:
    std::vector<std::unique_ptr<GlobalAggregationFunction>> aggregation_functions_;
    std::unique_ptr<RecordBatch> result_batch_;
    Atomic<bool> emitted_{false};
};

class GroupBySinkSourceOperator : public SinkOperator, public SourceOperator {
public:
    GroupBySinkSourceOperator(std::vector<size_t> group_by_col_indices,
                              std::vector<ColumnType> key_types,
                              std::vector<std::unique_ptr<GroupedAggregationFunction>> agg_funcs,
                              size_t num_threads);

    void Sink(std::unique_ptr<RecordBatch> batch, size_t thread_id) override;
    void Finalize() && override;

    std::unique_ptr<RecordBatch> GetData() override;

private:
    struct alignas(64) GroupByThreadState {
        absl::flat_hash_map<std::string, uint32_t> hash_table;
        std::vector<std::shared_ptr<ColumnBuilder>> key_builders;
        size_t num_groups = 0;
    };

    std::vector<size_t> group_by_col_indices_;
    std::vector<ColumnType> key_types_;
    std::vector<std::unique_ptr<GroupedAggregationFunction>> agg_funcs_;

    std::vector<GroupByThreadState> thread_states_;
    std::unique_ptr<RecordBatch> result_batch_;
    Atomic<bool> emitted_{false};
};

class OrderBySinkSourceOperator : public SinkOperator, public SourceOperator {
public:
    OrderBySinkSourceOperator(std::vector<std::pair<size_t, bool>> sort_columns,
                              std::vector<ColumnType> column_types, std::optional<size_t> limit,
                              std::optional<size_t> offset, size_t num_threads);

    void Sink(std::unique_ptr<RecordBatch> batch, size_t thread_id) override;
    void Finalize() && override;

    std::unique_ptr<RecordBatch> GetData() override;

private:
    void TrimLocalBatch(size_t thread_id);

    struct alignas(64) OrderByThreadState {
        std::vector<std::shared_ptr<ColumnBuilder>> column_builders;
        size_t accum_num_rows = 0;
    };

    std::vector<std::pair<size_t, bool>> sort_columns_;
    const std::vector<ColumnType> column_types_;
    std::optional<size_t> limit_;
    std::optional<size_t> offset_;
    std::optional<size_t> total_limit_;

    std::vector<OrderByThreadState> thread_states_;
    std::unique_ptr<RecordBatch> result_batch_;
    Atomic<bool> emitted_{false};
};
