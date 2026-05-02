//
// Created by ragnarokk on 30.03.2026.
//

#ifndef COLUMNAR_ENGINE_AGGREGATIONFUNCTIONS_H
#define COLUMNAR_ENGINE_AGGREGATIONFUNCTIONS_H

#include "OperatorsBase.h"

#include <memory>
#include <limits>
#include <unordered_set>

class AggregationFunction {
public:
    virtual ~AggregationFunction() = default;

    virtual void Update(const RecordBatch& batch,
                        const std::vector<uint32_t>* group_ids = nullptr) = 0;
    virtual std::shared_ptr<Column> Finalize() = 0;
};

struct SumOperation {
    template <typename ResultType, typename ValueType>
    static inline void Apply(ResultType& result, const ValueType& value) {
        result += value;
    }

    template <typename ResultType>
    static constexpr ResultType GetInitValue() {
        return 0;
    }
};

struct MaxOperation {
    template <typename ResultType, typename ValueType>
    static inline void Apply(ResultType& result, const ValueType& value) {
        if (value > result) {
            result = value;
        }
    }

    template <typename ResultType>
    static constexpr ResultType GetInitValue() {
        return std::numeric_limits<ResultType>::lowest();
    }
};

struct MinOperation {
    template <typename ResultType, typename ValueType>
    static inline void Apply(ResultType& result, const ValueType& value) {
        if (value < result) {
            result = value;
        }
    }

    template <typename ResultType>
    static constexpr ResultType GetInitValue() {
        return std::numeric_limits<ResultType>::max();
    }
};

// OPTIMIZE: maybe redundant
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
struct AggregationFunctionTraits<Int128Column> {
    using ValueType = __int128_t;
    using StateType = __int128_t;
    using ResultColumnType = Int128Column;
};

template <>
struct AggregationFunctionTraits<FloatColumn> {
    using ValueType = float;
    using StateType = double;
    using ResultColumnType = DoubleColumn;
};

template <>
struct AggregationFunctionTraits<DoubleColumn> {
    using ValueType = double;
    using StateType = long double;
    using ResultColumnType = LongDoubleColumn;
};

template <>
struct AggregationFunctionTraits<LongDoubleColumn> {
    using ValueType = long double;
    using StateType = long double;
    using ResultColumnType = LongDoubleColumn;
};

template <>
struct AggregationFunctionTraits<CharColumn> {
    using ValueType = char;
    using StateType = int16_t;
    using ResultColumnType = Int16Column;
};

template <>
struct AggregationFunctionTraits<DateColumn> {
    using ValueType = int32_t;
    using StateType = int32_t;
    using ResultColumnType = DateColumn;
};

template <>
struct AggregationFunctionTraits<TimestampColumn> {
    using ValueType = int64_t;
    using StateType = int64_t;
    using ResultColumnType = TimestampColumn;
};

template <>
struct AggregationFunctionTraits<StringColumn> {
    using ValueType = std::string;
};

template <typename ColumnType, typename Operation>
class TypedAggregationFunction : public AggregationFunction {
    using ResultColumnType = typename AggregationFunctionTraits<ColumnType>::ResultColumnType;
    using StateType = typename AggregationFunctionTraits<ColumnType>::StateType;

public:
    explicit TypedAggregationFunction(size_t column_index) : column_index_(column_index) {
    }

    void Update(const RecordBatch& batch,
                const std::vector<uint32_t>* group_ids = nullptr) override {
        auto* column = static_cast<ColumnType*>(batch.columns[column_index_].get());
        const auto& data = column->GetData();

        if (!batch.selection_vector) {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                uint32_t gid = group_ids ? (*group_ids)[i] : 0;
                if (gid >= states_.size()) {
                    states_.resize(gid + 1, Operation::template GetInitValue<StateType>());
                }
                Operation::Apply(states_[gid], data[i]);
            }
        } else {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                uint32_t gid = group_ids ? (*group_ids)[i] : 0;
                if (gid >= states_.size()) {
                    states_.resize(gid + 1, Operation::template GetInitValue<StateType>());
                }
                uint32_t ind = (*batch.selection_vector)[i];
                Operation::Apply(states_[gid], data[ind]);
            }
        }
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<ResultColumnType>();
        if (states_.empty()) {
            result_column->Add(Operation::template GetInitValue<StateType>());
        } else {
            for (const auto& state : states_) {
                result_column->Add(state);
            }
        }
        return result_column;
    }

private:
    size_t column_index_;
    std::vector<StateType> states_;
};

template <typename ColumnType>
using SumAggregationFunction = TypedAggregationFunction<ColumnType, SumOperation>;

template <typename ColumnType>
using MinAggregationFunction = TypedAggregationFunction<ColumnType, MinOperation>;

