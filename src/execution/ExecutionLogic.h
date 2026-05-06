#pragma once

#include "execution/OperatorsBase.h"
#include "execution/expressions/AggregationFunctions.h"
#include "execution/expressions/AggregationExpressions.h"
#include "execution/expressions/FilterFunctions.h"
#include "execution/expressions/FilterExpressions.h"
#include "execution/expressions/ScalarExpressions.h"

#include "column/Schema.h"
#include "column/NumericColumn.h"
#include "column/StringColumn.h"
#include "column/CharColumn.h"
#include "column/TemporalColumn.h"

#include "utils/Macro.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class PlanNodeType {
    SCAN,
    AGGREGATE,
    FILTER,
    ORDER_BY,
    LIMIT,
    SCALAR
};

struct PlanNode {
    PlanNodeType type;
protected:
    explicit PlanNode(PlanNodeType node_type) : type(node_type) {}
};

struct ScanNode : public PlanNode {
    ScanNode() : PlanNode(PlanNodeType::SCAN) {}
    std::string table_path;
    std::vector<std::string> column_names;
};

struct AggregateNode : public PlanNode {
    AggregateNode() : PlanNode(PlanNodeType::AGGREGATE) {}
    std::shared_ptr<PlanNode> child;
    std::vector<std::string> group_by_columns;
    std::vector<std::shared_ptr<AggregateExpression>> aggregate_expressions;
};

struct FilterNode : public PlanNode {
    FilterNode() : PlanNode(PlanNodeType::FILTER) {}
    std::shared_ptr<PlanNode> child;
    std::shared_ptr<FilterExpression> filter_expression;
};

struct OrderByNode : public PlanNode {
    OrderByNode() : PlanNode(PlanNodeType::ORDER_BY) {}
    std::shared_ptr<PlanNode> child;
    std::vector<std::pair<std::string, bool>> order_by_columns;
    std::optional<size_t> limit;
};

struct LimitNode : public PlanNode {
    LimitNode() : PlanNode(PlanNodeType::LIMIT), limit(0) {
    }
    std::shared_ptr<PlanNode> child;
    size_t limit;
};

struct ScalarNode : public PlanNode {
    ScalarNode() : PlanNode(PlanNodeType::SCALAR) {}
    std::shared_ptr<PlanNode> child;
    std::vector<std::shared_ptr<ScalarExpression>> scalar_expressions;
};

struct PhysicalOperatorContext {
    std::unique_ptr<Operator> root_operator;
    Schema schema;
};

