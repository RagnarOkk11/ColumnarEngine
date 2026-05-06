#pragma once

#include "execution/expressions/AggregationFunctions.h"
#include "execution/expressions/AggExpHelper.h"
#include "column/Schema.h"

#include <memory>
#include <string>
#include <utility>

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
        const Schema& child_schema, Schema& output_schema) = 0;

    virtual std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema) = 0;

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
        [[maybe_unused]] const Schema& child_schema, Schema& output_schema) override {
        output_schema.AddColumn(GetOutputName(), ColumnType::INT32);
        return std::make_unique<CountGlobalAggregationFunction>();
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        [[maybe_unused]] const Schema& child_schema, Schema& output_schema) override {
        output_schema.AddColumn(GetOutputName(), ColumnType::INT32);
        return std::make_unique<CountGroupedAggregationFunction>();
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
        const Schema& child_schema, Schema& output_schema) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type, [&]<typename T>(ColumnType) -> std::unique_ptr<GlobalAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
                return std::make_unique<DistinctCountGlobalAggregationFunction<T>>(column_ind);
            });
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType) -> std::unique_ptr<GroupedAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
                return std::make_unique<DistinctCountGroupedAggregationFunction<T>>(column_ind);
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
        const Schema& child_schema, Schema& output_schema) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType t) -> std::unique_ptr<GlobalAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), t);
                return std::make_unique<TypedGlobalAggregationFunction<T, MinOperation>>(
                    column_ind);
            });
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType t) -> std::unique_ptr<GroupedAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), t);
                return std::make_unique<TypedGroupedAggregationFunction<T, MinOperation>>(
                    column_ind);
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
        const Schema& child_schema, Schema& output_schema) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType t) -> std::unique_ptr<GlobalAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), t);
                return std::make_unique<TypedGlobalAggregationFunction<T, MaxOperation>>(
                    column_ind);
            });
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename T>(ColumnType t) -> std::unique_ptr<GroupedAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), t);
                return std::make_unique<TypedGroupedAggregationFunction<T, MaxOperation>>(
                    column_ind);
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
        const Schema& child_schema, Schema& output_schema) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        switch (column_type) {
            case ColumnType::INT16:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT32);
                return std::make_unique<TypedGlobalAggregationFunction<Int16Column, SumOperation>>(
                    column_ind);
            case ColumnType::INT32:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
                return std::make_unique<TypedGlobalAggregationFunction<Int32Column, SumOperation>>(
                    column_ind);
            case ColumnType::INT64:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT128);
                return std::make_unique<TypedGlobalAggregationFunction<Int64Column, SumOperation>>(
                    column_ind);
            case ColumnType::INT128:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT128);
                return std::make_unique<TypedGlobalAggregationFunction<Int128Column, SumOperation>>(
                    column_ind);
            case ColumnType::FLOAT:
                output_schema.AddColumn(GetOutputName(), ColumnType::DOUBLE);
                return std::make_unique<TypedGlobalAggregationFunction<FloatColumn, SumOperation>>(
                    column_ind);
            case ColumnType::DOUBLE:
                output_schema.AddColumn(GetOutputName(), ColumnType::LONGDOUBLE);
                return std::make_unique<TypedGlobalAggregationFunction<DoubleColumn, SumOperation>>(
                    column_ind);
            case ColumnType::LONGDOUBLE:
                output_schema.AddColumn(GetOutputName(), ColumnType::LONGDOUBLE);
                return std::make_unique<
                    TypedGlobalAggregationFunction<LongDoubleColumn, SumOperation>>(column_ind);
            default: THROW_NOT_IMPLEMENTED;
        }
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        switch (column_type) {
            case ColumnType::INT16:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT32);
                return std::make_unique<TypedGroupedAggregationFunction<Int16Column, SumOperation>>(
                    column_ind);
            case ColumnType::INT32:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
                return std::make_unique<TypedGroupedAggregationFunction<Int32Column, SumOperation>>(
                    column_ind);
            case ColumnType::INT64:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT128);
                return std::make_unique<TypedGroupedAggregationFunction<Int64Column, SumOperation>>(
                    column_ind);
            case ColumnType::INT128:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT128);
                return std::make_unique<
                    TypedGroupedAggregationFunction<Int128Column, SumOperation>>(column_ind);
            case ColumnType::FLOAT:
                output_schema.AddColumn(GetOutputName(), ColumnType::DOUBLE);
                return std::make_unique<TypedGroupedAggregationFunction<FloatColumn, SumOperation>>(
                    column_ind);
            case ColumnType::DOUBLE:
                output_schema.AddColumn(GetOutputName(), ColumnType::LONGDOUBLE);
                return std::make_unique<
                    TypedGroupedAggregationFunction<DoubleColumn, SumOperation>>(column_ind);
            case ColumnType::LONGDOUBLE:
                output_schema.AddColumn(GetOutputName(), ColumnType::LONGDOUBLE);
                return std::make_unique<
                    TypedGroupedAggregationFunction<LongDoubleColumn, SumOperation>>(column_ind);
            default: THROW_NOT_IMPLEMENTED;
        }
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
        const Schema& child_schema, Schema& output_schema) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchNumeric(
            column_type, [&]<typename T>(ColumnType) -> std::unique_ptr<GlobalAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), ColumnType::DOUBLE);
                return std::make_unique<AvgGlobalAggregationFunction<T>>(column_ind);
            });
    }

    std::unique_ptr<GroupedAggregationFunction> CreateGroupedAggregationFunction(
        const Schema& child_schema, Schema& output_schema) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        return AggExpHelper::DispatchNumeric(
            column_type,
            [&]<typename T>(ColumnType) -> std::unique_ptr<GroupedAggregationFunction> {
                output_schema.AddColumn(GetOutputName(), ColumnType::DOUBLE);
                return std::make_unique<AvgGroupedAggregationFunction<T>>(column_ind);
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
