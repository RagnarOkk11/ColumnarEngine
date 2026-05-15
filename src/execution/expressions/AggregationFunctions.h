#pragma once

#include "execution/expressions/AggExpHelper.h"
#include "execution/OperatorsBase.h"

#include "column/Column.h"
#include "column/CharColumn.h"
#include "column/NumericColumn.h"
#include "column/StringColumn.h"
#include "column/TemporalColumn.h"

#include "column/ColumnFactory.h"

#include <memory>
#include <type_traits>
#include <unordered_set>
#include <vector>

class AggregationFunction {
public:
    virtual ~AggregationFunction() = default;
    AggregationFunction(ColumnType column_type) : column_type_(column_type) {
    }

    std::shared_ptr<Column> Finalize() {
        auto builder = ColumnFactory::MakeColumnBuilder(column_type_);
        WriteResult(builder);
        return builder->Finish();
    }

protected:
    ColumnType column_type_;

    virtual void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) = 0;
};

class GlobalAggregationFunction : public AggregationFunction {
public:
    GlobalAggregationFunction(ColumnType column_type) : AggregationFunction(column_type) {}

    virtual void Update(const RecordBatch& batch) = 0;
};

class GroupedAggregationFunction : public AggregationFunction {
public:
    GroupedAggregationFunction(ColumnType column_type) : AggregationFunction(column_type) {}

    virtual void Resize(size_t num_groups) = 0;
    virtual void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids) = 0;
};

