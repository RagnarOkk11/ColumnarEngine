//
// Created by ragnarokk on 31.03.2026.
//

#include "AggregationFunctions.h"
#include "FilterFunctions.h"
#include "OperatorsBase.h"

#include <numeric>

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
        batch->selection_vector = std::make_shared<const std::vector<uint32_t>>(std::move(selection_vector));
        return batch;
    }
    return nullptr;
}

GroupByOperator::GroupByOperator(std::unique_ptr<Operator> child,
                                 std::vector<size_t> group_by_col_indices,
                                 std::vector<std::shared_ptr<Column>> empty_key_columns,
                                 std::vector<std::unique_ptr<AggregationFunction>> agg_funcs)
    : child_(std::move(child)),
      group_by_col_indices_(std::move(group_by_col_indices)),
      key_columns_(std::move(empty_key_columns)),
      agg_funcs_(std::move(agg_funcs)) {
}

std::unique_ptr<RecordBatch> GroupByOperator::Run() {
    if (!accumulated_) {
        while (auto batch = child_->Run()) {
            std::vector<uint32_t> group_ids;
            size_t batch_size = batch->num_rows;
            group_ids.reserve(batch_size);

            const std::shared_ptr<const std::vector<uint32_t>>& sel_vec = batch->selection_vector;
            std::vector<std::string> composite_keys(batch_size);
            for (size_t col_idx : group_by_col_indices_) {
                batch->columns[col_idx]->SerializeBatch(composite_keys, sel_vec.get());
            }

            std::vector<uint32_t> new_group_indices;
            size_t sz = key_columns_.empty() ? 0 : key_columns_[0]->Size();

            bool is_filtered = sel_vec != nullptr;
            for (size_t i = 0; i < batch_size; ++i) {
                const std::string& key = composite_keys[i];
                auto it = hash_table_.find(key);
                uint32_t gid;

                if (it == hash_table_.end()) {
                    gid = sz + new_group_indices.size();
                    hash_table_[key] = gid;
                    size_t actual_idx = is_filtered ? (*sel_vec)[i] : i;
                    new_group_indices.push_back(actual_idx);
                } else {
                    gid = it->second;
                }
                group_ids.push_back(gid);
            }

            if (!new_group_indices.empty()) {
                for (size_t k = 0; k < group_by_col_indices_.size(); ++k) {
                    key_columns_[k]->CopyBatchFrom(*batch->columns[group_by_col_indices_[k]],
                                                   &new_group_indices);
                }
            }

            for (auto& func : agg_funcs_) {
                func->Update(*batch, &group_ids);
            }
        }
        accumulated_ = true;

        size_t total_groups = key_columns_.empty() ? 0 : key_columns_[0]->Size();
        auto result_batch = std::make_unique<RecordBatch>();
        result_batch->num_rows = total_groups;

        for (size_t k = 0; k < key_columns_.size(); ++k) {
            result_batch->columns.push_back(std::move(key_columns_[k]));
        }
        for (size_t a = 0; a < agg_funcs_.size(); ++a) {
            auto full_agg_col = agg_funcs_[a]->Finalize();
            result_batch->columns.push_back(std::move(full_agg_col));
        }
        return result_batch;
    }

    return nullptr;
}

OrderByOperator::OrderByOperator(std::unique_ptr<Operator> child,
                                 std::vector<std::pair<size_t, bool>> sort_columns)
    : child_(std::move(child)), sort_columns_(std::move(sort_columns)) {
}

std::unique_ptr<RecordBatch> OrderByOperator::Run() {
    if (!accumulated_) {
        while (auto batch = child_->Run()) {
            if (!accumulated_batch_) {
                accumulated_batch_ = std::make_unique<RecordBatch>();
                size_t sz = batch->columns.size();
                for (size_t c = 0; c < sz; ++c) {
                    accumulated_batch_->columns.push_back(batch->columns[c]->CloneEmpty());
                }
                accumulated_batch_->num_rows = 0;
            }

            const std::shared_ptr<const std::vector<uint32_t>>& sel = batch->selection_vector;
            size_t sz = batch->columns.size();
            for (size_t c = 0; c < sz; ++c) {
                accumulated_batch_->columns[c]->CopyBatchFrom(*batch->columns[c], sel.get());
            }
            accumulated_batch_->num_rows += batch->num_rows;
        }

        if (accumulated_batch_) {
            size_t sz = accumulated_batch_->num_rows;
            indices_.reserve(sz);
            for (uint32_t i = 0; i < sz; ++i) {
                indices_.push_back(i);
            }

            sz = sort_columns_.size();
            for (int64_t i = static_cast<int64_t>(sz) - 1; i >= 0; --i) {
                const auto& [col_ind, is_desc] = sort_columns_[i];
                Column* col = accumulated_batch_->columns[col_ind].get();
                ColumnType column_type = col->GetType();

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                                               \
    case ColumnType::ENUM_VAL: {                                                                 \
        const auto& raw_data = static_cast<CLASS_TYPE*>(col)->GetData();                         \
        if (is_desc) {                                                                           \
            std::stable_sort(indices_.begin(), indices_.end(),                                   \
                             [&](uint32_t a, uint32_t b) { return raw_data[a] > raw_data[b]; }); \
        } else {                                                                                 \
            std::stable_sort(indices_.begin(), indices_.end(),                                   \
                             [&](uint32_t a, uint32_t b) { return raw_data[a] < raw_data[b]; }); \
        }                                                                                        \
        break;                                                                                   \
    }

                switch (column_type) {
                    FOR_EACH_COLUMN_TYPE(HANDLE_TYPE);
                    default: THROW_NOT_IMPLEMENTED;
                }
#undef HANDLE_TYPE
            }

            auto sorted_batch = std::make_unique<RecordBatch>();
            sorted_batch->num_rows = accumulated_batch_->num_rows;
            for (size_t c = 0; c < accumulated_batch_->columns.size(); ++c) {
                auto new_col = accumulated_batch_->columns[c]->CloneEmpty();
                new_col->CopyBatchFrom(*accumulated_batch_->columns[c], &indices_);
                sorted_batch->columns.push_back(std::move(new_col));
            }
            accumulated_batch_ = std::move(sorted_batch);

            accumulated_ = true;
        }
        if (!accumulated_batch_) {
            return nullptr;
        }
        return std::move(accumulated_batch_);
    }
    return nullptr;
}