#pragma once

#include "execution/expressions/AggExpHelper.h"
#include "execution/OperatorsBase.h"

#include "column/Column.h"
#include "column/CharColumn.h"
#include "column/NumericColumn.h"
#include "column/StringColumn.h"
#include "column/TemporalColumn.h"

#include <memory>
#include <unordered_set>
#include <vector>

class GlobalAggregationFunction {
public:
    virtual ~GlobalAggregationFunction() = default;
    virtual void Update(const RecordBatch& batch) = 0;
    virtual std::shared_ptr<Column> Finalize() = 0;
};

class GroupedAggregationFunction {
public:
    virtual ~GroupedAggregationFunction() = default;
    virtual void Resize(size_t num_groups) = 0;
    virtual void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids) = 0;
    virtual std::shared_ptr<Column> Finalize() = 0;
};

template <typename T>
struct AggregationFunctionTraits;

#define DEFINE_AGG_TRAIT(COL_TYPE, VAL_TYPE, STATE_TYPE, RES_TYPE) \
    template <>                                                    \
    struct AggregationFunctionTraits<COL_TYPE> {                   \
        using ValueType = VAL_TYPE;                                \
        using StateType = STATE_TYPE;                              \
        using ResultColumnType = RES_TYPE;                         \
    }

DEFINE_AGG_TRAIT(Int16Column, int16_t, int32_t, Int32Column);
DEFINE_AGG_TRAIT(Int32Column, int32_t, int64_t, Int64Column);
DEFINE_AGG_TRAIT(Int64Column, int64_t, Int128, Int128Column);
DEFINE_AGG_TRAIT(Int128Column, Int128, Int128, Int128Column);
DEFINE_AGG_TRAIT(FloatColumn, float, double, DoubleColumn);
DEFINE_AGG_TRAIT(DoubleColumn, double, long double, LongDoubleColumn);
DEFINE_AGG_TRAIT(LongDoubleColumn, long double, long double, LongDoubleColumn);
DEFINE_AGG_TRAIT(CharColumn, char, char, CharColumn);
DEFINE_AGG_TRAIT(DateColumn, int32_t, int32_t, DateColumn);
DEFINE_AGG_TRAIT(TimestampColumn, int64_t, int64_t, TimestampColumn);

template <>
struct AggregationFunctionTraits<StringColumn> {
    using ValueType = std::string;
    using StateType = std::string;
    using ResultColumnType = StringColumn;
};

#undef DEFINE_AGG_TRAIT

struct SumOperation {
    template <typename ResultType, typename ValueType>
    static void Apply(ResultType& result, const ValueType& value) {
        result += value;
    }
    template <typename ResultType>
    static constexpr ResultType GetInitValue() {
        return 0;
    }
};

struct MaxOperation {
    template <typename ResultType, typename ValueType>
    static void Apply(ResultType& result, const ValueType& value) {
        if (value > result) {
            result = value;
        }
    }
};

struct MinOperation {
    template <typename ResultType, typename ValueType>
    static inline void Apply(ResultType& result, const ValueType& value) {
        if (value < result) {
            result = value;
        }
    }
};

template <typename ColumnType, typename Operation>
class TypedGlobalAggregationFunction : public GlobalAggregationFunction {
    using ResultColumnType = AggregationFunctionTraits<ColumnType>::ResultColumnType;
    using StateType = AggregationFunctionTraits<ColumnType>::StateType;

public:
    explicit TypedGlobalAggregationFunction(size_t column_index) : column_index_(column_index) {
        if constexpr (requires { Operation::template GetInitValue<StateType>(); }) {
            state_ = Operation::template GetInitValue<StateType>();
        } else {
            state_ = StateType{};
        }
    }

    void Update(const RecordBatch& batch) override {
        AggExpHelper::IterateColumnData<ColumnType>(batch, column_index_,
                                                    [&](const auto& value, size_t, size_t) {
                                                        if (!has_data_) {
                                                            state_ = value;
                                                            has_data_ = true;
                                                        } else {
                                                            Operation::Apply(state_, value);
                                                        }
                                                    });
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<ResultColumnType>();
        if (has_data_) {
            result_column->AddValue(state_);
        } else {
            if constexpr (requires { Operation::template GetInitValue<StateType>(); }) {
                result_column->AddValue(Operation::template GetInitValue<StateType>());
            } else {
                result_column->AddValue(StateType{});
            }
        }
        return result_column;
    }

private:
    size_t column_index_;
    StateType state_;
    bool has_data_ = false;
};

template <typename ColumnType, typename Operation>
class TypedGroupedAggregationFunction : public GroupedAggregationFunction {
    using ResultColumnType = AggregationFunctionTraits<ColumnType>::ResultColumnType;
    using StateType = AggregationFunctionTraits<ColumnType>::StateType;

public:
    explicit TypedGroupedAggregationFunction(size_t column_index) : column_index_(column_index) {
    }

