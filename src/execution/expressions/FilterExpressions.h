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

    virtual void CollectRequiredColumns(std::vector<std::string>& required_columns) {
        if (!column_name_.empty()) {
            required_columns.push_back(column_name_);
        }
    }

    virtual std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) = 0;

protected:
    std::string column_name_;
};

template <typename ValueType>
class NotEqFilterExpression : public FilterExpression {
public:
    NotEqFilterExpression(std::string column_name, ValueType value)
        : FilterExpression(std::move(column_name)), value_(std::move(value)) {
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

        return AggExpHelper::DispatchAll(
            column_type, [&]<typename ColumnClass>(ColumnType) -> std::unique_ptr<FilterFunction> {
                if constexpr (std::is_same_v<typename ColumnClass::ValueType, ValueType>) {
                    return std::make_unique<NotEqFilterFunction<ColumnClass, ValueType>>(ind,
                                                                                         value_);
                } else {
                    THROW_NOT_IMPLEMENTED;
                }
            });
    }

private:
    ValueType value_;
};

template <typename ValueType>
class EqFilterExpression : public FilterExpression {
public:
    EqFilterExpression(std::string column_name, ValueType value)
        : FilterExpression(std::move(column_name)), value_(std::move(value)) {
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

        return AggExpHelper::DispatchAll(
            column_type, [&]<typename ColumnClass>(ColumnType) -> std::unique_ptr<FilterFunction> {
                if constexpr (std::is_same_v<typename ColumnClass::ValueType, ValueType>) {
                    return std::make_unique<EqFilterFunction<ColumnClass, ValueType>>(ind, value_);
                } else {
                    THROW_NOT_IMPLEMENTED;
                }
            });
    }

private:
    ValueType value_;
};

template <typename ValueType>
class GreaterFilterExpression : public FilterExpression {
public:
    GreaterFilterExpression(std::string column_name, ValueType value)
        : FilterExpression(column_name), value_(std::move(value)) {
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

        return AggExpHelper::DispatchAll(
            column_type, [&]<typename ColumnClass>(ColumnType) -> std::unique_ptr<FilterFunction> {
                if constexpr (std::is_same_v<typename ColumnClass::ValueType, ValueType>) {
                    return std::make_unique<GreaterFilterFunction<ColumnClass, ValueType>>(ind,
                                                                                           value_);
                } else {
                    THROW_NOT_IMPLEMENTED;
                }
            });
    }

private:
    ValueType value_;
};

template <typename ValueType>
class GreaterEqFilterExpression : public FilterExpression {
public:
    GreaterEqFilterExpression(std::string column_name, ValueType value)
        : FilterExpression(column_name), value_(std::move(value)) {
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

        return AggExpHelper::DispatchAll(
            column_type, [&]<typename ColumnClass>(ColumnType) -> std::unique_ptr<FilterFunction> {
                if constexpr (std::is_same_v<typename ColumnClass::ValueType, ValueType>) {
                    return std::make_unique<GreaterEqFilterFunction<ColumnClass, ValueType>>(
                        ind, value_);
                } else {
                    THROW_NOT_IMPLEMENTED;
                }
            });
    }

private:
    ValueType value_;
};

template <typename ValueType>
class LessEqFilterExpression : public FilterExpression {
public:
    LessEqFilterExpression(std::string column_name, ValueType value)
        : FilterExpression(column_name), value_(std::move(value)) {
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

        return AggExpHelper::DispatchAll(
            column_type, [&]<typename ColumnClass>(ColumnType) -> std::unique_ptr<FilterFunction> {
                if constexpr (std::is_same_v<typename ColumnClass::ValueType, ValueType>) {
                    return std::make_unique<LessEqFilterFunction<ColumnClass, ValueType>>(ind,
                                                                                          value_);
                } else {
                    THROW_NOT_IMPLEMENTED;
                }
            });
    }

private:
    ValueType value_;
};

class LikeExpression : public FilterExpression {
public:
    LikeExpression(std::string column_name, std::string value)
        : FilterExpression(column_name), value_(std::move(value)) {
        value_.erase(value_.begin());
        value_.pop_back();
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename ColumnClass = StringColumn>(
                ColumnType) -> std::unique_ptr<FilterFunction> {
                if constexpr (std::is_same_v<typename ColumnClass::ValueType, std::string>) {
                    return std::make_unique<LikeFilterFunction<ColumnClass, std::string>>(ind,
                                                                                          value_);
                } else {
                    THROW_NOT_IMPLEMENTED;
                }
            });
    }

