#include "execution/ExecutionLogic.h"

// ========================= ScanNode =========================

void ScanNode::BuildPipelines(PipelineBuildContext& ctx,
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
        for (const std::string& req_col : required_columns.value()) {
            if (std::find(final_columns.begin(), final_columns.end(), req_col) ==
                final_columns.end()) {
                final_columns.push_back(req_col);
            }
        }
    }

    auto scan_op = std::make_shared<ScanOperator>(table_path, final_columns, table_meta);
    ctx.current_pipeline->source = scan_op;

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

    ctx.schema = std::move(output_schema);
}

// ========================= AggregateNode =========================

void AggregateNode::BuildPipelines(PipelineBuildContext& ctx,
                                   std::optional<std::vector<std::string>>) const {
    Pipeline* outer_pipeline = ctx.current_pipeline;
    Pipeline* inner_pipeline = new Pipeline();
    ctx.current_pipeline = inner_pipeline;

    if (!group_by_columns.empty()) {
        // === GROUP BY path ===
        std::vector<std::string> needed_columns = group_by_columns;
        for (const std::shared_ptr<AggregateExpression>& expr : aggregate_expressions) {
            expr->CollectRequiredColumns(needed_columns);
        }
        std::sort(needed_columns.begin(), needed_columns.end());
        needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                             needed_columns.end());

        child->BuildPipelines(ctx, needed_columns);

        std::vector<size_t> group_col_indices;
        std::vector<std::shared_ptr<ColumnBuilder>> key_builders;
        Schema output_schema;

        for (const std::string& col_name : group_by_columns) {
            size_t ind = ctx.schema.GetColumnIndexByName(col_name);
            group_col_indices.push_back(ind);

            ColumnType col_type = ctx.schema.GetColumnTypeByName(col_name);
            output_schema.AddColumn(col_name, col_type);
            key_builders.push_back(ColumnFactory::MakeColumnBuilder(col_type));
        }

        std::vector<std::unique_ptr<GroupedAggregationFunction>> agg_funcs;
        for (const std::shared_ptr<AggregateExpression>& expr : aggregate_expressions) {
            agg_funcs.push_back(expr->CreateGroupedAggregationFunction(ctx.schema, output_schema));
        }

        auto breaker = std::make_shared<GroupBySinkSourceOperator>(
            std::move(group_col_indices), std::move(key_builders), std::move(agg_funcs));

        inner_pipeline->sink = breaker;
        ctx.completed_pipelines.push_back(std::unique_ptr<Pipeline>(inner_pipeline));
        outer_pipeline->source = breaker;
        ctx.current_pipeline = outer_pipeline;
        ctx.schema = std::move(output_schema);
    } else {
        // === Global aggregation path ===
        std::vector<std::string> needed_columns;
        for (const std::shared_ptr<AggregateExpression>& expr : aggregate_expressions) {
            expr->CollectRequiredColumns(needed_columns);
        }
        std::sort(needed_columns.begin(), needed_columns.end());
        needed_columns.erase(std::unique(needed_columns.begin(), needed_columns.end()),
                             needed_columns.end());

        child->BuildPipelines(ctx, needed_columns);

        std::vector<std::unique_ptr<GlobalAggregationFunction>> agg_functions;
        Schema output_schema;
        for (const std::shared_ptr<AggregateExpression>& expr : aggregate_expressions) {
            agg_functions.push_back(
                expr->CreateGlobalAggregationFunction(ctx.schema, output_schema));
        }

        auto breaker = std::make_shared<AggregationSinkSourceOperator>(std::move(agg_functions));

        inner_pipeline->sink = breaker;
        ctx.completed_pipelines.push_back(std::unique_ptr<Pipeline>(inner_pipeline));
        outer_pipeline->source = breaker;
        ctx.current_pipeline = outer_pipeline;
        ctx.schema = std::move(output_schema);
    }
}

// ========================= FilterNode =========================

