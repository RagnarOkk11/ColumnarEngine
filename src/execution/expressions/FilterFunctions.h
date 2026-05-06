#pragma once

#include "execution/OperatorsBase.h"
#include "execution/expressions/AggExpHelper.h"

class FilterFunction {
public:
    virtual ~FilterFunction() = default;
    virtual std::vector<size_t> Evaluate(const RecordBatch& batch) = 0;
};

template <typename ColumnType, typename ValueType>
class NotEqFilterFunction : public FilterFunction {
public:
    NotEqFilterFunction(size_t column_ind, ValueType target_val)
        : column_ind_(column_ind), target_val_(std::move(target_val)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnType>(
            batch, column_ind_, [&](const auto& value, size_t, size_t real_ind) {
                if (value != target_val_) {
                    selection_vector.push_back(real_ind);
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
        : column_ind_(column_ind), target_val_(std::move(target_val)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnType>(
            batch, column_ind_, [&](const auto& value, size_t, size_t real_ind) {
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

template <typename ColumnType, typename ValueType>
class LikeFilterFunction : public FilterFunction {
public:
    LikeFilterFunction(size_t column_ind, ValueType pattern)
        : column_ind_(column_ind), pattern_(std::move(pattern)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnType>(
            batch, column_ind_, [&](const auto& value, size_t, size_t real_ind) {
                if (value.find(pattern_) != std::string::npos) {
                    selection_vector.push_back(real_ind);
                }
            });
        return selection_vector;
    }

private:
    size_t column_ind_;
    ValueType pattern_;
};

template <typename ColumnType, typename ValueType>
class NotLikeFilterFunction : public FilterFunction {
public:
    NotLikeFilterFunction(size_t column_ind, ValueType pattern)
        : column_ind_(column_ind), pattern_(std::move(pattern)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnType>(
            batch, column_ind_, [&](const auto& value, size_t, size_t real_ind) {
                if (value.find(pattern_) == std::string::npos) {
                    selection_vector.push_back(real_ind);
                }
            });
        return selection_vector;
    }

private:
    size_t column_ind_;
    ValueType pattern_;
};
