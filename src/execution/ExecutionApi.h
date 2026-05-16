#pragma once

#include "execution/expressions/AggregationExpressions.h"
#include "execution/expressions/FilterExpressions.h"
#include "execution/ExecutionLogic.h"
#include "execution/ExecutionHelper.h"
#include "execution/PipelineExecutor.h"

#include <iostream>

class DataResult {
public:
    DataResult(std::vector<std::unique_ptr<RecordBatch>>&& batches, Schema schema)
        : batches_(std::move(batches)), schema_(std::move(schema)) {
    }

    const std::vector<std::unique_ptr<RecordBatch>>& GetBatches() const {
        return batches_;
    }

    const Schema& GetSchema() const {
        return schema_;
    }

    void Display() const {
        const std::vector<Field>& fields = schema_.GetFields();
        if (!fields.empty()) {
            for (const Field& field : fields) {
                std::cout << field.name << ",";
            }
            std::cout << "\n";
        }

        for (const std::unique_ptr<RecordBatch>& batch : batches_) {
            if (batch->columns.empty() || batch->num_rows == 0) {
                continue;
            }
            size_t rows = batch->num_rows;
            for (size_t i = 0; i < rows; ++i) {
                size_t ind = batch->selection_vector ? (*batch->selection_vector)[i] : i;
                for (const std::shared_ptr<Column>& column : batch->columns) {
                    std::cout << ExecutionHelper::FormatValue(*column, ind) << ",";
                }
                std::cout << "\n";
            }
        }
    }

    std::shared_ptr<Column> GetResult(const std::string& column_name) const {
        const std::vector<Field>& fields = schema_.GetFields();
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i].name == column_name) {
                if (!batches_[0]->columns.empty()) {
                    return batches_[0]->columns[i];
                }
                break;
            }
        }
        THROW_RUNTIME_ERROR("No charoncik bebe");
    }

private:
    std::vector<std::unique_ptr<RecordBatch>> batches_;
    Schema schema_;
};

class DataFrame {
public:
    explicit DataFrame(std::shared_ptr<PlanNode> logical_plan)
        : logical_plan_(std::move(logical_plan)) {
    }

    static DataFrame Select(std::string columnar_file_path, std::vector<std::string> column_names) {
        auto table_meta = std::make_shared<MetadataTable>(columnar_file_path);

        auto scan_node = std::make_shared<ScanNode>(std::move(columnar_file_path),
                                                    std::move(column_names), std::move(table_meta));
        return DataFrame(scan_node);
    }

    DataFrame Aggregate(std::vector<std::string> group_by_columns,
                        std::vector<std::shared_ptr<AggregateExpression>> aggregate_expressions) {
        auto aggregate_node = std::make_shared<AggregateNode>(
            logical_plan_, std::move(group_by_columns), std::move(aggregate_expressions));

        return DataFrame(aggregate_node);
    }

    DataFrame Filter(std::shared_ptr<FilterExpression> filter_expression) {
        auto filter_node =
            std::make_shared<FilterNode>(logical_plan_, std::move(filter_expression));
        return DataFrame(filter_node);
    }

    DataFrame OrderBy(std::vector<std::pair<std::string, bool>> order_by_columns,
                      std::optional<size_t> limit = std::nullopt,
                      std::optional<size_t> offset = std::nullopt) {
        auto order_by_node = std::make_shared<OrderByNode>(
            logical_plan_, std::move(order_by_columns), std::move(limit), std::move(offset));
        return DataFrame(order_by_node);
    }

    DataFrame Limit(size_t limit) {
        auto limit_node = std::make_shared<LimitNode>(logical_plan_, limit);
        return DataFrame(limit_node);
    }

    DataFrame Project(std::vector<std::shared_ptr<ScalarExpression>> scalar_expressions) {
        auto scalar_node =
            std::make_shared<ScalarNode>(logical_plan_, std::move(scalar_expressions));
        return DataFrame(scalar_node);
    }

    DataFrame Drop(std::vector<std::string> columns_to_drop) {
        auto drop_node = std::make_shared<DropNode>(logical_plan_, std::move(columns_to_drop));
        return DataFrame(drop_node);
    }

    DataFrame Reorder(std::vector<std::string> desired_order) {
        auto node = std::make_shared<ReorderNode>(logical_plan_, std::move(desired_order));
        return DataFrame(node);
    }

    DataResult Collect() const {
        PipelineBuildContext ctx;

        auto outer_pipe = std::make_unique<Pipeline>();
        auto result_sink = std::make_shared<ResultSinkOperator>();
        outer_pipe->sink = result_sink;
        ctx.current_pipeline = outer_pipe.get();

        logical_plan_->BuildPipelines(ctx);
        ctx.completed_pipelines.push_back(std::move(outer_pipe));
        for (auto& pipeline : ctx.completed_pipelines) {
            pipeline->Execute();
        }

        return DataResult{result_sink->TakeBatches(), std::move(ctx.schema)};
    }

private:
    std::shared_ptr<PlanNode> logical_plan_;
};
