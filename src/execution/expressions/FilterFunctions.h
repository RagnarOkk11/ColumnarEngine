#pragma once

#include "execution/OperatorsBase.h"
#include "execution/expressions/AggExpHelper.h"

class FilterFunction {
public:
    virtual ~FilterFunction() = default;
    virtual std::vector<size_t> Evaluate(const RecordBatch& batch) = 0;
};

template <typename ColumnT, typename ValueType>
class NotEqFilterFunction : public FilterFunction {
public:
    NotEqFilterFunction(size_t column_ind, ValueType target_val)
        : column_ind_(column_ind), target_val_(std::move(target_val)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnT>(batch, column_ind_,
                                                 [&](const auto& value, size_t, size_t real_ind) {
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

template <typename ColumnT, typename ValueType>
class EqFilterFunction : public FilterFunction {
public:
    EqFilterFunction(size_t column_ind, ValueType target_val)
        : column_ind_(column_ind), target_val_(std::move(target_val)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnT>(batch, column_ind_,
                                                 [&](const auto& value, size_t, size_t real_ind) {
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

template <typename ColumnT, typename ValueType>
class GreaterFilterFunction : public FilterFunction {
public:
    GreaterFilterFunction(size_t column_ind, ValueType target_val)
        : column_ind_(column_ind), target_val_(std::move(target_val)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnT>(batch, column_ind_,
                                                 [&](const auto& value, size_t, size_t real_ind) {
                                                     if (value > target_val_) {
                                                         selection_vector.push_back(real_ind);
                                                     }
                                                 });
        return selection_vector;
    }

private:
    size_t column_ind_;
    ValueType target_val_;
};

template <typename ColumnT, typename ValueType>
class GreaterEqFilterFunction : public FilterFunction {
public:
    GreaterEqFilterFunction(size_t column_ind, ValueType target_val)
        : column_ind_(column_ind), target_val_(std::move(target_val)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnT>(batch, column_ind_,
                                                 [&](const auto& value, size_t, size_t real_ind) {
                                                     if (value >= target_val_) {
                                                         selection_vector.push_back(real_ind);
                                                     }
                                                 });
        return selection_vector;
    }

private:
    size_t column_ind_;
    ValueType target_val_;
};

template <typename ColumnT, typename ValueType>
class LessEqFilterFunction : public FilterFunction {
public:
    LessEqFilterFunction(size_t column_ind, ValueType target_val)
        : column_ind_(column_ind), target_val_(std::move(target_val)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnT>(batch, column_ind_,
                                                 [&](const auto& value, size_t, size_t real_ind) {
                                                     if (value <= target_val_) {
                                                         selection_vector.push_back(real_ind);
                                                     }
                                                 });
        return selection_vector;
    }

private:
    size_t column_ind_;
    ValueType target_val_;
};

template <typename ColumnT, typename ValueType>
class LikeFilterFunction : public FilterFunction {
public:
    LikeFilterFunction(size_t column_ind, ValueType pattern)
        : column_ind_(column_ind), pattern_(std::move(pattern)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnT>(
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

template <typename ColumnT, typename ValueType>
class NotLikeFilterFunction : public FilterFunction {
public:
    NotLikeFilterFunction(size_t column_ind, ValueType pattern)
        : column_ind_(column_ind), pattern_(std::move(pattern)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> selection_vector;
        selection_vector.reserve(batch.num_rows);

        AggExpHelper::IterateColumnData<ColumnT>(
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

class AndFilterFunction : public FilterFunction {
public:
    AndFilterFunction(std::unique_ptr<FilterFunction> left, std::unique_ptr<FilterFunction> right)
        : left_(std::move(left)), right_(std::move(right)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> left_res = left_->Evaluate(batch);
        std::vector<size_t> right_res = right_->Evaluate(batch);

        std::vector<size_t> result;
        result.reserve(left_res.size() + right_res.size());
        std::set_intersection(left_res.begin(), left_res.end(), right_res.begin(), right_res.end(),
                              std::back_inserter(result));
        return result;
    }

private:
    std::unique_ptr<FilterFunction> left_;
    std::unique_ptr<FilterFunction> right_;
};

class OrFilterFunction : public FilterFunction {
public:
    OrFilterFunction(std::unique_ptr<FilterFunction> left, std::unique_ptr<FilterFunction> right)
        : left_(std::move(left)), right_(std::move(right)) {
    }

    std::vector<size_t> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> left_res = left_->Evaluate(batch);
        std::vector<size_t> right_res = right_->Evaluate(batch);

        std::vector<size_t> result;
        result.reserve(left_res.size() + right_res.size());
        std::set_union(left_res.begin(), left_res.end(), right_res.begin(), right_res.end(),
                       std::back_inserter(result));
        return result;
    }

private:
    std::unique_ptr<FilterFunction> left_;
    std::unique_ptr<FilterFunction> right_;
};
