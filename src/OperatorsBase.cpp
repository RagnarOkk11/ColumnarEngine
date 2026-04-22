//
// Created by ragnarokk on 31.03.2026.
//

#include "AggregationFunctions.h"
#include "FilterFunctions.h"
#include "OperatorsBase.h"

AggregationOperator::AggregationOperator(
    std::unique_ptr<Operator> child,
    std::vector<std::unique_ptr<AggregationFunction>> aggregation_functions)
    : child_(std::move(child)), aggregation_functions_(std::move(aggregation_functions)) {
}

std::unique_ptr<RecordBatch> AggregationOperator::Run() {
    if (finished_) {
        return nullptr;
    }
    while (std::unique_ptr<RecordBatch> batch = child_->Run()) {
        for (std::unique_ptr<AggregationFunction>& aggregation_function : aggregation_functions_) {
            aggregation_function->Update(*batch);
        }
    }
    auto result_batch = std::make_unique<RecordBatch>();
    result_batch->num_rows = 1;

    for (std::unique_ptr<AggregationFunction>& aggregation_function : aggregation_functions_) {
        result_batch->columns.push_back(aggregation_function->Finalize());
    }

    finished_ = true;
    return result_batch;
}

FilterOperator::FilterOperator(std::unique_ptr<Operator> child,
                               std::shared_ptr<FilterFunction> filter_function)
    : child_(std::move(child)), filter_function_(std::move(filter_function)) {
}

std::unique_ptr<RecordBatch> FilterOperator::Run() {
    while (std::unique_ptr<RecordBatch> batch = child_->Run()) {
        std::vector<uint32_t> selection_vector = filter_function_->Evaluate(*batch);
        if (selection_vector.empty()) {
            continue;
        }
        batch->num_rows = selection_vector.size();
        batch->selection_vector = std::move(selection_vector);
        return batch;
    }
    return nullptr;
}
