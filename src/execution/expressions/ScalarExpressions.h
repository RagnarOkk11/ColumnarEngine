#pragma once

#include "execution/expressions/ScalarFunctions.h"
#include "column/Schema.h"

#include <memory>
#include <string>
#include <vector>

class ScalarExpression {
public:
    virtual ~ScalarExpression() = default;

    explicit ScalarExpression(std::string column_name, std::string output_name = "")
        : column_name_(std::move(column_name)), output_name_(std::move(output_name)) {
    }

    virtual void CollectRequiredColumns(std::vector<std::string>& required_columns) {
        required_columns.push_back(column_name_);
    }

    virtual std::string GetOutputName() const {
        return output_name_.empty() ? column_name_ : output_name_;
    }

    virtual std::unique_ptr<ScalarFunction> CreateScalarFunction(Schema& child_schema) = 0;

protected:
    std::string column_name_;
    std::string output_name_;
};

class ExtractMinuteExpression : public ScalarExpression {
public:
    explicit ExtractMinuteExpression(std::string column_name, std::string output_name = "")
        : ScalarExpression(std::move(column_name), std::move(output_name)) {
    }

    std::string GetOutputName() const override {
        return output_name_.empty() ? "minutes_" + column_name_ : output_name_;
    }

    std::unique_ptr<ScalarFunction> CreateScalarFunction(Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType type = child_schema.GetColumnTypeByName(column_name_);

        if (type != ColumnType::TIMESTAMP) [[unlikely]] {
            THROW_RUNTIME_ERROR("ExtractMinute requires TIMESTAMP column");
        }

        child_schema.AddColumn(GetOutputName(), ColumnType::INT32);
        return std::make_unique<ExtractMinuteFunction>(ind);
    }
};

class LengthExpression : public ScalarExpression {
public:
    explicit LengthExpression(std::string column_name, std::string output_name = "")
        : ScalarExpression(std::move(column_name), std::move(output_name)) {
    }

    std::string GetOutputName() const override {
        return output_name_.empty() ? "length_" + column_name_ : output_name_;
    }

    std::unique_ptr<ScalarFunction> CreateScalarFunction(Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType type = child_schema.GetColumnTypeByName(column_name_);

        if (type != ColumnType::STRING) [[unlikely]] {
            THROW_RUNTIME_ERROR("Length requires STRING column");
        }

        child_schema.AddColumn(GetOutputName(), ColumnType::INT64);
        return std::make_unique<LengthFunction>(ind);
    }
};

template <typename T>
class ConstantExpression : public ScalarExpression {
public:
    ConstantExpression(std::string output_name, T constant)
        : ScalarExpression("", std::move(output_name)), constant_(std::move(constant)) {
    }

    void CollectRequiredColumns(std::vector<std::string>&) override {
        // yes charonchik bebe
    }

    std::unique_ptr<ScalarFunction> CreateScalarFunction(Schema& child_schema) override {
        if constexpr (std::is_constructible_v<std::string_view, T>) {
            child_schema.AddColumn(GetOutputName(), ColumnType::STRING);
            return std::make_unique<ConstantFunction<StringColumn>>(ColumnType::STRING,
                                                                    std::string(constant_));
        } else {
            ColumnType type = ColumnTypeTraits<T>::kType;
            child_schema.AddColumn(GetOutputName(), type);
            return std::make_unique<ConstantFunction<NumericColumn<T>>>(type, constant_);
        }
    }

private:
    T constant_;
};

template <typename T>
class AddConstantExpression : public ScalarExpression {
public:
    AddConstantExpression(std::string column_name, T constant, std::string output_name = "")
        : ScalarExpression(std::move(column_name), std::move(output_name)),
          constant_(std::move(constant)) {
    }

    std::string GetOutputName() const override {
        if (!output_name_.empty()) {
            return output_name_;
        }
        return column_name_ + "_plus_" + std::to_string(constant_);
    }

    std::unique_ptr<ScalarFunction> CreateScalarFunction(Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

        if (column_type == ColumnType::STRING || column_type == ColumnType::CHAR ||
            column_type == ColumnType::DATE || column_type == ColumnType::TIMESTAMP) {
            THROW_RUNTIME_ERROR("AddConst is not supported for non-numeric columns");
            }

        return AggExpHelper::DispatchNumeric(
            column_type,
            [&]<typename ColumnClass>(ColumnType) -> std::unique_ptr<ScalarFunction> {
                child_schema.AddColumn(GetOutputName(), column_type);
                return std::make_unique<AddConstantFunction<ColumnClass>>(ind, constant_);
            });
    }

private:
    T constant_;
};

inline std::shared_ptr<ScalarExpression> ExtractMinute(std::string column_name,
                                                       std::string output_name = "") {
    return std::make_shared<ExtractMinuteExpression>(std::move(column_name),
                                                     std::move(output_name));
}

inline std::shared_ptr<ScalarExpression> Length(std::string column_name,
                                                std::string output_name = "") {
    return std::make_shared<LengthExpression>(std::move(column_name), std::move(output_name));
}

template <typename T>
inline std::shared_ptr<ScalarExpression> Literal(std::string output_name, T value) {
    return std::make_shared<ConstantExpression<T>>(std::move(output_name), std::move(value));
}

template <typename T>
inline std::shared_ptr<ScalarExpression> AddConst(std::string column_name, T value, std::string output_name = "") {
    return std::make_shared<AddConstantExpression<T>>(std::move(column_name), std::move(value), std::move(output_name));
}
