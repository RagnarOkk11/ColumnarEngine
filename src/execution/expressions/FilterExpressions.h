#pragma once

#include "FilterFunctions.h"
#include "utils/Macro.h"
#include "column/Schema.h"

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

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                                            \
    case ColumnType::ENUM_VAL:                                                                \
        if constexpr (std::is_same_v<typename CLASS_TYPE::ValueType, ValueType>) {            \
            return std::make_unique<NotEqFilterFunction<CLASS_TYPE, ValueType>>(ind, value_); \
        } else {                                                                              \
            THROW_NOT_IMPLEMENTED;                                                            \
        }

        switch (column_type) {
            FOR_EACH_COLUMN_TYPE(HANDLE_TYPE);
            default: THROW_NOT_IMPLEMENTED;
        }

#undef HANDLE_TYPE
    }

private:
    ValueType value_;
};

template <typename ValueType>
class EqFilterExpression : public FilterExpression {
public:
    EqFilterExpression(std::string column_name, ValueType value)
        : FilterExpression(column_name), value_(std::move(value)) {
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                                         \
    case ColumnType::ENUM_VAL:                                                             \
        if constexpr (std::is_same_v<typename CLASS_TYPE::ValueType, ValueType>) {         \
            return std::make_unique<EqFilterFunction<CLASS_TYPE, ValueType>>(ind, value_); \
        } else {                                                                           \
            THROW_NOT_IMPLEMENTED;                                                         \
        }

        switch (column_type) {
            FOR_EACH_COLUMN_TYPE(HANDLE_TYPE);
            default: THROW_NOT_IMPLEMENTED;
        }

#undef HANDLE_TYPE
    }

private:
    ValueType value_;
};

template <typename T>
inline std::shared_ptr<FilterExpression> NotEq(const std::string& column_name, T target_val) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::make_shared<NotEqFilterExpression<std::string>>(column_name,
                                                                    std::string(target_val));
    } else {
        return std::make_shared<NotEqFilterExpression<T>>(column_name, std::move(target_val));
    }
}

template <typename T>
inline std::shared_ptr<FilterExpression> Eq(const std::string& column_name, T target_val) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::make_shared<EqFilterExpression<std::string>>(column_name,
                                                                 std::string(target_val));
    } else {
        return std::make_shared<EqFilterExpression<T>>(column_name, std::move(target_val));
    }
}
