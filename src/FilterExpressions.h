//
// Created by ragnarokk on 21.04.2026.
//

#ifndef COLUMNAR_ENGINE_FILTEREXPRESSIONS_H
#define COLUMNAR_ENGINE_FILTEREXPRESSIONS_H

#include "FilterFunctions.h"
#include "macro.h"
#include "Schema.h"

#include <string>
#include <vector>

class FilterExpression {
public:
    virtual ~FilterExpression() = default;

    explicit FilterExpression(std::string column_name) : column_name_(std::move(column_name)) {
    }

    void CollectRequiredColumns(std::vector<std::string>& required_columns) {
        required_columns.push_back(column_name_);
    }

    virtual std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) = 0;

protected:
    std::string column_name_;
};

template <typename ValueType>
class NotEqFilterExpression : public FilterExpression {
public:
    NotEqFilterExpression(std::string column_name, ValueType value)
        : FilterExpression(column_name), value_(std::move(value)) {
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

        #define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
            case ColumnType::ENUM_VAL:                     \
                return std::make_unique<NotEqFilterFunction<CLASS_TYPE, ValueType>>(ind, value_);

                switch (column_type) {
                    FOR_EACH_COLUMN_TYPE(HANDLE_TYPE)
                    default:
                        THROW_NOT_IMPLEMENTED;
                }
        #undef HANDLE_TYPE
    }

private:
    ValueType value_;
};

template <typename ValueType>
inline std::shared_ptr<FilterExpression> NotEq(const std::string& column_name,
                                               ValueType target_val) {
    return std::make_shared<NotEqFilterExpression<ValueType>>(column_name, target_val);
}

#endif  // COLUMNAR_ENGINE_FILTEREXPRESSIONS_H
