//
// Created by ragnarokk on 31.03.2026.
//

#ifndef COLUMNAR_ENGINE_EXPRESSIONS_H
#define COLUMNAR_ENGINE_EXPRESSIONS_H

#include "AggregationFunctions.h"
#include "Schema.h"

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

    virtual std::unique_ptr<AggregationFunction> CreateAggregationFunction(
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

    std::unique_ptr<AggregationFunction> CreateAggregationFunction(
        [[maybe_unused]] const Schema& child_schema, Schema& output_schema) override {
        output_schema.AddColumn(GetOutputName(), ColumnType::INT32);
        return std::make_unique<CountAggregationFunction>();
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

    std::unique_ptr<AggregationFunction> CreateAggregationFunction(const Schema& child_schema,
                                                                   Schema& output_schema) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());
        output_schema.AddColumn(GetOutputName(), ColumnType::INT64);

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
    case ColumnType::ENUM_VAL:                     \
        return std::make_unique<DistinctCountAggregationFunction<CLASS_TYPE>>(column_ind);

        switch (column_type) {
            FOR_EACH_COLUMN_TYPE(HANDLE_TYPE);
            default:
                THROW_NOT_IMPLEMENTED;
        }
#undef HANDLE_TYPE
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

    std::unique_ptr<AggregationFunction> CreateAggregationFunction(const Schema& child_schema,
                                                                   Schema& output_schema) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                      \
    case ColumnType::ENUM_VAL:                                          \
        output_schema.AddColumn(GetOutputName(), ColumnType::ENUM_VAL); \
        return std::make_unique<MinAggregationFunction<CLASS_TYPE>>(column_ind);
        // END_HANDLE_TYPE

        switch (column_type) {
            FOR_NUMERIC_COLUMN_TYPE(HANDLE_TYPE);
            default:
                THROW_NOT_IMPLEMENTED;
        }

#undef HANDLE_TYPE
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

    std::unique_ptr<AggregationFunction> CreateAggregationFunction(const Schema& child_schema,
                                                                   Schema& output_schema) override {
        size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                      \
    case ColumnType::ENUM_VAL:                                          \
        output_schema.AddColumn(GetOutputName(), ColumnType::ENUM_VAL); \
        return std::make_unique<MaxAggregationFunction<CLASS_TYPE>>(column_ind);
        // END_HANDLE_TYPE

        switch (column_type) {
            FOR_NUMERIC_COLUMN_TYPE(HANDLE_TYPE);
            default:
                THROW_NOT_IMPLEMENTED;
        }

#undef HANDLE_TYPE
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

    std::unique_ptr<AggregationFunction> CreateAggregationFunction(const Schema& child_schema,
                                                                   Schema& output_schema) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());

        switch (column_type) {
            case ColumnType::INT16:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT32);
                return std::make_unique<SumAggregationFunction<Int16Column>>(column_ind);
            case ColumnType::INT32:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
                return std::make_unique<SumAggregationFunction<Int32Column>>(column_ind);
            case ColumnType::INT64:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT128);
                return std::make_unique<SumAggregationFunction<Int64Column>>(column_ind);
            case ColumnType::INT128:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT128);
                return std::make_unique<SumAggregationFunction<Int128Column>>(column_ind);
            case ColumnType::FLOAT:
                output_schema.AddColumn(GetOutputName(), ColumnType::DOUBLE);
                return std::make_unique<SumAggregationFunction<FloatColumn>>(column_ind);
            case ColumnType::DOUBLE:
                output_schema.AddColumn(GetOutputName(), ColumnType::LONGDOUBLE);
                return std::make_unique<SumAggregationFunction<DoubleColumn>>(column_ind);
            case ColumnType::LONGDOUBLE:
                output_schema.AddColumn(GetOutputName(), ColumnType::LONGDOUBLE);
                return std::make_unique<SumAggregationFunction<LongDoubleColumn>>(column_ind);
            default:
                THROW_NOT_IMPLEMENTED;
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

    std::unique_ptr<AggregationFunction> CreateAggregationFunction(const Schema& child_schema,
                                                                   Schema& output_schema) override {
        const size_t column_ind = child_schema.GetColumnIndexByName(GetName());
        const ColumnType column_type = child_schema.GetColumnTypeByName(GetName());
        output_schema.AddColumn(GetOutputName(), ColumnType::LONGDOUBLE);

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
    case ColumnType::ENUM_VAL:                     \
        return std::make_unique<AvgAggregationFunction<CLASS_TYPE>>(column_ind);
        // END_HANDLE_TYPE

        switch (column_type) {
            FOR_NUMERIC_COLUMN_TYPE(HANDLE_TYPE);
            default:
                THROW_NOT_IMPLEMENTED;
        }
    }

#undef HANDLE_TYPE
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

#endif  // COLUMNAR_ENGINE_EXPRESSIONS_H
