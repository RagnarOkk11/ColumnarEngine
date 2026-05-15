#pragma once

#include "execution/OperatorsBase.h"
#include "execution/expressions/AggregationExpressions.h"
#include "execution/expressions/FilterExpressions.h"
#include "execution/expressions/ScalarExpressions.h"

#include "column/Schema.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct PhysicalOperatorContext {
    std::unique_ptr<Operator> root_operator;
    Schema schema;
};

struct PlanNode {
    virtual ~PlanNode() = default;

    virtual PhysicalOperatorContext BuildPhysicalPlan(
        std::optional<std::vector<std::string>> required_columns = std::nullopt) const = 0;
};

struct ScanNode : public PlanNode {
    std::string table_path;
    std::vector<std::string> column_names;
    std::shared_ptr<const MetadataTable> table_meta;

    ScanNode(std::string table_p, std::vector<std::string> column_n,
             std::shared_ptr<const MetadataTable> table_m)
        : table_path(std::move(table_p)),
          column_names(std::move(column_n)),
          table_meta(std::move(table_m)) {
    }

    PhysicalOperatorContext BuildPhysicalPlan(
        std::optional<std::vector<std::string>> required_columns) const override;
};

struct AggregateNode : public PlanNode {
    std::shared_ptr<PlanNode> child;
    std::vector<std::string> group_by_columns;
    std::vector<std::shared_ptr<AggregateExpression>> aggregate_expressions;

    AggregateNode(std::shared_ptr<PlanNode> c, std::vector<std::string> gb_cols,
                  std::vector<std::shared_ptr<AggregateExpression>> agg_exprs)
        : child(std::move(c)),
          group_by_columns(std::move(gb_cols)),
          aggregate_expressions(std::move(agg_exprs)) {
    }

    PhysicalOperatorContext BuildPhysicalPlan(
        std::optional<std::vector<std::string>> required_columns) const override;
};

struct FilterNode : public PlanNode {
    std::shared_ptr<PlanNode> child;
    std::shared_ptr<FilterExpression> filter_expression;

    FilterNode(std::shared_ptr<PlanNode> c, std::shared_ptr<FilterExpression> filter_expr)
        : child(std::move(c)), filter_expression(std::move(filter_expr)) {
    }

    PhysicalOperatorContext BuildPhysicalPlan(
        std::optional<std::vector<std::string>> required_columns) const override;
};

struct OrderByNode : public PlanNode {
    std::shared_ptr<PlanNode> child;
    std::vector<std::pair<std::string, bool>> order_by_columns;
    std::optional<size_t> limit;
    std::optional<size_t> offset;

    OrderByNode(std::shared_ptr<PlanNode> c,
                std::vector<std::pair<std::string, bool>> order_by_cols, std::optional<size_t> lim,
                std::optional<size_t> off)
        : child(std::move(c)), order_by_columns(std::move(order_by_cols)), limit(lim), offset(off) {
    }

    PhysicalOperatorContext BuildPhysicalPlan(
        std::optional<std::vector<std::string>> required_columns) const override;
};

struct LimitNode : public PlanNode {
    std::shared_ptr<PlanNode> child;
    size_t limit;

    LimitNode(std::shared_ptr<PlanNode> c, size_t lim) : child(std::move(c)), limit(lim) {
    }

    PhysicalOperatorContext BuildPhysicalPlan(
        std::optional<std::vector<std::string>> required_columns) const override;
};

struct ScalarNode : public PlanNode {
    std::shared_ptr<PlanNode> child;
    std::vector<std::shared_ptr<ScalarExpression>> scalar_expressions;

    ScalarNode(std::shared_ptr<PlanNode> c,
               std::vector<std::shared_ptr<ScalarExpression>> scalar_exprs)
        : child(std::move(c)), scalar_expressions(std::move(scalar_exprs)) {
    }

    PhysicalOperatorContext BuildPhysicalPlan(
        std::optional<std::vector<std::string>> required_columns) const override;
};

struct DropNode : public PlanNode {
    std::shared_ptr<PlanNode> child;
    std::vector<std::string> columns_to_drop;

    DropNode(std::shared_ptr<PlanNode> c, std::vector<std::string> cols_to_drop)
        : child(std::move(c)), columns_to_drop(std::move(cols_to_drop)) {
    }

    PhysicalOperatorContext BuildPhysicalPlan(
        std::optional<std::vector<std::string>> required_columns) const override;
};

struct ReorderNode : public PlanNode {
    std::shared_ptr<PlanNode> child;
    std::vector<std::string> desired_order;

    ReorderNode(std::shared_ptr<PlanNode> c, std::vector<std::string> desired_ord)
        : child(std::move(c)), desired_order(std::move(desired_ord)) {
    }

    PhysicalOperatorContext BuildPhysicalPlan(
        std::optional<std::vector<std::string>> required_columns) const override;
};
