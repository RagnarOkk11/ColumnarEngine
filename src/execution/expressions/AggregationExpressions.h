#pragma once

#include "execution/expressions/AggregationFunctions.h"
#include "execution/expressions/AggExpHelper.h"
#include "column/Schema.h"

#include <memory>
#include <string>
#include <utility>

template <typename InputColumn>
struct SumResultTrait {
    using Type = InputColumn;
    static constexpr ColumnType kOutTypeEnum = ColumnType::INT64;
};

template <>
struct SumResultTrait<Int16Column> {
    using Type = Int32Column;
    static constexpr ColumnType kOutTypeEnum = ColumnType::INT32;
};
template <>
struct SumResultTrait<Int32Column> {
    using Type = Int64Column;
    static constexpr ColumnType kOutTypeEnum = ColumnType::INT64;
};
template <>
struct SumResultTrait<Int64Column> {
    using Type = Int128Column;
    static constexpr ColumnType kOutTypeEnum = ColumnType::INT128;
};
template <>
struct SumResultTrait<Int128Column> {
    using Type = Int128Column;
    static constexpr ColumnType kOutTypeEnum = ColumnType::INT128;
};
template <>
struct SumResultTrait<FloatColumn> {
    using Type = DoubleColumn;
    static constexpr ColumnType kOutTypeEnum = ColumnType::DOUBLE;
};
template <>
struct SumResultTrait<DoubleColumn> {
    using Type = LongDoubleColumn;
    static constexpr ColumnType kOutTypeEnum = ColumnType::LONGDOUBLE;
};
template <>
struct SumResultTrait<LongDoubleColumn> {
    using Type = LongDoubleColumn;
    static constexpr ColumnType kOutTypeEnum = ColumnType::LONGDOUBLE;
};

class AggregateExpression {
public:
    virtual ~AggregateExpression() = default;

    explicit AggregateExpression(std::string column_name, std::string output_name = "")
        : column_name_(std::move(column_name)), output_name_(std::move(output_name)) {
    }

    std::string GetName() const {
        return column_name_;
    }

    void CollectRequiredColumns(std::vector<std::string>& required_columns) {
        if (!column_name_.empty() && column_name_ != "*") {
            required_columns.push_back(column_name_);
        }
    }

    virtual std::string GetOutputName() const {
        return output_name_.empty() ? column_name_ : output_name_;
    }

    virtual std::unique_ptr<GlobalAggregationFunction> CreateGlobalAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) = 0;

    virtual std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) = 0;

protected:
    std::string column_name_;
    std::string output_name_;
};

class CountExpression : public AggregateExpression {
public:
    explicit CountExpression(std::string column_name, std::string output_name = "count")
        : AggregateExpression(std::move(column_name), std::move(output_name)) {
    }

    std::unique_ptr<GlobalAggregationFunction> CreateGlobalAggregationFunction(
        const Schema&, Schema& output_schema, size_t num_threads) override {
        output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
        return std::make_unique<CountGlobalAggregationFunction<Int64Column>>(ColumnType::INT64, num_threads);
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema&, Schema& output_schema, size_t num_threads) override {
        output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
        return std::make_unique<CountGroupedAggregationFunction<Int64Column>>(ColumnType::INT64, num_threads);
    }
};

class DistinctCountExpression : public AggregateExpression {
public:
    explicit DistinctCountExpression(std::string column_name, std::string output_name = "")
        : AggregateExpression(std::move(column_name), std::move(output_name)) {
    }

    std::string GetOutputName() const override {
        if (!output_name_.empty()) {
            return output_name_;
        }
        return "distinct_count_" + column_name_;
    }

    std::unique_ptr<GlobalAggregationFunction> CreateGlobalAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type, [&]<typename T>(ColumnType) -> std::unique_ptr<GlobalAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
                return std::make_unique<DistinctCountGlobalAggregationFunction<T, Int64Column>>(
                    column_ind, ColumnType::INT64, num_threads);
            });
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType) -> std::unique_ptr<GroupedAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
                return std::make_unique<DistinctCountGroupedAggregationFunction<T, Int64Column>>(
                    column_ind, ColumnType::INT64, num_threads);
            });
    }
};

class MinExpression : public AggregateExpression {
public:
    explicit MinExpression(std::string column_name, std::string output_name = "")
        : AggregateExpression(std::move(column_name), std::move(output_name)) {
    }

    std::string GetOutputName() const override {
        if (!output_name_.empty()) {
            return output_name_;
        }
        return "min_" + column_name_;
    }

    std::unique_ptr<GlobalAggregationFunction> CreateGlobalAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType t) -> std::unique_ptr<GlobalAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), t);
                return std::make_unique<TypedGlobalAggregationFunction<T, T, MinOperation>>(
                    column_ind, t, num_threads);
            });
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType t) -> std::unique_ptr<GroupedAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), t);
                return std::make_unique<TypedGroupedAggregationFunction<T, T, MinOperation>>(
                    column_ind, t, num_threads);
            });
    }
};

class MaxExpression : public AggregateExpression {
public:
    explicit MaxExpression(std::string column_name, std::string output_name = "")
        : AggregateExpression(std::move(column_name), std::move(output_name)) {
    }

    std::string GetOutputName() const override {
        if (!output_name_.empty()) {
            return output_name_;
        }
        return "max_" + column_name_;
    }

    std::unique_ptr<GlobalAggregationFunction> CreateGlobalAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType t) -> std::unique_ptr<GlobalAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), t);
                return std::make_unique<TypedGlobalAggregationFunction<T, T, MaxOperation>>(
                    column_ind, t, num_threads);
            });
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType t) -> std::unique_ptr<GroupedAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), t);
                return std::make_unique<TypedGroupedAggregationFunction<T, T, MaxOperation>>(
                    column_ind, t, num_threads);
            });
    }
};

