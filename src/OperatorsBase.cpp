//
// Created by ragnarokk on 31.03.2026.
//

#include "AggregationFunctions.h"
#include "FilterFunctions.h"
#include "OperatorsBase.h"

#include <numeric>

ScanOperator::ScanOperator(const std::string& table_path,
                           const std::vector<std::string>& column_names)
    : reader_(table_path) {
    const std::vector<ColumnMetadata>& metadata = reader_.GetMetadata();

    if (column_names.empty()) {
        for (size_t i = 0; i < metadata.size(); ++i) {
            column_indices_.push_back(i);
        }
    } else {
        for (const auto& name : column_names) {
            bool found = false;
            for (size_t i = 0; i < metadata.size(); ++i) {
                if (metadata[i].name == name) {
                    column_indices_.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                THROW_RUNTIME_ERROR("Column " + name + " not found in metadata");
            }
        }
    }

    if (!metadata.empty()) {
        total_chunks_ = metadata[0].offsets.size();
    }
}

std::unique_ptr<RecordBatch> ScanOperator::Run() {
    if (current_chunk_ >= total_chunks_) {
        return nullptr;
    }

    auto record_batch = std::make_unique<RecordBatch>();
    record_batch->num_rows = 0;

    for (size_t col_idx : column_indices_) {
        std::shared_ptr<Column> column_data = reader_.GetColumnData(col_idx, current_chunk_);
        if (record_batch->num_rows == 0) {
            record_batch->num_rows = column_data->Size();
        }
        record_batch->columns.push_back(std::move(column_data));
    }

    ++current_chunk_;
    return record_batch;
}

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
        std::vector<size_t> selection_vector = filter_function_->Evaluate(*batch);
        if (selection_vector.empty()) {
            continue;
        }
        batch->num_rows = selection_vector.size();
        batch->selection_vector =
            std::make_shared<const std::vector<size_t>>(std::move(selection_vector));
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

            const std::shared_ptr<const std::vector<size_t>>& sel_vec = batch->selection_vector;
            std::vector<std::string> composite_keys(batch_size);
            for (size_t col_idx : group_by_col_indices_) {
                batch->columns[col_idx]->SerializeBatch(composite_keys, sel_vec.get());
            }

            std::vector<size_t> new_group_indices;
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
                                 std::vector<std::pair<size_t, bool>> sort_columns,
                                 std::optional<uint32_t> limit)
    : child_(std::move(child)), sort_columns_(std::move(sort_columns)), limit_(limit) {
}

using CompareFunc = int (*)(const void*, uint32_t, uint32_t);

struct SortColumnInfo {
    const void* raw_data;
    CompareFunc cmp_func;
    bool is_desc;
};

template <typename ColumnType>
int Compare(const void* raw_data, uint32_t a, uint32_t b) {
    using Container = ColumnType::ContainerType;
    const Container* data = static_cast<const Container*>(raw_data);
    if ((*data)[a] < (*data)[b]) {
        return -1;
    }
    if ((*data)[a] > (*data)[b]) {
        return 1;
    }
    return 0;
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

            const std::shared_ptr<const std::vector<size_t>>& sel = batch->selection_vector;
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

            std::vector<SortColumnInfo> cols_info;
            cols_info.reserve(sort_columns_.size());
            for (const auto& [col_idx, is_desc] : sort_columns_) {
                const Column* raw_column = accumulated_batch_->columns[col_idx].get();

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                      \
    case ColumnType::ENUM_VAL: {                                        \
        const void* data_ptr = raw_column->GetRawData();                \
        cols_info.push_back({data_ptr, &Compare<CLASS_TYPE>, is_desc}); \
        break;                                                          \
    }
                auto col_type = raw_column->GetType();
                switch (col_type) {
                    FOR_EACH_COLUMN_TYPE(HANDLE_TYPE);
                    default: THROW_NOT_IMPLEMENTED;
                }
#undef HANDLE_TYPE
            }

            auto cmp = [&cols_info](uint32_t a, uint32_t b) {
                for (const auto& info : cols_info) {
                    int res = info.cmp_func(info.raw_data, a, b);
                    if (res != 0) {
                        return info.is_desc ? (res > 0) : (res < 0);
                    }
                }
                return false;
            };

            if (limit_.has_value() && limit_.value() < sz) {
                uint32_t lim = limit_.value();
                std::nth_element(indices_.begin(), indices_.begin() + lim, indices_.end(), cmp);
                std::sort(indices_.begin(), indices_.begin() + lim, cmp);
                indices_.resize(lim);
                accumulated_batch_->num_rows = lim;
            } else {
                std::sort(indices_.begin(), indices_.end(), cmp);
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

LimitOperator::LimitOperator(std::unique_ptr<Operator> child, size_t limit)
    : child_(std::move(child)), limit_(limit) {
}

std::unique_ptr<RecordBatch> LimitOperator::Run() {
    if (cur_rows_ >= limit_) {
        return nullptr;
    }
    std::unique_ptr<RecordBatch> batch = child_->Run();
    if (!batch) {
        return nullptr;
    }

    if (cur_rows_ + batch->num_rows <= limit_) {
        cur_rows_ += batch->num_rows;
        return batch;
    } else {
        size_t remaining = limit_ - cur_rows_;
        cur_rows_ += remaining;
        batch->num_rows = remaining;
        if (batch->selection_vector) {
            auto new_sel_vec = std::make_shared<std::vector<size_t>>(
                batch->selection_vector->begin(),
                batch->selection_vector->begin() + remaining
            );
            batch->selection_vector = std::move(new_sel_vec);
        } else {
            auto sel_vec = std::make_shared<std::vector<size_t>>(remaining);
            std::iota(sel_vec->begin(), sel_vec->end(), 0);
            batch->selection_vector = std::move(sel_vec);
        }

        return batch;
    }
}
