//
// Created by ragnarokk on 31.03.2026.
//

#ifndef COLUMNAR_ENGINE_EXPRESSIONS_H
#define COLUMNAR_ENGINE_EXPRESSIONS_H

#include <memory>
#include <string>
#include <utility>

class Expression {
public:
    virtual ~Expression() = default;

    virtual std::string GetName() const = 0;
    virtual std::string GetOutputName() const = 0;
};

class AggregateExpression : public Expression {
public:
    explicit AggregateExpression(std::string column_name, std::string output_name = "")
        : column_name_(std::move(column_name)), output_name_(std::move(output_name)) {
    }

    std::string GetName() const override {
        return column_name_;
    }

    std::string GetOutputName() const override {
        return output_name_.empty() ? column_name_ : output_name_;
    }

protected:
    std::string column_name_;
    std::string output_name_;
};

class CountExpression : public AggregateExpression {
public:
    explicit CountExpression(std::string column_name, std::string output_name = "count")
        : AggregateExpression(std::move(column_name), std::move(output_name)) {
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
};

inline std::shared_ptr<Expression> Count(std::string column_name = "*",
                                         std::string output_name = "count") {
    return std::make_shared<CountExpression>(std::move(column_name), std::move(output_name));
}

inline std::shared_ptr<Expression> Sum(std::string column_name, std::string output_name = "") {
    return std::make_shared<SumExpression>(std::move(column_name), std::move(output_name));
}

#endif  // COLUMNAR_ENGINE_EXPRESSIONS_H