class SumExpression : public AggregateExpression {
public:
    explicit SumExpression(std::string column_name, std::string output_name = "")
        : AggregateExpression(std::move(column_name), std::move(output_name)) {
    }

    std::string GetOutputName() const override {
        if (!output_name_.empty()) {
            return output_name_;
        }
        return "sum_" + column_name_;
    }

    std::unique_ptr<GlobalAggregationFunction> CreateGlobalAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        if (column_type == ColumnType::STRING || column_type == ColumnType::CHAR ||
            column_type == ColumnType::DATE || column_type == ColumnType::TIMESTAMP) {
            THROW_RUNTIME_ERROR("SUM operation is not supported for this column type");
        }

        return AggExpHelper::DispatchNumeric(
            column_type,
            [&]<typename InputColumn>(ColumnType) -> std::unique_ptr<GlobalAggregationFunction> {
                using OutputColumn = SumResultTrait<InputColumn>::Type;
                ColumnType out_type = SumResultTrait<InputColumn>::kOutTypeEnum;

                output_schema.AddColumn(GetOutputName(), out_type);
                return std::make_unique<
                    TypedGlobalAggregationFunction<InputColumn, OutputColumn, SumOperation>>(
                    column_ind, out_type, num_threads);
            });
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        if (column_type == ColumnType::STRING || column_type == ColumnType::CHAR ||
            column_type == ColumnType::DATE || column_type == ColumnType::TIMESTAMP) {
            THROW_RUNTIME_ERROR("SUM operation is not supported for this column type");
        }

        return AggExpHelper::DispatchNumeric(
            column_type,
            [&]<typename InputColumn>(ColumnType) -> std::unique_ptr<GroupedAggregationFunction> {
                using OutputColumn = SumResultTrait<InputColumn>::Type;
                ColumnType out_type = SumResultTrait<InputColumn>::kOutTypeEnum;

                output_schema.AddColumn(GetOutputName(), out_type);
                return std::make_unique<
                    TypedGroupedAggregationFunction<InputColumn, OutputColumn, SumOperation>>(
                    column_ind, out_type, num_threads);
            });
    }
};

class AvgExpression : public AggregateExpression {
public:
    explicit AvgExpression(std::string column_name, std::string output_name = "")
        : AggregateExpression(std::move(column_name), std::move(output_name)) {
    }
    std::string GetOutputName() const override {
        if (!output_name_.empty()) {
            return output_name_;
        }
        return "avg_" + column_name_;
    }

    std::unique_ptr<GlobalAggregationFunction> CreateGlobalAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        if (column_type == ColumnType::STRING || column_type == ColumnType::CHAR ||
            column_type == ColumnType::DATE || column_type == ColumnType::TIMESTAMP) {
            THROW_RUNTIME_ERROR("AVG operation is not supported for non-numeric columns");
        }

        return AggExpHelper::DispatchNumeric(
            column_type,
            [&]<typename InputColumn>(ColumnType) -> std::unique_ptr<GlobalAggregationFunction> {
                using StateColumn = SumResultTrait<InputColumn>::Type;

                output_schema.AddColumn(GetOutputName(), ColumnType::LONGDOUBLE);
                return std::make_unique<
                    AvgGlobalAggregationFunction<InputColumn, LongDoubleColumn, StateColumn>>(
                    column_ind, ColumnType::LONGDOUBLE, num_threads);
            });
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema, size_t num_threads) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        if (column_type == ColumnType::STRING || column_type == ColumnType::CHAR ||
            column_type == ColumnType::DATE || column_type == ColumnType::TIMESTAMP) {
            THROW_RUNTIME_ERROR("AVG operation is not supported for non-numeric columns");
        }

        return AggExpHelper::DispatchNumeric(
            column_type,
            [&]<typename InputColumn>(ColumnType) -> std::unique_ptr<GroupedAggregationFunction> {
                using StateColumn = SumResultTrait<InputColumn>::Type;

                output_schema.AddColumn(GetOutputName(), ColumnType::LONGDOUBLE);
                return std::make_unique<
                    AvgGroupedAggregationFunction<InputColumn, LongDoubleColumn, StateColumn>>(
                    column_ind, ColumnType::LONGDOUBLE, num_threads);
            });
    }
};

inline std::shared_ptr<AggregateExpression> Count(std::string column_name = "*",
                                                  std::string output_name = "count") {
    return std::make_shared<CountExpression>(std::move(column_name), std::move(output_name));
}
inline std::shared_ptr<AggregateExpression> Min(std::string column_name,
                                                std::string output_name = "") {
    return std::make_shared<MinExpression>(std::move(column_name), std::move(output_name));
}
inline std::shared_ptr<AggregateExpression> Max(std::string column_name,
                                                std::string output_name = "") {
    return std::make_shared<MaxExpression>(std::move(column_name), std::move(output_name));
}
inline std::shared_ptr<AggregateExpression> Sum(std::string column_name,
                                                std::string output_name = "") {
    return std::make_shared<SumExpression>(std::move(column_name), std::move(output_name));
}
inline std::shared_ptr<AggregateExpression> Avg(std::string column_name,
                                                std::string output_name = "") {
    return std::make_shared<AvgExpression>(std::move(column_name), std::move(output_name));
}
inline std::shared_ptr<AggregateExpression> DistinctCount(std::string column_name,
                                                          std::string output_name = "") {
    return std::make_shared<DistinctCountExpression>(std::move(column_name),
                                                     std::move(output_name));
}
