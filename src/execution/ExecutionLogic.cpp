#include "execution/ExecutionLogic.h"

PhysicalOperatorContext ScanNode::BuildPhysicalPlan(
    std::optional<std::vector<std::string>> required_columns) const {
    const auto& full_columns = table_meta->GetColumns();

    std::vector<std::string> user_cols;
    if (column_names.size() == 1 && column_names[0] == "*") {
        for (const auto& field : full_columns) {
            user_cols.push_back(field.name);
        }
    } else {
        user_cols = column_names;
    }

    std::vector<std::string> final_columns = user_cols;
    if (required_columns.has_value()) {
        final_columns.insert(final_columns.end(), required_columns->begin(),
                             required_columns->end());
        std::sort(final_columns.begin(), final_columns.end());
        final_columns.erase(std::unique(final_columns.begin(), final_columns.end()),
                            final_columns.end());
    }

    auto op = std::make_unique<ScanOperator>(table_path, final_columns, table_meta);

    Schema output_schema;
    for (const std::string& name : final_columns) {
        bool found = false;
        for (const auto& field : full_columns) {
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

PhysicalOperatorContext AggregateNode::BuildPhysicalPlan(
    std::optional<std::vector<std::string>>) const {
    if (!group_by_columns.empty()) {
        std::vector<std::string> needed_columns = group_by_columns;
        for (const std::shared_ptr<AggregateExpression>& expr : aggregate_expressions) {
            expr->CollectRequiredColumns(needed_columns);
        }
        std::sort(needed_columns.begin(), needed_columns.end());
        needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                             needed_columns.end());

        PhysicalOperatorContext child_context = child->BuildPhysicalPlan(needed_columns);

        std::vector<size_t> group_col_indices;
        std::vector<std::shared_ptr<ColumnBuilder>> key_builders;
        Schema output_schema;

        for (const std::string& col_name : group_by_columns) {
            size_t ind = child_context.schema.GetColumnIndexByName(col_name);
            group_col_indices.push_back(ind);

            ColumnType col_type = child_context.schema.GetColumnTypeByName(col_name);
            output_schema.AddColumn(col_name, col_type);
            key_builders.push_back(ColumnFactory::MakeColumnBuilder(col_type));
        }

        std::vector<std::unique_ptr<GroupedAggregationFunction>> agg_funcs;
        for (const std::shared_ptr<AggregateExpression>& expr : aggregate_expressions) {
            agg_funcs.push_back(
                expr->CreateGroupedAggregationFunction(child_context.schema, output_schema));
        }

        auto op = std::make_unique<GroupByOperator>(std::move(child_context.root_operator),
                                                    std::move(group_col_indices),
                                                    std::move(key_builders), std::move(agg_funcs));
        return {std::move(op), std::move(output_schema)};
    }

    std::vector<std::string> needed_columns = group_by_columns;
    for (const std::shared_ptr<AggregateExpression>& expr : aggregate_expressions) {
        expr->CollectRequiredColumns(needed_columns);
    }
    std::sort(needed_columns.begin(), needed_columns.end());
    needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                         needed_columns.end());

    PhysicalOperatorContext child_context = child->BuildPhysicalPlan(needed_columns);
    std::vector<std::unique_ptr<GlobalAggregationFunction>> agg_functions;
    Schema output_schema;
    for (const std::shared_ptr<AggregateExpression>& expr : aggregate_expressions) {
        agg_functions.push_back(
            expr->CreateGlobalAggregationFunction(child_context.schema, output_schema));
    }
    auto op = std::make_unique<AggregationOperator>(std::move(child_context.root_operator),
                                                    std::move(agg_functions));
    return {std::move(op), std::move(output_schema)};
}

PhysicalOperatorContext FilterNode::BuildPhysicalPlan(
    std::optional<std::vector<std::string>> required_columns) const {
    std::vector<std::string> needed_columns;
    filter_expression->CollectRequiredColumns(needed_columns);
    if (required_columns.has_value()) {
        needed_columns.insert(needed_columns.end(), required_columns->begin(),
                              required_columns->end());
    }

    std::sort(needed_columns.begin(), needed_columns.end());
    needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                         needed_columns.end());

    PhysicalOperatorContext child_context = child->BuildPhysicalPlan(needed_columns);
    std::unique_ptr<FilterFunction> filter_function =
        filter_expression->CreateFilterFunction(child_context.schema);
    auto op = std::make_unique<FilterOperator>(std::move(child_context.root_operator),
                                               std::move(filter_function));
    return {std::move(op), std::move(child_context.schema)};
}

