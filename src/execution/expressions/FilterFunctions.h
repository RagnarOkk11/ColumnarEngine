#pragma once

#include "execution/OperatorsBase.h"
#include "execution/ExecutionHelper.h"

class FilterFunction {
public:
    virtual ~FilterFunction() = default;
    virtual std::vector<size_t> Evaluate(const RecordBatch& batch) = 0;
};

template <typename ColumnType, typename ValueType>
class NotEqFilterFunction : public FilterFunction {
public:
    NotEqFilterFunction(size_t column_ind, ValueType target_val)
        : column_ind_(column_ind), target_val_(target_val) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        ExecutionHelper::IterateColumnData<ColumnType>(batch, column_ind_, [&](const auto& value, size_t, size_t physical_idx) {
            if (value != target_val_) {
                selection_vector.push_back(physical_idx);
            }
        });
        return selection_vector;
    }

private:
    size_t column_ind_;
    ValueType target_val_;
};

template <typename ColumnType, typename ValueType>
class EqFilterFunction : public FilterFunction {
public:
    EqFilterFunction(size_t column_ind, ValueType target_val)
        : column_ind_(column_ind), target_val_(target_val) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        ExecutionHelper::IterateColumnData<ColumnType>(batch, column_ind_, [&](const auto& value, size_t, size_t real_ind) {
            if (value == target_val_) {
                selection_vector.push_back(real_ind);
            }
        });
        return selection_vector;
    }

private:
    size_t column_ind_;
    ValueType target_val_;
};
