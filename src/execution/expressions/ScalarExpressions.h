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

    void CollectRequiredColumns(std::vector<std::string>& required_columns) {
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

inline std::shared_ptr<ScalarExpression> ExtractMinute(std::string column_name,
                                                       std::string output_name = "") {
    return std::make_shared<ExtractMinuteExpression>(std::move(column_name),
                                                     std::move(output_name));
}
