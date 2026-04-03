//
// Created by ragnarokk on 31.03.2026.
//

#ifndef COLUMNAR_ENGINE_EXECUTIONAPI_H
#define COLUMNAR_ENGINE_EXECUTIONAPI_H

#include "ExecutionLogic.h"
#include "Expressions.h"

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
            if (batch->columns.empty()) {
                continue;
            }
            size_t rows = batch->columns[0]->Size();
            for (size_t i = 0; i < rows; ++i) {
                for (const std::unique_ptr<Column>& column : batch->columns) {
                    std::cout << column->GetDataAsString(i) << " ";
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
                        std::vector<std::shared_ptr<Expression>> aggregate_expressions) {
        auto aggregate_node = std::make_shared<AggregateNode>();
        aggregate_node->child = logical_plan_;
        aggregate_node->group_by_columns = std::move(group_by_columns);
        aggregate_node->aggregate_expressions = std::move(aggregate_expressions);
        return DataFrame(aggregate_node);
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

#endif  // COLUMNAR_ENGINE_EXECUTIONAPI_H