    void Resize(size_t num_groups) override {
        if (num_groups > states_.size()) {
            states_.resize(num_groups);
            has_data_.resize(num_groups, 0);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids) override {
        AggExpHelper::IterateColumnData<ColumnType>(batch, column_index_,
                                                    [&](const auto& value, size_t row_idx, size_t) {
                                                        uint32_t gid = group_ids[row_idx];
                                                        if (!has_data_[gid]) {
                                                            states_[gid] = value;
                                                            has_data_[gid] = 1;
                                                        } else {
                                                            Operation::Apply(states_[gid], value);
                                                        }
                                                    });
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<ResultColumnType>();
        for (const auto& state : states_) {
            result_column->AddValue(state);
        }
        return result_column;
    }

private:
    size_t column_index_;
    std::vector<StateType> states_;
    std::vector<char> has_data_;
};

class CountGlobalAggregationFunction : public GlobalAggregationFunction {
public:
    void Update(const RecordBatch& batch) override {
        count_ += batch.num_rows;
    }
    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<Int64Column>();
        result_column->AddValue(count_);
        return result_column;
    }

private:
    int64_t count_ = 0;
};

class CountGroupedAggregationFunction : public GroupedAggregationFunction {
public:
    void Resize(size_t num_groups) override {
        if (num_groups > counts_.size()) {
            counts_.resize(num_groups, 0);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids) override {
        for (size_t i = 0; i < batch.num_rows; ++i) {
            uint32_t gid = group_ids[i];
            ++counts_[gid];
        }
    }
    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<Int64Column>();
        for (int64_t c : counts_) {
            result_column->AddValue(c);
        }
        return result_column;
    }

private:
    std::vector<int64_t> counts_;
};

template <typename ColumnType>
class AvgGlobalAggregationFunction : public GlobalAggregationFunction {
    using StateType = AggregationFunctionTraits<ColumnType>::StateType;

public:
    explicit AvgGlobalAggregationFunction(size_t column_index) : column_index_(column_index) {
    }

    void Update(const RecordBatch& batch) override {
        AggExpHelper::IterateColumnData<ColumnType>(batch, column_index_,
                                                    [&](const auto& value, size_t, size_t) {
                                                        sum_ += value;
                                                        ++count_;
                                                    });
        has_data_ = true;
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<DoubleColumn>();
        if (has_data_ && count_ > 0) {
            result_column->AddValue(static_cast<double>(sum_) / static_cast<double>(count_));
        } else {
            result_column->AddValue(0.0);
        }
        return result_column;
    }

private:
    size_t column_index_;
    StateType sum_ = 0;
    int64_t count_ = 0;
    bool has_data_ = false;
};

template <typename ColumnType>
class AvgGroupedAggregationFunction : public GroupedAggregationFunction {
    using StateType = AggregationFunctionTraits<ColumnType>::StateType;

public:
    explicit AvgGroupedAggregationFunction(size_t column_index) : column_index_(column_index) {
    }

    void Resize(size_t num_groups) override {
        if (num_groups > states_.size()) {
            states_.resize(num_groups);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids) override {
        AggExpHelper::IterateColumnData<ColumnType>(batch, column_index_,
                                                    [&](const auto& value, size_t row_idx, size_t) {
                                                        uint32_t gid = group_ids[row_idx];
                                                        states_[gid].sum += value;
                                                        ++states_[gid].count;
                                                    });
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<DoubleColumn>();
        for (const auto& state : states_) {
            if (state.count > 0) {
                result_column->AddValue(static_cast<double>(state.sum) /
                                        static_cast<double>(state.count));
            } else {
                result_column->AddValue(0.0);
            }
        }
        return result_column;
    }

private:
    struct State {
        StateType sum = 0;
        int64_t count = 0;
    };
    size_t column_index_;
    std::vector<State> states_;
};

template <typename ColumnType>
class DistinctCountGlobalAggregationFunction : public GlobalAggregationFunction {
    using ValueType = AggregationFunctionTraits<ColumnType>::ValueType;

public:
    explicit DistinctCountGlobalAggregationFunction(size_t column_index)
        : column_index_(column_index) {
    }

    void Update(const RecordBatch& batch) override {
        AggExpHelper::IterateColumnData<ColumnType>(
            batch, column_index_,
            [&](const auto& value, size_t, size_t) { distinct_values_.insert(ValueType(value)); });
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<Int32Column>();
        result_column->AddValue(static_cast<int32_t>(distinct_values_.size()));
        return result_column;
    }

private:
    size_t column_index_;
    std::unordered_set<ValueType> distinct_values_;
};

template <typename ColumnType>
class DistinctCountGroupedAggregationFunction : public GroupedAggregationFunction {
    using ValueType = AggregationFunctionTraits<ColumnType>::ValueType;

public:
    explicit DistinctCountGroupedAggregationFunction(size_t column_index)
        : column_index_(column_index) {
    }

    void Resize(size_t num_groups) override {
        if (num_groups > distinct_values_.size()) {
            distinct_values_.resize(num_groups);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids) override {
        AggExpHelper::IterateColumnData<ColumnType>(
            batch, column_index_, [&](const auto& value, size_t row_idx, size_t) {
                uint32_t gid = group_ids[row_idx];
                distinct_values_[gid].insert(ValueType(value));
            });
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<Int32Column>();
        for (const auto& set : distinct_values_) {
            result_column->AddValue(static_cast<int32_t>(set.size()));
        }
        return result_column;
    }

private:
    size_t column_index_;
    std::vector<std::unordered_set<ValueType>> distinct_values_;
};