struct SumOperation {
    template <typename ResultType, typename ValueType>
    static void Apply(ResultType& result, const ValueType& value) {
        result += value;
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

template <typename InputColumn, typename OutputColumn, typename Operation>
class TypedGlobalAggregationFunction : public GlobalAggregationFunction {
    using StateType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    TypedGlobalAggregationFunction(size_t column_index, ColumnType column_type)
        : GlobalAggregationFunction(column_type), column_index_(column_index) {
    }

    void Update(const RecordBatch& batch) override {
        AggExpHelper::IterateColumnData<InputColumn>(batch, column_index_,
                                                    [&](const auto& value, size_t, size_t) {
                                                        if (!has_data_) {
                                                            state_ = value;
                                                            has_data_ = true;
                                                        } else {
                                                            Operation::Apply(state_, value);
                                                        }
                                                    });
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        if (has_data_) {
            typed->AddValue(state_);
        } else {
            if constexpr (requires { Operation::template GetInitValue<StateType>(); }) {
                typed->AddValue(Operation::template GetInitValue<StateType>());
            } else {
                typed->AddValue(StateType{});
            }
        }
    }

private:
    size_t column_index_;
    StateType state_{};
    bool has_data_ = false;
};

template <typename InputColumn, typename OutputColumn, typename Operation>
class TypedGroupedAggregationFunction : public GroupedAggregationFunction {
    using StateType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    TypedGroupedAggregationFunction(size_t column_index, ColumnType column_type)
        : GroupedAggregationFunction(column_type), column_index_(column_index) {
    }

    void Resize(size_t num_groups) override {
        if (num_groups > states_.size()) {
            states_.resize(num_groups);
            has_data_.resize(num_groups, 0);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids) override {
        AggExpHelper::IterateColumnData<InputColumn>(batch, column_index_,
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

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        for (const auto& state : states_) {
            typed->AddValue(state);
        }
    }

private:
    size_t column_index_;
    std::vector<StateType> states_;
    std::vector<char> has_data_;
};

template <typename OutputColumn>
class CountGlobalAggregationFunction : public GlobalAggregationFunction {
    using ValueType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    CountGlobalAggregationFunction(ColumnType column_type)
        : GlobalAggregationFunction(column_type) {
    }

    void Update(const RecordBatch& batch) override {
        count_ += batch.num_rows;
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        typed->AddValue(static_cast<ValueType>(count_));
    }

private:
    int64_t count_ = 0;
};

template <typename OutputColumn>
class CountGroupedAggregationFunction : public GroupedAggregationFunction {
    using ValueType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    CountGroupedAggregationFunction(ColumnType column_type)
        : GroupedAggregationFunction(column_type) {
    }

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

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        for (int64_t c : counts_) {
            typed->AddValue(static_cast<ValueType>(c));
        }
    }

private:
    std::vector<int64_t> counts_;
};

template <typename InputColumn, typename OutputColumn, typename StateColumn>
class AvgGlobalAggregationFunction : public GlobalAggregationFunction {
    using StateType = StateColumn::ValueType;
    using OutputType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    AvgGlobalAggregationFunction(size_t column_index, ColumnType column_type)
        : GlobalAggregationFunction(column_type), column_index_(column_index) {
    }

    void Update(const RecordBatch& batch) override {
        AggExpHelper::IterateColumnData<InputColumn>(batch, column_index_,
                                                    [&](const auto& value, size_t, size_t) {
                                                        sum_ += value;
                                                        ++count_;
                                                    });
        has_data_ = true;
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        if (has_data_ && count_ > 0) {
            typed->AddValue(ComputeAvg());
        } else {
            typed->AddValue(OutputType{});
        }
    }

private:
    OutputType ComputeAvg() const {
        if constexpr (std::is_integral_v<StateType>) {
            OutputType int_part = static_cast<OutputType>(sum_ / count_);
            OutputType rem =
                static_cast<OutputType>(static_cast<long double>(sum_ % count_) / count_);
            return int_part + rem;
        } else {
            return static_cast<OutputType>(sum_) / static_cast<OutputType>(count_);
        }
    }

    size_t column_index_;
    StateType sum_ = 0;
    int64_t count_ = 0;
    bool has_data_ = false;
};

template <typename InputColumn, typename OutputColumn, typename StateColumn>
class AvgGroupedAggregationFunction : public GroupedAggregationFunction {
    using StateType = StateColumn::ValueType;
    using OutputType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    AvgGroupedAggregationFunction(size_t column_index, ColumnType column_type)
        : GroupedAggregationFunction(column_type), column_index_(column_index) {
    }

    void Resize(size_t num_groups) override {
        if (num_groups > states_.size()) {
            states_.resize(num_groups);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids) override {
        AggExpHelper::IterateColumnData<InputColumn>(batch, column_index_,
                                                    [&](const auto& value, size_t row_idx, size_t) {
                                                        uint32_t gid = group_ids[row_idx];
                                                        states_[gid].sum += value;
                                                        ++states_[gid].count;
                                                    });
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        for (const auto& state : states_) {
            if (state.count > 0) {
                typed->AddValue(ComputeAvg(state.sum, state.count));
            } else {
                typed->AddValue(OutputType{});
            }
        }
    }

private:
    static OutputType ComputeAvg(StateType sum, int64_t count) {
        if constexpr (std::is_integral_v<StateType>) {
            OutputType int_part = static_cast<OutputType>(sum / count);
            OutputType rem =
                static_cast<OutputType>(static_cast<long double>(sum % count) / count);
            return int_part + rem;
        } else {
            return static_cast<OutputType>(sum) / static_cast<OutputType>(count);
        }
    }

    struct State {
        StateType sum = 0;
        int64_t count = 0;
    };
    size_t column_index_;
    std::vector<State> states_;
};

template <typename InputColumn, typename OutputColumn>
class DistinctCountGlobalAggregationFunction : public GlobalAggregationFunction {
    using InputValueType = InputColumn::ValueType;
    using OutputValueType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    DistinctCountGlobalAggregationFunction(size_t column_index, ColumnType column_type)
        : GlobalAggregationFunction(column_type), column_index_(column_index) {
    }

    void Update(const RecordBatch& batch) override {
        AggExpHelper::IterateColumnData<InputColumn>(
            batch, column_index_,
            [&](const auto& value, size_t, size_t) {
                distinct_values_.insert(InputValueType(value));
            });
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        typed->AddValue(static_cast<OutputValueType>(distinct_values_.size()));
    }

private:
    size_t column_index_;
    std::unordered_set<InputValueType> distinct_values_;
};

template <typename InputColumn, typename OutputColumn>
class DistinctCountGroupedAggregationFunction : public GroupedAggregationFunction {
    using InputValueType = InputColumn::ValueType;
    using OutputValueType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    DistinctCountGroupedAggregationFunction(size_t column_index, ColumnType column_type)
        : GroupedAggregationFunction(column_type), column_index_(column_index) {
    }

    void Resize(size_t num_groups) override {
        if (num_groups > distinct_values_.size()) {
            distinct_values_.resize(num_groups);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids) override {
        AggExpHelper::IterateColumnData<InputColumn>(
            batch, column_index_, [&](const auto& value, size_t row_idx, size_t) {
                uint32_t gid = group_ids[row_idx];
                distinct_values_[gid].insert(InputValueType(value));
            });
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        for (const auto& set : distinct_values_) {
            typed->AddValue(static_cast<OutputValueType>(set.size()));
        }
    }

private:
    size_t column_index_;
    std::vector<std::unordered_set<InputValueType>> distinct_values_;
};
