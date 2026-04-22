//
// Created by ragnarokk on 30.03.2026.
//

#ifndef COLUMNAR_ENGINE_LOGIC_H
#define COLUMNAR_ENGINE_LOGIC_H

#include "AggregationFunctions.h"
#include "AggregationExpressions.h"
#include "FilterFunctions.h"
#include "FilterExpressions.h"
#include "OperatorsBase.h"
#include "Schema.h"
#include "macro.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct PlanNode {
    virtual ~PlanNode() = default;
};

struct ScanNode : public PlanNode {
    std::string table_path;
    std::vector<std::string> column_names;
};

struct AggregateNode : public PlanNode {
    std::shared_ptr<PlanNode> child;
    std::vector<std::string> group_by_columns;
    std::vector<std::shared_ptr<AggregateExpression>> aggregate_expressions;
};

struct FilterNode : public PlanNode {
    std::shared_ptr<PlanNode> child;
    std::shared_ptr<FilterExpression> filter_expression;
};

struct PhysicalOperatorContext {
    std::unique_ptr<Operator> root_operator;
    Schema schema;
};

inline PhysicalOperatorContext BuildPhysicalPlan(
    const std::shared_ptr<PlanNode>& plan_node,
    std::optional<std::vector<std::string>> required_columns = std::nullopt) {
    if (auto scan = std::dynamic_pointer_cast<ScanNode>(plan_node)) {
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

    if (auto aggregate = std::dynamic_pointer_cast<AggregateNode>(plan_node)) {
        if (!aggregate->group_by_columns.empty()) {
            THROW_NOT_IMPLEMENTED;
        }

        std::vector<std::string> needed_columns = aggregate->group_by_columns;
        for (const std::shared_ptr<AggregateExpression>& expr : aggregate->aggregate_expressions) {
            expr->CollectRequiredColumns(needed_columns);
        }
        std::sort(needed_columns.begin(), needed_columns.end());
        needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                             needed_columns.end());

        PhysicalOperatorContext child_context = BuildPhysicalPlan(aggregate->child, needed_columns);
        std::vector<std::unique_ptr<AggregationFunction>> agg_functions;
        Schema output_schema;
        for (const std::shared_ptr<AggregateExpression>& expr : aggregate->aggregate_expressions) {
            agg_functions.push_back(
                expr->CreateAggregationFunction(child_context.schema, output_schema));
        }
        auto op = std::make_unique<AggregationOperator>(std::move(child_context.root_operator),
                                                        std::move(agg_functions));
        return {std::move(op), std::move(output_schema)};
    }

    if (auto filter = std::dynamic_pointer_cast<FilterNode>(plan_node)) {
        std::vector<std::string> needed_columns;
        filter->filter_expression->CollectRequiredColumns(needed_columns);
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

    THROW_RUNTIME_ERROR("Unknown plan node type");
}

#endif  // COLUMNAR_ENGINE_LOGIC_H