template <typename ColumnType>
using MaxAggregationFunction = TypedAggregationFunction<ColumnType, MaxOperation>;

class CountAggregationFunction : public AggregationFunction {
public:
    void Update(const RecordBatch& batch,
                const std::vector<uint32_t>* group_ids = nullptr) override {
        for (size_t i = 0; i < batch.num_rows; ++i) {
            uint32_t gid = group_ids ? (*group_ids)[i] : 0;
            if (gid >= counts_.size()) {
                counts_.resize(gid + 1, 0);
            }
            counts_[gid]++;
        }
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<Int32Column>();
        if (counts_.empty()) {
            result_column->Add(0);
        } else {
            for (int64_t c : counts_) {
                result_column->Add(static_cast<int32_t>(c));
            }
        }
        return result_column;
    }

private:
    std::vector<int64_t> counts_;
};

template <typename ColumnType>
class DistinctCountAggregationFunction : public AggregationFunction {
    using ResultColumnType = Int64Column;
    using ValueType = typename AggregationFunctionTraits<ColumnType>::ValueType;

public:
    explicit DistinctCountAggregationFunction(size_t column_index) : column_index_(column_index) {
    }

    void Update(const RecordBatch& batch,
                const std::vector<uint32_t>* group_ids = nullptr) override {
        auto* column = static_cast<ColumnType*>(batch.columns[column_index_].get());
        const auto& data = column->GetData();

        if (!batch.selection_vector) {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                uint32_t gid = group_ids ? (*group_ids)[i] : 0;
                if (gid >= distinct_values_.size()) {
                    distinct_values_.resize(gid + 1);
                }
                if constexpr (std::is_same_v<ColumnType, StringColumn>) {
                    distinct_values_[gid].emplace(data[i]);
                } else {
                    distinct_values_[gid].insert(data[i]);
                }
            }
        } else {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                uint32_t gid = group_ids ? (*group_ids)[i] : 0;
                if (gid >= distinct_values_.size()) {
                    distinct_values_.resize(gid + 1);
                }
                uint32_t ind = (*batch.selection_vector)[i];
                if constexpr (std::is_same_v<ColumnType, StringColumn>) {
                    distinct_values_[gid].emplace(data[ind]);
                } else {
                    distinct_values_[gid].insert(data[ind]);
                }
            }
        }
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<ResultColumnType>();
        if (distinct_values_.empty()) {
            result_column->Add(0);
        } else {
            for (const auto& set : distinct_values_) {
                result_column->Add(static_cast<int64_t>(set.size()));
            }
        }
        return result_column;
    }

private:
    size_t column_index_;
    std::vector<std::unordered_set<ValueType>> distinct_values_;
};

template <typename ColumnType>
class AvgAggregationFunction : public AggregationFunction {
public:
    using StateType = typename AggregationFunctionTraits<ColumnType>::StateType;

    explicit AvgAggregationFunction(size_t column_index) : column_index_(column_index) {
    }

    void Update(const RecordBatch& batch,
                const std::vector<uint32_t>* group_ids = nullptr) override {
        auto* column = static_cast<ColumnType*>(batch.columns[column_index_].get());
        const auto& data = column->GetData();

        if (!batch.selection_vector) {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                uint32_t gid = group_ids ? (*group_ids)[i] : 0;
                if (gid >= sums_.size()) {
                    sums_.resize(gid + 1, 0);
                    counts_.resize(gid + 1, 0);
                }
                sums_[gid] += static_cast<StateType>(data[i]);
                counts_[gid]++;
            }
        } else {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                uint32_t gid = group_ids ? (*group_ids)[i] : 0;
                if (gid >= sums_.size()) {
                    sums_.resize(gid + 1, 0);
                    counts_.resize(gid + 1, 0);
                }
                uint32_t ind = (*batch.selection_vector)[i];
                sums_[gid] += static_cast<StateType>(data[ind]);
                counts_[gid]++;
            }
        }
    }

    std::shared_ptr<Column> Finalize() override {
        auto result_column = std::make_shared<LongDoubleColumn>();
        if (sums_.empty()) {
            result_column->Add(0.0);
        } else {
            for (size_t gid = 0; gid < sums_.size(); ++gid) {
                if (counts_[gid] == 0) {
                    result_column->Add(0.0);
                } else {
                    StateType quotient = sums_[gid] / counts_[gid];
                    long double rem =
                        static_cast<long double>(sums_[gid] - quotient * counts_[gid]) /
                        counts_[gid];
                    result_column->Add(static_cast<long double>(quotient) + rem);
                }
            }
        }
        return result_column;
    }

private:
    size_t column_index_;
    std::vector<StateType> sums_;
    std::vector<int64_t> counts_;
};

#endif  // COLUMNAR_ENGINE_AGGREGATIONFUNCTIONS_H
