#pragma once

#include "execution/expressions/AggregationExpressions.h"
#include "execution/expressions/FilterExpressions.h"
#include "execution/ExecutionLogic.h"
#include "execution/ExecutionHelper.h"

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
                std::cout << field.name << " ";
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
                    std::cout << ExecutionHelper::FormatValue(*column, ind) << " ";
                }
                std::cout << "\n";
            }
        }
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
        auto scan_node = std::make_shared<ScanNode>();
        scan_node->table_path = std::move(columnar_file_path);
        scan_node->column_names = std::move(column_names);
        return DataFrame(scan_node);
    }

    DataFrame Aggregate(std::vector<std::string> group_by_columns,
                        std::vector<std::shared_ptr<AggregateExpression>> aggregate_expressions) {
        auto aggregate_node = std::make_shared<AggregateNode>();
        aggregate_node->child = logical_plan_;
        aggregate_node->group_by_columns = std::move(group_by_columns);
        aggregate_node->aggregate_expressions = std::move(aggregate_expressions);
        return DataFrame(aggregate_node);
    }

    DataFrame Filter(std::shared_ptr<FilterExpression> filter_expression) {
        auto filter_node = std::make_shared<FilterNode>();
        filter_node->child = logical_plan_;
        filter_node->filter_expression = std::move(filter_expression);
        return DataFrame(filter_node);
    }

    DataFrame OrderBy(std::vector<std::pair<std::string, bool>> order_by_columns,
                      std::optional<size_t> limit = std::nullopt) {
        auto order_by_node = std::make_shared<OrderByNode>();
        order_by_node->child = logical_plan_;
        order_by_node->order_by_columns = std::move(order_by_columns);
        order_by_node->limit = limit;
        return DataFrame(order_by_node);
    }

    DataFrame Limit(size_t limit) {
        auto limit_node = std::make_shared<LimitNode>();
        limit_node->child = logical_plan_;
        limit_node->limit = limit;
        return DataFrame(limit_node);
    }

    DataFrame Project(std::vector<std::shared_ptr<ScalarExpression>> scalar_expressions) {
        auto scalar_node = std::make_shared<ScalarNode>();
        scalar_node->child = logical_plan_;
        scalar_node->scalar_expressions = std::move(scalar_expressions);
        return DataFrame(scalar_node);
    }

    DataResult Collect() {
        PhysicalOperatorContext context = BuildPhysicalPlan(logical_plan_);
        std::unique_ptr<Operator> physical_plan_root = std::move(context.root_operator);
        std::vector<std::unique_ptr<RecordBatch>> result;
        while (std::unique_ptr<RecordBatch> batch = physical_plan_root->Run()) {
            if (batch != nullptr) {
                result.push_back(std::move(batch));
            }
        }
        return DataResult{std::move(result), std::move(context.schema)};
    }

private:
    std::shared_ptr<PlanNode> logical_plan_;
};
