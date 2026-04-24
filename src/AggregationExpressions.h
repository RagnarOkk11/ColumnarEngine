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

template <template <typename> class AggregatorT>
std::unique_ptr<AggregationFunction> CreateNumericAggregator(ColumnType type, size_t col_index) {
    switch (type) {
        case ColumnType::INT16:
            return std::make_unique<AggregatorT<Int16Column>>(col_index);
        case ColumnType::INT32:
            return std::make_unique<AggregatorT<Int32Column>>(col_index);
        case ColumnType::INT64:
            return std::make_unique<AggregatorT<Int64Column>>(col_index);
        case ColumnType::INT128:
            return std::make_unique<AggregatorT<Int128Column>>(col_index);
        case ColumnType::FLOAT:
            return std::make_unique<AggregatorT<FloatColumn>>(col_index);
        case ColumnType::DOUBLE:
            return std::make_unique<AggregatorT<DoubleColumn>>(col_index);
        case ColumnType::LONGDOUBLE:
            return std::make_unique<AggregatorT<LongDoubleColumn>>(col_index);
        default:
            THROW_NOT_IMPLEMENTED;
    }
}

template <template <typename> class AggregatorT>
std::unique_ptr<AggregationFunction> CreateAnyAggregator(ColumnType type, size_t col_index) {
    #define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
        case ColumnType::ENUM_VAL:                     \
            return std::make_unique<AggregatorT<CLASS_TYPE>>(col_index);

    switch (type) {
        FOR_EACH_COLUMN_TYPE(HANDLE_TYPE)
        default:
            THROW_NOT_IMPLEMENTED;
    }
    #undef HANDLE_TYPE
}

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
        return CreateAnyAggregator<DistinctCountAggregationFunction>(column_type, column_ind);
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
                break;
            case ColumnType::INT32:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT64);
                break;
            case ColumnType::INT64:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT128);
                break;
            case ColumnType::INT128:
                output_schema.AddColumn(GetOutputName(), ColumnType::INT128);
                break;
            case ColumnType::FLOAT:
                output_schema.AddColumn(GetOutputName(), ColumnType::DOUBLE);
                break;
            case ColumnType::DOUBLE:
                output_schema.AddColumn(GetOutputName(), ColumnType::DOUBLE);
                break;
            default:
                THROW_NOT_IMPLEMENTED;
        }
        return CreateNumericAggregator<SumAggregationFunction>(column_type, column_ind);
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
        output_schema.AddColumn(GetOutputName(), ColumnType::DOUBLE);
        return CreateNumericAggregator<AvgAggregationFunction>(column_type, column_ind);
    }
};

inline std::shared_ptr<AggregateExpression> Count(std::string column_name = "*",
                                                  std::string output_name = "count") {
    return std::make_shared<CountExpression>(std::move(column_name), std::move(output_name));
}
inline std::shared_ptr<AggregateExpression> Sum(std::string column_name,
                                                std::string output_name = "") {
    return std::make_shared<SumExpression>(std::move(column_name), std::move(output_name));
}
inline std::shared_ptr<AggregateExpression> Avg(std::string column_name,
                                                std::string output_name = "") {
    return std::make_shared<AvgExpression>(std::move(column_name), std::move(output_name));
}
inline std::shared_ptr<AggregateExpression> DistinctCount(std::string column_name, std::string output_name = "") {
    return std::make_shared<DistinctCountExpression>(std::move(column_name), std::move(output_name));
}

#endif  // COLUMNAR_ENGINE_EXPRESSIONS_H