inline PhysicalOperatorContext BuildPhysicalPlan(
    const std::shared_ptr<PlanNode>& plan_node,
    std::optional<std::vector<std::string>> required_columns = std::nullopt) {
    switch (plan_node->type) {
        case PlanNodeType::SCAN: {
            auto scan = std::static_pointer_cast<ScanNode>(plan_node);
            std::vector<std::string> final_columns;

            Schema full_schema = Schema::FromColumnarFile(scan->table_path);
            const std::vector<Field>& full_columns = full_schema.GetFields();

            if (required_columns.has_value()) {
                final_columns = required_columns.value();
                if (final_columns.empty() && !full_columns.empty()) {
                    final_columns.push_back(full_columns[0].name);
                }
            } else {
                if (scan->column_names.empty()) {
                    for (const Field& field : full_columns) {
                        final_columns.push_back(field.name);
                    }
                } else {
                    final_columns = scan->column_names;
                }
            }

            auto op = std::make_unique<ScanOperator>(scan->table_path, final_columns);

            Schema output_schema;
            for (const std::string& name : final_columns) {
                bool found = false;
                for (const Field& field : full_columns) {
                    if (field.name == name) {
                        output_schema.AddField({field.name, field.type});
                        found = true;
                        break;
                    }
                }
                if (!found) [[unlikely]] {
                    THROW_RUNTIME_ERROR("Column " + name + " not found in table schema");
                }
            }

            return {std::move(op), std::move(output_schema)};
        }

        case PlanNodeType::AGGREGATE: {
            auto aggregate = std::static_pointer_cast<AggregateNode>(plan_node);
            if (!aggregate->group_by_columns.empty()) {
                std::vector<std::string> needed_columns = aggregate->group_by_columns;
                for (const std::shared_ptr<AggregateExpression>& expr :
                     aggregate->aggregate_expressions) {
                    expr->CollectRequiredColumns(needed_columns);
                }
                std::sort(needed_columns.begin(), needed_columns.end());
                needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                                     needed_columns.end());

                PhysicalOperatorContext child_context =
                    BuildPhysicalPlan(aggregate->child, needed_columns);

                std::vector<size_t> group_col_indices;
                std::vector<std::shared_ptr<Column>> empty_key_columns;
                Schema output_schema;

                for (const auto& col_name : aggregate->group_by_columns) {
                    size_t ind = child_context.schema.GetColumnIndexByName(col_name);
                    group_col_indices.push_back(ind);

                    ColumnType type = child_context.schema.GetColumnTypeByName(col_name);
                    output_schema.AddColumn(col_name, type);

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
        case ColumnType::ENUM_VAL: empty_key_columns.push_back(std::make_shared<CLASS_TYPE>()); break;

                    switch (type) {
                        FOR_EACH_COLUMN_TYPE(HANDLE_TYPE);
                        default: THROW_NOT_IMPLEMENTED;
                    }
#undef HANDLE_TYPE
                }

                std::vector<std::unique_ptr<GroupedAggregationFunction>> agg_funcs;
                for (const std::shared_ptr<AggregateExpression>& expr :
                     aggregate->aggregate_expressions) {
                    agg_funcs.push_back(
                        expr->CreateGroupedAggregationFunction(child_context.schema, output_schema));
                }

                auto op = std::make_unique<GroupByOperator>(
                    std::move(child_context.root_operator), std::move(group_col_indices),
                    std::move(empty_key_columns), std::move(agg_funcs));
                return {std::move(op), std::move(output_schema)};
            }

            std::vector<std::string> needed_columns = aggregate->group_by_columns;
            for (const std::shared_ptr<AggregateExpression>& expr : aggregate->aggregate_expressions) {
                expr->CollectRequiredColumns(needed_columns);
            }
            std::sort(needed_columns.begin(), needed_columns.end());
            needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                                 needed_columns.end());

            PhysicalOperatorContext child_context = BuildPhysicalPlan(aggregate->child, needed_columns);
            std::vector<std::unique_ptr<GlobalAggregationFunction>> agg_functions;
            Schema output_schema;
            for (const std::shared_ptr<AggregateExpression>& expr : aggregate->aggregate_expressions) {
                agg_functions.push_back(
                    expr->CreateGlobalAggregationFunction(child_context.schema, output_schema));
            }
            auto op = std::make_unique<AggregationOperator>(std::move(child_context.root_operator),
                                                            std::move(agg_functions));
            return {std::move(op), std::move(output_schema)};
        }

        case PlanNodeType::FILTER: {
            auto filter = std::static_pointer_cast<FilterNode>(plan_node);
            std::vector<std::string> needed_columns;
            filter->filter_expression->CollectRequiredColumns(needed_columns);
            if (required_columns.has_value()) {
                needed_columns.insert(needed_columns.end(), required_columns->begin(),
                                      required_columns->end());
            }

            std::sort(needed_columns.begin(), needed_columns.end());
            needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                                 needed_columns.end());

            PhysicalOperatorContext child_context = BuildPhysicalPlan(filter->child, needed_columns);
            std::unique_ptr<FilterFunction> filter_function =
                filter->filter_expression->CreateFilterFunction(child_context.schema);
            auto op = std::make_unique<FilterOperator>(std::move(child_context.root_operator),
                                                       std::move(filter_function));
            return {std::move(op), std::move(child_context.schema)};
        }

        case PlanNodeType::ORDER_BY: {
            auto order_by = std::static_pointer_cast<OrderByNode>(plan_node);
            std::vector<std::string> needed_columns;
            if (required_columns.has_value()) {
                needed_columns = required_columns.value();
            }
            for (const auto& [col_name, is_desc] : order_by->order_by_columns) {
                needed_columns.push_back(col_name);
            }
            std::sort(needed_columns.begin(), needed_columns.end());
            needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                                 needed_columns.end());

            PhysicalOperatorContext child_context = BuildPhysicalPlan(order_by->child, needed_columns);
            std::vector<std::pair<size_t, bool>> sort_columns;
            for (const auto& [col_name, is_desc] : order_by->order_by_columns) {
                size_t sort_col_idx = child_context.schema.GetColumnIndexByName(col_name);
                sort_columns.push_back({sort_col_idx, is_desc});
            }

            auto op =
                std::make_unique<OrderByOperator>(std::move(child_context.root_operator),
                                                  std::move(sort_columns), std::move(order_by->limit));
            return {std::move(op), std::move(child_context.schema)};
        }

        case PlanNodeType::LIMIT: {
            auto limit = std::static_pointer_cast<LimitNode>(plan_node);
            PhysicalOperatorContext child_context = BuildPhysicalPlan(limit->child, required_columns);
            auto op =
                std::make_unique<LimitOperator>(std::move(child_context.root_operator), limit->limit);
            return {std::move(op), std::move(child_context.schema)};
        }

        case PlanNodeType::SCALAR: {
            auto scalar = std::static_pointer_cast<ScalarNode>(plan_node);
            std::vector<std::string> needed_columns;
            if (required_columns.has_value()) {
                for (const auto& req_col : required_columns.value()) {
                    bool is_generated_here = false;
                    for (const auto& expr : scalar->scalar_expressions) {
                        if (expr->GetOutputName() == req_col) {
                            is_generated_here = true;
                            break;
                        }
                    }
                    if (!is_generated_here) {
                        needed_columns.push_back(req_col);
                    }
                }
            }
            for (const auto& expr : scalar->scalar_expressions) {
                expr->CollectRequiredColumns(needed_columns);
            }

            std::sort(needed_columns.begin(), needed_columns.end());
            needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                                 needed_columns.end());

            PhysicalOperatorContext child_context = BuildPhysicalPlan(scalar->child, needed_columns);

            std::vector<std::unique_ptr<ScalarFunction>> scalar_functions;

            for (const auto& expr : scalar->scalar_expressions) {
                scalar_functions.push_back(expr->CreateScalarFunction(child_context.schema));
            }

            auto op = std::make_unique<ScalarOperator>(std::move(child_context.root_operator),
                                                       std::move(scalar_functions));
            return {std::move(op), std::move(child_context.schema)};
        }

        default:
            THROW_RUNTIME_ERROR("Unknown plan node type");
    }
}
