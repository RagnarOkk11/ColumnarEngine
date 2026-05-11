#include "execution/expressions/AggregationFunctions.h"
#include "execution/expressions/FilterFunctions.h"
#include "execution/OperatorsBase.h"
#include "execution/expressions/ScalarFunctions.h"
#include "execution/ExecutionHelper.h"
#include "column/ColumnFactory.h"

#include <numeric>

ScanOperator::ScanOperator(const std::string& table_path,
                           const std::vector<std::string>& column_names)
    : reader_(table_path) {
    const std::vector<ColumnMetadata>& metadata = reader_.GetMetadata();

    if (column_names == std::vector<std::string>{"*"}) {
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
    std::vector<std::unique_ptr<GlobalAggregationFunction>> aggregation_functions)
    : child_(std::move(child)), aggregation_functions_(std::move(aggregation_functions)) {
}

std::unique_ptr<RecordBatch> AggregationOperator::Run() {
    if (finished_) {
        return nullptr;
    }
    while (std::unique_ptr<RecordBatch> batch = child_->Run()) {
        for (std::unique_ptr<GlobalAggregationFunction>& aggregation_function :
             aggregation_functions_) {
            aggregation_function->Update(*batch);
        }
    }
    auto result_batch = std::make_unique<RecordBatch>();
    result_batch->num_rows = 1;

    for (std::unique_ptr<GlobalAggregationFunction>& aggregation_function :
         aggregation_functions_) {
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
                                 std::vector<std::shared_ptr<ColumnBuilder>> key_builders,
                                 std::vector<std::unique_ptr<GroupedAggregationFunction>> agg_funcs)
    : child_(std::move(child)),
      group_by_col_indices_(std::move(group_by_col_indices)),
      key_builders_(std::move(key_builders)),
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
                ExecutionHelper::HashKeys(*batch->columns[col_idx], composite_keys, sel_vec.get());
            }

            std::vector<size_t> new_group_indices;
            size_t sz = num_groups_;

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
                    ExecutionHelper::CopySelection(*key_builders_[k],
                                                   *batch->columns[group_by_col_indices_[k]],
                                                   &new_group_indices);
                }
            }

            num_groups_ = hash_table_.size();
            for (auto& func : agg_funcs_) {
                func->Resize(num_groups_);
                func->Update(*batch, group_ids);
            }
        }
        accumulated_ = true;

        auto result_batch = std::make_unique<RecordBatch>();
        result_batch->num_rows = num_groups_;

        for (size_t k = 0; k < key_builders_.size(); ++k) {
            result_batch->columns.push_back(key_builders_[k]->Finish());
        }
        for (size_t a = 0; a < agg_funcs_.size(); ++a) {
            result_batch->columns.push_back(agg_funcs_[a]->Finalize());
        }
        return result_batch;
    }

    return nullptr;
}

OrderByOperator::OrderByOperator(std::unique_ptr<Operator> child,
                                 std::vector<std::pair<size_t, bool>> sort_columns,
                                 std::optional<size_t> limit, std::optional<size_t> offset)
    : child_(std::move(child)),
      sort_columns_(std::move(sort_columns)),
      limit_(limit),
      offset_(offset) {
    total_limit_ = limit_;
    if (total_limit_.has_value()) {
        if (offset_.has_value()) {
            total_limit_.value() += offset_.value();
        }
    } else {
        total_limit_ = offset_;
    }
}

using CompareFunc = int (*)(const void*, uint32_t, uint32_t);

struct SortColumnInfo {
    const void* raw_data;
    CompareFunc cmp_func;
    bool is_desc;
};