PhysicalOperatorContext OrderByNode::BuildPhysicalPlan(
    std::optional<std::vector<std::string>> required_columns) const {
    std::vector<std::string> needed_columns;
    if (required_columns.has_value()) {
        needed_columns = required_columns.value();
    }
    for (const auto& [col_name, is_desc] : order_by_columns) {
        needed_columns.push_back(col_name);
    }
    std::sort(needed_columns.begin(), needed_columns.end());
    needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                         needed_columns.end());

    PhysicalOperatorContext child_context = child->BuildPhysicalPlan(needed_columns);
    std::vector<std::pair<size_t, bool>> sort_columns;
    for (const auto& [col_name, is_desc] : order_by_columns) {
        size_t sort_col_idx = child_context.schema.GetColumnIndexByName(col_name);
        sort_columns.push_back({sort_col_idx, is_desc});
    }

    auto op = std::make_unique<OrderByOperator>(std::move(child_context.root_operator),
                                                std::move(sort_columns), std::move(limit),
                                                std::move(offset));
    return {std::move(op), std::move(child_context.schema)};
}

PhysicalOperatorContext LimitNode::BuildPhysicalPlan(
    std::optional<std::vector<std::string>> required_columns) const {
    PhysicalOperatorContext child_context = child->BuildPhysicalPlan(required_columns);
    auto op = std::make_unique<LimitOperator>(std::move(child_context.root_operator), limit);
    return {std::move(op), std::move(child_context.schema)};
}

PhysicalOperatorContext ScalarNode::BuildPhysicalPlan(
    std::optional<std::vector<std::string>> required_columns) const {
    std::vector<std::string> needed_columns;
    if (required_columns.has_value()) {
        for (const auto& req_col : required_columns.value()) {
            bool is_generated_here = false;
            for (const auto& expr : scalar_expressions) {
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
    for (const auto& expr : scalar_expressions) {
        expr->CollectRequiredColumns(needed_columns);
    }

    std::sort(needed_columns.begin(), needed_columns.end());
    needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                         needed_columns.end());

    PhysicalOperatorContext child_context = child->BuildPhysicalPlan(needed_columns);

    std::vector<std::unique_ptr<ScalarFunction>> scalar_functions;

    for (const auto& expr : scalar_expressions) {
        scalar_functions.push_back(expr->CreateScalarFunction(child_context.schema));
    }

    auto op = std::make_unique<ScalarOperator>(std::move(child_context.root_operator),
                                               std::move(scalar_functions));
    return {std::move(op), std::move(child_context.schema)};
}

PhysicalOperatorContext DropNode::BuildPhysicalPlan(
    std::optional<std::vector<std::string>> required_columns) const {
    PhysicalOperatorContext child_context = child->BuildPhysicalPlan(required_columns);

    std::vector<size_t> cols_to_keep_indices;
    Schema output_schema;
    const auto& child_fields = child_context.schema.GetFields();

    for (const std::string& drop_col : columns_to_drop) {
        bool found = false;
        for (const auto& field : child_fields) {
            if (field.name == drop_col) {
                found = true;
                break;
            }
        }
        if (!found) {
            THROW_RUNTIME_ERROR("Cannot drop column '" + drop_col + "': not found in dataframe");
        }
    }

    for (size_t i = 0; i < child_fields.size(); ++i) {
        const auto& field = child_fields[i];
        if (std::find(columns_to_drop.begin(), columns_to_drop.end(), field.name) ==
            columns_to_drop.end()) {
            cols_to_keep_indices.push_back(i);
            output_schema.AddField({field.name, field.type});
        }
    }

    auto op = std::make_unique<DropOperator>(std::move(child_context.root_operator),
                                             std::move(cols_to_keep_indices));
    return {std::move(op), std::move(output_schema)};
}

PhysicalOperatorContext ReorderNode::BuildPhysicalPlan(
    std::optional<std::vector<std::string>> required_columns) const {
    PhysicalOperatorContext child_context = child->BuildPhysicalPlan(required_columns);

    std::vector<size_t> new_indices;
    Schema output_schema;
    const auto& child_fields = child_context.schema.GetFields();

    for (const std::string& reorder_col : desired_order) {
        bool found = false;
        for (size_t i = 0; i < child_fields.size(); ++i) {
            const auto& field = child_fields[i];
            if (field.name == reorder_col) {
                found = true;
                new_indices.push_back(i);
                output_schema.AddField({field.name, field.type});
                break;
            }
        }

        if (!found) {
            THROW_RUNTIME_ERROR("Cannot reorder column '" + reorder_col +
                                "': not found in dataframe");
        }
    }

    auto op = std::make_unique<ReorderOperator>(std::move(child_context.root_operator),
                                                std::move(new_indices));
    return {std::move(op), std::move(output_schema)};
}