void FilterNode::BuildPipelines(PipelineBuildContext& ctx,
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

    child->BuildPipelines(ctx, needed_columns);

    std::unique_ptr<FilterFunction> filter_function =
        filter_expression->CreateFilterFunction(ctx.schema);
    auto filter_op = std::make_shared<FilterTransformOperator>(std::move(filter_function));
    ctx.current_pipeline->transforms.push_back(filter_op);
}

// ========================= OrderByNode =========================

void OrderByNode::BuildPipelines(PipelineBuildContext& ctx,
                                 std::optional<std::vector<std::string>> required_columns) const {
    Pipeline* outer_pipeline = ctx.current_pipeline;
    Pipeline* inner_pipeline = new Pipeline();
    ctx.current_pipeline = inner_pipeline;

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

    child->BuildPipelines(ctx, needed_columns);

    std::vector<std::pair<size_t, bool>> sort_columns;
    for (const auto& [col_name, is_desc] : order_by_columns) {
        size_t sort_col_idx = ctx.schema.GetColumnIndexByName(col_name);
        sort_columns.push_back({sort_col_idx, is_desc});
    }

    auto breaker = std::make_shared<OrderBySinkSourceOperator>(std::move(sort_columns),
                                                               std::move(limit), std::move(offset));

    inner_pipeline->sink = breaker;
    ctx.completed_pipelines.push_back(std::unique_ptr<Pipeline>(inner_pipeline));
    outer_pipeline->source = breaker;
    ctx.current_pipeline = outer_pipeline;
}

// ========================= LimitNode =========================

void LimitNode::BuildPipelines(PipelineBuildContext& ctx,
                               std::optional<std::vector<std::string>> required_columns) const {
    child->BuildPipelines(ctx, required_columns);
    auto limit_op = std::make_shared<LimitTransformOperator>(limit);
    ctx.current_pipeline->transforms.push_back(limit_op);
}

// ========================= ScalarNode =========================

void ScalarNode::BuildPipelines(PipelineBuildContext& ctx,
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

    child->BuildPipelines(ctx, needed_columns);

    std::vector<std::unique_ptr<ScalarFunction>> scalar_functions;
    for (const auto& expr : scalar_expressions) {
        scalar_functions.push_back(expr->CreateScalarFunction(ctx.schema));
    }

    auto scalar_op = std::make_shared<ScalarTransformOperator>(std::move(scalar_functions));
    ctx.current_pipeline->transforms.push_back(scalar_op);
}

// ========================= DropNode =========================

void DropNode::BuildPipelines(PipelineBuildContext& ctx,
                              std::optional<std::vector<std::string>> required_columns) const {
    child->BuildPipelines(ctx, required_columns);

    const auto& child_fields = ctx.schema.GetFields();

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

    std::vector<size_t> cols_to_keep_indices;
    Schema output_schema;
    for (size_t i = 0; i < child_fields.size(); ++i) {
        const auto& field = child_fields[i];
        if (std::find(columns_to_drop.begin(), columns_to_drop.end(), field.name) ==
            columns_to_drop.end()) {
            cols_to_keep_indices.push_back(i);
            output_schema.AddField({field.name, field.type});
        }
    }

    auto drop_op = std::make_shared<DropTransformOperator>(std::move(cols_to_keep_indices));
    ctx.current_pipeline->transforms.push_back(drop_op);
    ctx.schema = std::move(output_schema);
}

// ========================= ReorderNode =========================

void ReorderNode::BuildPipelines(PipelineBuildContext& ctx,
                                 std::optional<std::vector<std::string>> required_columns) const {
    child->BuildPipelines(ctx, required_columns);

    const auto& child_fields = ctx.schema.GetFields();

    std::vector<size_t> new_indices;
    Schema output_schema;

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

    auto reorder_op = std::make_shared<ReorderTransformOperator>(std::move(new_indices));
    ctx.current_pipeline->transforms.push_back(reorder_op);
    ctx.schema = std::move(output_schema);
}