template <typename ColumnT>
int Compare(const void* raw_data, uint32_t a, uint32_t b) {
    using Container = ColumnT::ContainerType;
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
    if (accumulated_) {
        return nullptr;
    }

    while (auto batch = child_->Run()) {
        if (accum_builders_.empty()) {
            for (size_t c = 0; c < batch->columns.size(); ++c) {
                accum_builders_.push_back(
                    ColumnFactory::MakeColumnBuilder(batch->columns[c]->GetType()));
                accum_types_.push_back(batch->columns[c]->GetType());
            }
            accum_num_rows_ = 0;
        }

        const std::shared_ptr<const std::vector<size_t>>& sel = batch->selection_vector;
        for (size_t c = 0; c < accum_builders_.size(); ++c) {
            ExecutionHelper::CopySelection(*accum_builders_[c], *batch->columns[c], sel.get());
        }
        accum_num_rows_ += batch->num_rows;

        if (total_limit_.has_value() && accum_num_rows_ > total_limit_.value() * 10) {
            TrimCurBatch(false);
        }
    }

    if (accum_num_rows_ > 0) {
        TrimCurBatch(true);

        auto final_batch = std::make_unique<RecordBatch>();
        final_batch->num_rows = accum_num_rows_;
        for (auto& builder : accum_builders_) {
            final_batch->columns.push_back(builder->Finish());
        }

        accumulated_ = true;
        return final_batch;
    }

    accumulated_ = true;
    return nullptr;
}

void OrderByOperator::TrimCurBatch(bool is_final) {
    if (accum_num_rows_ == 0) {
        return;
    }

    auto temp_batch = std::make_unique<RecordBatch>();
    temp_batch->num_rows = accum_num_rows_;
    for (auto& builder : accum_builders_) {
        temp_batch->columns.push_back(builder->Finish());
    }
    accum_builders_.clear();

    std::vector<size_t> indices(accum_num_rows_);
    for (size_t i = 0; i < accum_num_rows_; ++i) {
        indices[i] = i;
    }

    std::vector<SortColumnInfo> cols_info;
    cols_info.reserve(sort_columns_.size());
    for (const auto& [col_ind, is_desc] : sort_columns_) {
        const Column* raw_column = temp_batch->columns[col_ind].get();

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

    if (total_limit_.has_value() && accum_num_rows_ > total_limit_.value()) {
        uint32_t lim = total_limit_.value();
        std::nth_element(indices.begin(), indices.begin() + lim, indices.end(), cmp);
        indices.resize(lim);
    }
    if (is_final) {
        std::sort(indices.begin(), indices.end(), cmp);
        if (offset_.has_value()) {
            if (offset_.value() < indices.size()) {
                indices.erase(indices.begin(), indices.begin() + offset_.value());
            } else {
                indices.clear();
            }
        }
    }

    for (auto type : accum_types_) {
        accum_builders_.push_back(ColumnFactory::MakeColumnBuilder(type));
    }
    for (size_t c = 0; c < accum_builders_.size(); ++c) {
        ExecutionHelper::CopySelection(*accum_builders_[c], *temp_batch->columns[c], &indices);
    }
    accum_num_rows_ = indices.size();
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
                batch->selection_vector->begin(), batch->selection_vector->begin() + remaining);
            batch->selection_vector = std::move(new_sel_vec);
        } else {
            auto sel_vec = std::make_shared<std::vector<size_t>>(remaining);
            std::iota(sel_vec->begin(), sel_vec->end(), 0);
            batch->selection_vector = std::move(sel_vec);
        }

        return batch;
    }
}

ScalarOperator::ScalarOperator(std::unique_ptr<Operator> child,
                               std::vector<std::unique_ptr<ScalarFunction>> scalar_functions)
    : child_(std::move(child)), scalar_functions_(std::move(scalar_functions)) {
}

std::unique_ptr<RecordBatch> ScalarOperator::Run() {
    auto batch = child_->Run();
    if (!batch) {
        return nullptr;
    }

    for (auto& func : scalar_functions_) {
        batch->columns.push_back(func->Evaluate(*batch));
    }

    return batch;
}

DropOperator::DropOperator(std::unique_ptr<Operator> child, std::vector<size_t> column_stay_indices)
    : child_(std::move(child)), column_stay_indices_(std::move(column_stay_indices)) {
}

std::unique_ptr<RecordBatch> DropOperator::Run() {
    auto batch = child_->Run();
    if (!batch) {
        return nullptr;
    }

    std::vector<std::shared_ptr<Column>> new_columns;
    new_columns.reserve(column_stay_indices_.size());

    for (size_t ind : column_stay_indices_) {
        new_columns.push_back(std::move(batch->columns[ind]));
    }

    batch->columns = std::move(new_columns);
    return batch;
}