private:
    std::string value_;
};

class NotLikeExpression : public FilterExpression {
public:
    NotLikeExpression(std::string column_name, std::string value)
        : FilterExpression(column_name), value_(std::move(value)) {
        value_.erase(value_.begin());
        value_.pop_back();
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        size_t ind = child_schema.GetColumnIndexByName(column_name_);
        ColumnType column_type = child_schema.GetColumnTypeByName(column_name_);

        return AggExpHelper::DispatchAll(
            column_type,
            [&]<typename ColumnClass = StringColumn>(
                ColumnType) -> std::unique_ptr<FilterFunction> {
                if constexpr (std::is_same_v<typename ColumnClass::ValueType, std::string>) {
                    return std::make_unique<NotLikeFilterFunction<ColumnClass, std::string>>(
                        ind, value_);
                } else {
                    THROW_NOT_IMPLEMENTED;
                }
            });
    }

private:
    std::string value_;
};

class AndExpression : public FilterExpression {
public:
    AndExpression(std::shared_ptr<FilterExpression> left, std::shared_ptr<FilterExpression> right)
        : FilterExpression(""), left_(std::move(left)), right_(std::move(right)) {
    }

    void CollectRequiredColumns(std::vector<std::string>& required_columns) override {
        left_->CollectRequiredColumns(required_columns);
        right_->CollectRequiredColumns(required_columns);
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        return std::make_unique<AndFilterFunction>(left_->CreateFilterFunction(child_schema),
                                                   right_->CreateFilterFunction(child_schema));
    }

private:
    std::shared_ptr<FilterExpression> left_;
    std::shared_ptr<FilterExpression> right_;
};

class OrExpression : public FilterExpression {
public:
    OrExpression(std::shared_ptr<FilterExpression> left, std::shared_ptr<FilterExpression> right)
        : FilterExpression(""), left_(std::move(left)), right_(std::move(right)) {
    }

    void CollectRequiredColumns(std::vector<std::string>& required_columns) override {
        left_->CollectRequiredColumns(required_columns);
        right_->CollectRequiredColumns(required_columns);
    }

    std::unique_ptr<FilterFunction> CreateFilterFunction(const Schema& child_schema) override {
        return std::make_unique<OrFilterFunction>(left_->CreateFilterFunction(child_schema),
                                                   right_->CreateFilterFunction(child_schema));
    }

private:
    std::shared_ptr<FilterExpression> left_;
    std::shared_ptr<FilterExpression> right_;
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

template <typename T>
inline std::shared_ptr<FilterExpression> Greater(const std::string& column_name, T target_val) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::make_shared<GreaterFilterExpression<std::string>>(column_name,
                                                                      std::string(target_val));
    } else {
        return std::make_shared<GreaterFilterExpression<T>>(column_name, std::move(target_val));
    }
}

template <typename T>
inline std::shared_ptr<FilterExpression> GreaterEq(const std::string& column_name, T target_val) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::make_shared<GreaterEqFilterExpression<std::string>>(column_name,
                                                                        std::string(target_val));
    } else {
        return std::make_shared<GreaterEqFilterExpression<T>>(column_name, std::move(target_val));
    }
}

template <typename T>
inline std::shared_ptr<FilterExpression> LessEq(const std::string& column_name, T target_val) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::make_shared<LessEqFilterExpression<std::string>>(column_name,
                                                                     std::string(target_val));
    } else {
        return std::make_shared<LessEqFilterExpression<T>>(column_name, std::move(target_val));
    }
}

template <typename T>
inline std::shared_ptr<FilterExpression> Like(const std::string& column_name, T target_val) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::make_shared<LikeExpression>(column_name, std::string(target_val));
    } else {
        THROW_RUNTIME_ERROR("Like pattern must be a string");
    }
}

template <typename T>
inline std::shared_ptr<FilterExpression> NotLike(const std::string& column_name, T target_val) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::make_shared<NotLikeExpression>(column_name, std::string(target_val));
    } else {
        THROW_RUNTIME_ERROR("Like pattern must be a string");
    }
}

inline std::shared_ptr<FilterExpression> And(std::shared_ptr<FilterExpression> left, std::shared_ptr<FilterExpression> right) {
    return std::make_shared<AndExpression>(std::move(left), std::move(right));
}

inline std::shared_ptr<FilterExpression> Or(std::shared_ptr<FilterExpression> left, std::shared_ptr<FilterExpression> right) {
    return std::make_shared<OrExpression>(std::move(left), std::move(right));
}
