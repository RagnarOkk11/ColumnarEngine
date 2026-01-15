//
// Created by ragnarokk on 06.01.2026.
//
// Эту штуку я только начал делать, поэтому тут мб не доделано

#ifndef COLUMNAR_ENGINE_OPERATORS_BASE_H
#define COLUMNAR_ENGINE_OPERATORS_BASE_H

#include <memory>
#include <vector>

#include "object.h"
#include "reader.h"

struct RecordBatch {
    size_t num_rows;
    std::vector<std::unique_ptr<Column>> columns;
};

class Operator {
public:
    virtual ~Operator() = default;

    virtual std::unique_ptr<RecordBatch> Run() = 0;
};

class ScanOperator : public Operator {
public:
    ScanOperator(Reader& reader, const std::vector<std::string>& column_names) : reader_(reader) {
        const auto& metadata = reader_.GetMetadata();
        for (const auto& name : column_names) {
            bool found = false;
            for (size_t i = 0; i < metadata.size(); ++i) {
                if (metadata[i].name == name) {
                    column_indices_.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("Column " + name + " not found in metadata");
            }
        }
        if (!metadata.empty()) {
            total_chunks_ = metadata[0].offsets.size();
        }
    }

    std::unique_ptr<RecordBatch> Run() override {
        if (current_chunk_ >= total_chunks_) {
            return nullptr;
        }

        auto record_batch = std::make_unique<RecordBatch>();
        record_batch->num_rows = 0;

        for (size_t col_idx : column_indices_) {
            auto column_data = reader_.GetColumnData(col_idx, current_chunk_);
            if (record_batch->num_rows == 0) {
                record_batch->num_rows = column_data->Size();
            }
            record_batch->columns.push_back(std::move(column_data));
        }

        ++current_chunk_;
        return record_batch;
    }

private:
    Reader& reader_;
    std::vector<size_t> column_indices_;
    size_t current_chunk_ = 0;
    size_t total_chunks_ = 0;
};

template <typename T>
struct AggregationTraits;

template <>
struct AggregationTraits<Int32Column> {
    using ValueType = int32_t;
    using SumType = int64_t;
    using ResultColumnType = Int32Column;
};

template <>
struct AggregationTraits<FloatColumn> {
    using ValueType = float;
    using SumType = double;
    using ResultColumnType = FloatColumn;
};

template <typename ColumnType>
class AggregationOperator : public Operator {
public:
    using T = typename AggregationTraits<ColumnType>::SumType;
    using ResultColumnType = typename AggregationTraits<ColumnType>::ResultColumnType;

    AggregationOperator(std::unique_ptr<Operator> child) : child_(std::move(child)) {
    }

    std::unique_ptr<RecordBatch> Run() override {
        if (finished_) {
            return nullptr;
        }
        while (auto batch = child_->Run()) {
            total_rows_ += batch->num_rows;
            auto* column = static_cast<ColumnType*>(batch->columns[0].get());
            const auto& data = column->GetData();
            for (const auto& value : data) {
                sum_ += value;
            }
        }
        auto result_batch = std::make_unique<RecordBatch>();

        auto sum_column = std::make_unique<ResultColumnType>();
        sum_column->Add(sum_);

        auto count_column = std::make_unique<ColumnType>();
        count_column->Add(total_rows_);

        result_batch->num_rows = 1;
        result_batch->columns.push_back(std::move(sum_column));
        result_batch->columns.push_back(std::move(count_column));

        finished_ = true;
        return result_batch;
    }

private:
    std::unique_ptr<Operator> child_;
    T sum_ = 0;
    uint64_t total_rows_ = 0;
    bool finished_ = false;
};

#endif  // COLUMNAR_ENGINE_OPERATORS_BASE_H
