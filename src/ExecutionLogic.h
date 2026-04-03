//
// Created by ragnarokk on 30.03.2026.
//

#ifndef COLUMNAR_ENGINE_LOGIC_H
#define COLUMNAR_ENGINE_LOGIC_H

#include <memory>
#include <string>
#include <vector>

#include "AggregationFunctions.h"
#include "Expressions.h"
#include "OperatorsBase.h"
#include "Schema.h"
#include "macro.h"

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
    std::vector<std::shared_ptr<Expression>> aggregate_expressions;
};

struct PhysicalOperatorContext {
    std::unique_ptr<Operator> root_operator;
    Schema schema;
};

inline PhysicalOperatorContext BuildPhysicalPlan(const std::shared_ptr<PlanNode>& plan_node) {
    if (auto scan = std::dynamic_pointer_cast<ScanNode>(plan_node)) {
        auto op = std::make_unique<ScanOperator>(scan->table_path, scan->column_names);

        Schema full_schema = Schema::FromColumnarFile(scan->table_path);
        const std::vector<Field>& full_columns = full_schema.GetFields();

        Schema output_schema;
        if (scan->column_names.empty()) {
            for (const Field& field : full_columns) {
                output_schema.AddField({field.name, field.type});
            }
        } else {
            for (const std::string& name : scan->column_names) {
                bool found = false;
                for (const Field& field : full_columns) { // TODO: optimize maybe
                    if (field.name == name) {
                        output_schema.AddField({field.name, field.type});
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    THROW_RUNTIME_ERROR("Column " + name + " not found in table schema");
                }
            }
        }

        return {std::move(op), std::move(output_schema)};
    }

    if (auto aggregate = std::dynamic_pointer_cast<AggregateNode>(plan_node)) {
        if (!aggregate->group_by_columns.empty()) {
            THROW_NOT_IMPLEMENTED;
        }

        PhysicalOperatorContext child_context = BuildPhysicalPlan(aggregate->child);
        std::vector<std::unique_ptr<AggregationFunction>> agg_functions;
        Schema schema;

        for (const std::shared_ptr<Expression>& expr : aggregate->aggregate_expressions) {
            if (dynamic_cast<CountExpression*>(expr.get()) != nullptr) {
                agg_functions.push_back(std::make_unique<CountRowsAggregationFunction>());
                schema.AddColumn(expr->GetOutputName(), ColumnType::INT32);
                continue;
            }

            if (dynamic_cast<SumExpression*>(expr.get()) != nullptr) {
                const std::string col_name = expr->GetName();
                const size_t col_idx = child_context.schema.GetColumnIndexByName(col_name);
                const ColumnType col_type = child_context.schema.GetColumnTypeByName(col_name);

                switch (col_type) {
                    case ColumnType::INT32:
                        agg_functions.push_back(
                            std::make_unique<SumAggregationFunction<Int32Column>>(col_idx));
                        schema.AddColumn(expr->GetOutputName(), ColumnType::INT32);
                        break;
                    case ColumnType::FLOAT:
                        agg_functions.push_back(
                            std::make_unique<SumAggregationFunction<FloatColumn>>(col_idx));
                        schema.AddColumn(expr->GetOutputName(), ColumnType::FLOAT);
                        break;
                    default:
                        THROW_NOT_IMPLEMENTED;
                }
                continue;
            }

            THROW_NOT_IMPLEMENTED;
        }

        auto op = std::make_unique<AggregationOperator>(std::move(child_context.root_operator),
                                                        std::move(agg_functions));
        return {std::move(op), std::move(schema)};
    }

    THROW_RUNTIME_ERROR("Unknown plan node type");
}

#endif  // COLUMNAR_ENGINE_LOGIC_H
