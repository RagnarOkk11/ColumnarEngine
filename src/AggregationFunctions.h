//
// Created by ragnarokk on 30.03.2026.
//

#ifndef COLUMNAR_ENGINE_AGGREGATIONFUNCTIONS_H
#define COLUMNAR_ENGINE_AGGREGATIONFUNCTIONS_H

#include <memory>

#include "OperatorsBase.h"

class AggregationFunction {
public:
    virtual ~AggregationFunction() = default;

    virtual void Update(const RecordBatch& batch) = 0;
    virtual std::unique_ptr<Column> Finalize() = 0;
};

class CountRowsAggregationFunction : public AggregationFunction {
public:
    void Update(const RecordBatch& batch) override {
        count_ += static_cast<int64_t>(batch.num_rows);
    }

    std::unique_ptr<Column> Finalize() override {
        auto result_column = std::make_unique<Int32Column>();
        result_column->Add(static_cast<int32_t>(count_));
        return result_column;
    }

private:
    int64_t count_ = 0;
};

struct SumOperation {
    template <typename ResultType, typename ValueType>
    static inline void Apply(ResultType& result, const ValueType& value) {
        result += value;
    }
};

struct MaxOperation {
    template <typename ResultType, typename ValueType>
    static inline void Apply(ResultType& result, const ValueType& value) {
        if (value > result) {
            result = value;
        }
    }
};

// maybe redundant
struct CountOperation {
    template <typename ResultType, typename ValueType>
    static inline void Apply(ResultType& result, const ValueType&) {
        ++result;
    }
};

template <typename T>
struct AggregationFunctionTraits;

template <>
struct AggregationFunctionTraits<Int16Column> {
    using ValueType = int16_t;
    using StateType = int32_t;
    using ResultColumnType = Int32Column;
};

template <>
struct AggregationFunctionTraits<Int32Column> {
    using ValueType = int32_t;
    using StateType = int64_t;
    using ResultColumnType = Int64Column;
};

template <>
struct AggregationFunctionTraits<Int64Column> {
    using ValueType = int64_t;
    using StateType = __int128_t;
    using ResultColumnType = Int128Column;
};

template <>
struct AggregationFunctionTraits<FloatColumn> {
    using ValueType = float;
    using StateType = double;
    using ResultColumnType = DoubleColumn;
};

template <typename ColumnType, typename Operation>
class TypedAggregationFunction : public AggregationFunction {
    using ResultColumnType = typename AggregationFunctionTraits<ColumnType>::ResultColumnType;
    using StateType = typename AggregationFunctionTraits<ColumnType>::StateType;

public:
    explicit TypedAggregationFunction(size_t column_index, StateType initial_state = StateType{})
        : column_index_(column_index), state_(initial_state) {
    }

    void Update(const RecordBatch& batch) override {
#ifndef NDEBUG
        auto* column = dynamic_cast<ColumnType*>(batch.columns[column_index_].get());
        ASSERT(column != nullptr);
#else
        auto* column = static_cast<ColumnType*>(batch.columns[column_index_].get());
#endif

        const auto& data = column->GetData();
        for (const auto& value : data) {
            Operation::Apply(state_, value);
        }
    }

    std::unique_ptr<Column> Finalize() override {
        auto result_column = std::make_unique<ResultColumnType>();
        result_column->Add(state_);
        return result_column;
    }

private:
    size_t column_index_;
    StateType state_;
};

template <typename ColumnType>
using SumAggregationFunction = TypedAggregationFunction<ColumnType, SumOperation>;

template <typename ColumnType>
using MaxAggregationFunction = TypedAggregationFunction<ColumnType, MaxOperation>;

template <typename ColumnType>
using CountAggregationFunction = TypedAggregationFunction<ColumnType, CountOperation>;

#endif  // COLUMNAR_ENGINE_AGGREGATIONFUNCTIONS_H
