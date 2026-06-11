#include "execution/expressions/AggregationFunctions.h"
#include "execution/expressions/FilterFunctions.h"
#include "execution/OperatorsBase.h"
#include "execution/expressions/ScalarFunctions.h"
#include "execution/ExecutionHelper.h"
#include "column/ColumnFactory.h"

#include <numeric>

// ========================= ScanOperator =========================

ScanOperator::ScanOperator(const std::string& table_path,
                           const std::vector<std::string>& column_names,
                           std::shared_ptr<const MetadataTable> metadata)
    : reader_(table_path, metadata), metadata_(metadata) {
    const auto& full_columns = metadata_->GetColumns();

    for (const std::string& name : column_names) {
        bool found = false;
        for (size_t i = 0; i < full_columns.size(); ++i) {
            if (full_columns[i].name == name) {
                column_indices_.push_back(i);
                found = true;
                break;
            }
        }
        if (!found) {
            THROW_RUNTIME_ERROR("Column " + name + " not found in metadata");
        }
    }

    if (!full_columns.empty()) {
        total_chunks_ = full_columns[0].offsets.size();
    }
}

std::unique_ptr<RecordBatch> ScanOperator::GetData() {
    if (column_indices_.empty()) {
        bool expected = false;
        if (finished_empty_scan_.compare_exchange_strong(expected, true)) {
            auto record_batch = std::make_unique<RecordBatch>();
            record_batch->num_rows = metadata_->GetNumRows();
            return record_batch;
        }
        return nullptr;
    }

    size_t cur_chunk = current_chunk_.fetch_add(1);
    if (cur_chunk >= total_chunks_) {
        return nullptr;
    }

    auto record_batch = std::make_unique<RecordBatch>();
    record_batch->num_rows = 0;

    for (size_t col_idx : column_indices_) {
        std::shared_ptr<Column> column_data = reader_.GetColumnData(col_idx, cur_chunk);
        if (record_batch->num_rows == 0) {
            record_batch->num_rows = column_data->Size();
        }
        record_batch->columns.push_back(std::move(column_data));
    }

    return record_batch;
}

// ========================= FilterTransformOperator =========================

FilterTransformOperator::FilterTransformOperator(std::shared_ptr<FilterFunction> filter_function)
    : filter_function_(std::move(filter_function)) {
}

std::unique_ptr<RecordBatch> FilterTransformOperator::Execute(std::unique_ptr<RecordBatch> batch) {
    std::vector<size_t> selection_vector = filter_function_->Evaluate(*batch);
    if (selection_vector.empty()) {
        return nullptr;
    }
    batch->num_rows = selection_vector.size();
    batch->selection_vector =
        std::make_shared<const std::vector<size_t>>(std::move(selection_vector));
    return batch;
}

// ========================= ScalarTransformOperator =========================

ScalarTransformOperator::ScalarTransformOperator(
    std::vector<std::unique_ptr<ScalarFunction>> scalar_functions)
    : scalar_functions_(std::move(scalar_functions)) {
}

std::unique_ptr<RecordBatch> ScalarTransformOperator::Execute(std::unique_ptr<RecordBatch> batch) {
    for (auto& func : scalar_functions_) {
        batch->columns.push_back(func->Evaluate(*batch));
    }
    return batch;
}

// ========================= DropTransformOperator =========================

DropTransformOperator::DropTransformOperator(std::vector<size_t> column_stay_indices)
    : column_stay_indices_(std::move(column_stay_indices)) {
}

std::unique_ptr<RecordBatch> DropTransformOperator::Execute(std::unique_ptr<RecordBatch> batch) {
    std::vector<std::shared_ptr<Column>> new_columns;
    new_columns.reserve(column_stay_indices_.size());

    for (size_t ind : column_stay_indices_) {
        new_columns.push_back(std::move(batch->columns[ind]));
    }

    batch->columns = std::move(new_columns);
    return batch;
}

// ========================= ReorderTransformOperator =========================

ReorderTransformOperator::ReorderTransformOperator(std::vector<size_t> new_indices)
    : new_indices_(std::move(new_indices)) {
}

std::unique_ptr<RecordBatch> ReorderTransformOperator::Execute(std::unique_ptr<RecordBatch> batch) {
    auto new_batch = std::make_unique<RecordBatch>();
    new_batch->num_rows = batch->num_rows;
    new_batch->selection_vector = batch->selection_vector;

    new_batch->columns.reserve(new_indices_.size());
    for (size_t ind : new_indices_) {
        new_batch->columns.push_back(batch->columns[ind]);
    }

    return new_batch;
}

// ========================= LimitTransformOperator =========================

LimitTransformOperator::LimitTransformOperator(size_t limit) : limit_(limit) {
}

std::unique_ptr<RecordBatch> LimitTransformOperator::Execute(std::unique_ptr<RecordBatch> batch) {
    size_t old_rows = cur_rows_.fetch_add(batch->num_rows);

    if (old_rows >= limit_) {
        done_.store(true, std::memory_order_relaxed);
        return nullptr;
    }

    size_t remaining = limit_ - old_rows;

    if (batch->num_rows <= remaining) {
        if (old_rows + batch->num_rows >= limit_) {
            done_.store(true, std::memory_order_relaxed);
        }
        return batch;
    } else {
        done_.store(true, std::memory_order_relaxed);
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

// ========================= AggregationSinkSourceOperator =========================

AggregationSinkSourceOperator::AggregationSinkSourceOperator(
    std::vector<std::unique_ptr<GlobalAggregationFunction>> aggregation_functions)
    : aggregation_functions_(std::move(aggregation_functions)) {
}

void AggregationSinkSourceOperator::Sink(std::unique_ptr<RecordBatch> batch, size_t thread_id) {
    for (auto& aggregation_function : aggregation_functions_) {
        aggregation_function->Update(*batch, thread_id);
    }
}

void AggregationSinkSourceOperator::Finalize() && {
    result_batch_ = std::make_unique<RecordBatch>();
    result_batch_->num_rows = 1;

    for (auto& aggregation_function : aggregation_functions_) {
        result_batch_->columns.push_back(aggregation_function->Finalize());
    }
}

std::unique_ptr<RecordBatch> AggregationSinkSourceOperator::GetData() {
    bool expected = false;
    if (emitted_.compare_exchange_strong(expected, true)) {
        return std::move(result_batch_);
    }
    return nullptr;
}

// ========================= GroupBySinkSourceOperator =========================

GroupBySinkSourceOperator::GroupBySinkSourceOperator(
    std::vector<size_t> group_by_col_indices, std::vector<ColumnType> key_types,
    std::vector<std::unique_ptr<GroupedAggregationFunction>> agg_funcs, size_t num_threads)
    : group_by_col_indices_(std::move(group_by_col_indices)),
      key_types_(std::move(key_types)),
      agg_funcs_(std::move(agg_funcs)),
      thread_states_(num_threads) {
}

void GroupBySinkSourceOperator::Sink(std::unique_ptr<RecordBatch> batch, size_t thread_id) {
    GroupByThreadState& local = thread_states_[thread_id];

    if (local.key_builders.empty()) {
        for (auto type : key_types_) {
            local.key_builders.push_back(ColumnFactory::MakeColumnBuilder(type));
        }
    }

    std::vector<uint32_t> group_ids;
    size_t batch_size = batch->num_rows;
    group_ids.reserve(batch_size);

    const std::shared_ptr<const std::vector<size_t>>& sel_vec = batch->selection_vector;
    std::vector<std::string> composite_keys(batch_size);
    for (size_t col_ind : group_by_col_indices_) {
        ExecutionHelper::HashKeys(*batch->columns[col_ind], composite_keys, sel_vec.get());
    }

    std::vector<size_t> new_group_indices;
    bool is_filtered = sel_vec != nullptr;

    for (size_t i = 0; i < batch_size; ++i) {
        const std::string& key = composite_keys[i];
        auto it = local.hash_table.find(key);
        uint32_t gid;

        if (it == local.hash_table.end()) {
            gid = local.num_groups++;
            local.hash_table[key] = gid;
            size_t actual_idx = is_filtered ? (*sel_vec)[i] : i;
            new_group_indices.push_back(actual_idx);
        } else {
            gid = it->second;
        }
        group_ids.push_back(gid);
    }

    if (!new_group_indices.empty()) {
        for (size_t k = 0; k < group_by_col_indices_.size(); ++k) {
            ExecutionHelper::CopySelection(*local.key_builders[k],
                                           *batch->columns[group_by_col_indices_[k]],
                                           &new_group_indices);
        }
    }

    for (auto& func : agg_funcs_) {
        func->Resize(local.num_groups, thread_id);
        func->Update(*batch, group_ids, thread_id);
    }
}

void GroupBySinkSourceOperator::Finalize() && {
    GroupByThreadState& global = thread_states_[0];

    if (global.key_builders.empty()) {
        for (auto type : key_types_) {
            global.key_builders.push_back(ColumnFactory::MakeColumnBuilder(type));
        }
    }

    for (size_t t = 1; t < thread_states_.size(); ++t) {
        GroupByThreadState& local = thread_states_[t];
        if (local.num_groups == 0) {
            continue;
        }

        std::vector<std::shared_ptr<Column>> local_key_cols;
        for (std::shared_ptr<ColumnBuilder>& key_builder : local.key_builders) {
            local_key_cols.push_back(key_builder->Finish());
        }

        for (const auto& [key, local_gid] : local.hash_table) {
            auto it = global.hash_table.find(key);
            uint32_t target_global_gid;

            if (it == global.hash_table.end()) {
                target_global_gid = global.num_groups++;
                global.hash_table[key] = target_global_gid;

                std::vector<size_t> single_ind = {local_gid};
                for (size_t k = 0; k < global.key_builders.size(); ++k) {
                    ExecutionHelper::CopySelection(*global.key_builders[k], *local_key_cols[k],
                                                   &single_ind);
                }

                for (auto& func : agg_funcs_) {
                    func->Resize(global.num_groups, 0);
                }
            } else {
                target_global_gid = it->second;
            }

            for (auto& func : agg_funcs_) {
                func->CombineState(t, local_gid, target_global_gid);
            }
        }

        local.hash_table.clear();
    }

    result_batch_ = std::make_unique<RecordBatch>();
    result_batch_->num_rows = global.num_groups;

    for (size_t k = 0; k < global.key_builders.size(); ++k) {
        result_batch_->columns.push_back(global.key_builders[k]->Finish());
    }
    for (size_t a = 0; a < agg_funcs_.size(); ++a) {
        result_batch_->columns.push_back(agg_funcs_[a]->Finalize());
    }
}

std::unique_ptr<RecordBatch> GroupBySinkSourceOperator::GetData() {
    bool expected = false;
    if (emitted_.compare_exchange_strong(expected, true)) {
        return std::move(result_batch_);
    }
    return nullptr;
}

// ========================= OrderBySinkSourceOperator =========================

OrderBySinkSourceOperator::OrderBySinkSourceOperator(
    std::vector<std::pair<size_t, bool>> sort_columns, std::vector<ColumnType> column_types,
    std::optional<size_t> limit, std::optional<size_t> offset, size_t num_threads)
    : sort_columns_(std::move(sort_columns)),
      column_types_(column_types),
      limit_(limit),
      offset_(offset),
      thread_states_(num_threads) {
    total_limit_ = limit_;
    if (total_limit_.has_value()) {
        if (offset_.has_value()) {
            total_limit_.value() += offset_.value();
        }
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

void OrderBySinkSourceOperator::Sink(std::unique_ptr<RecordBatch> batch, size_t thread_id) {
    OrderByThreadState& local = thread_states_[thread_id];

    if (local.column_builders.empty()) {
        for (auto type : column_types_) {
            local.column_builders.push_back(ColumnFactory::MakeColumnBuilder(type));
        }
        local.accum_num_rows = 0;
    }

    const std::shared_ptr<const std::vector<size_t>>& sel = batch->selection_vector;
    for (size_t c = 0; c < local.column_builders.size(); ++c) {
        ExecutionHelper::CopySelection(*local.column_builders[c], *batch->columns[c], sel.get());
    }
    local.accum_num_rows += batch->num_rows;

    if (total_limit_.has_value() && local.accum_num_rows > total_limit_.value() * 10) {
        TrimLocalBatch(thread_id);
    }
}

// TODO: IT DOES NOT USE MULTITHREADING! if we don't have limit in a query, then OrderBy will work
// with a speed of one thread algorithm...
void OrderBySinkSourceOperator::Finalize() && {
    std::vector<std::shared_ptr<ColumnBuilder>> global_builders;
    for (ColumnType type : column_types_) {
        global_builders.push_back(ColumnFactory::MakeColumnBuilder(type));
    }

    size_t num_rows = 0;
    for (size_t t = 0; t < thread_states_.size(); ++t) {
        OrderByThreadState& local = thread_states_[t];
        if (local.accum_num_rows == 0) {
            continue;
        }
        std::vector<std::shared_ptr<Column>> local_cols;
        for (auto& builder : local.column_builders) {
            local_cols.push_back(builder->Finish());
        }

        for (size_t c = 0; c < global_builders.size(); ++c) {
            ExecutionHelper::CopySelection(*global_builders[c], *local_cols[c], nullptr);
        }
        num_rows += local.accum_num_rows;
        local.column_builders.clear();
    }

    if (num_rows == 0) {
        result_batch_ = std::make_unique<RecordBatch>();
        result_batch_->num_rows = 0;
        return;
    }

    auto temp_batch = std::make_unique<RecordBatch>();
    temp_batch->num_rows = num_rows;
    for (auto& builder : global_builders) {
        temp_batch->columns.push_back(builder->Finish());
    }

    std::vector<size_t> indices(num_rows);
    std::iota(indices.begin(), indices.end(), 0);

    std::vector<SortColumnInfo> cols_info;
    cols_info.reserve(sort_columns_.size());
    for (const auto& [col_ind, is_desc] : sort_columns_) {
        const Column* raw_column = temp_batch->columns[col_ind].get();
        auto col_type = raw_column->GetType();
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                      \
    case ColumnType::ENUM_VAL: {                                        \
        const void* data_ptr = raw_column->GetRawData();                \
        cols_info.push_back({data_ptr, &Compare<CLASS_TYPE>, is_desc}); \
        break;                                                          \
    }
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

    if (total_limit_.has_value() && num_rows > total_limit_.value()) {
        uint32_t lim = total_limit_.value();
        std::partial_sort(indices.begin(), indices.begin() + lim, indices.end(), cmp);
        indices.resize(lim);
    } else {
        std::sort(indices.begin(), indices.end(), cmp);
    }

    if (offset_.has_value() && offset_.value() > 0) {
        size_t off = offset_.value();
        if (off >= indices.size()) {
            indices.clear();
        } else {
            indices.erase(indices.begin(), indices.begin() + off);
        }
    }

    result_batch_ = std::make_unique<RecordBatch>();
    result_batch_->num_rows = indices.size();

    std::vector<std::shared_ptr<ColumnBuilder>> final_builders;
    for (auto type : column_types_) {
        final_builders.push_back(ColumnFactory::MakeColumnBuilder(type));
    }

    for (size_t c = 0; c < final_builders.size(); ++c) {
        ExecutionHelper::CopySelection(*final_builders[c], *temp_batch->columns[c], &indices);
        result_batch_->columns.push_back(final_builders[c]->Finish());
    }
}

std::unique_ptr<RecordBatch> OrderBySinkSourceOperator::GetData() {
    bool expected = false;
    if (emitted_.compare_exchange_strong(expected, true)) {
        return std::move(result_batch_);
    }
    return nullptr;
}

void OrderBySinkSourceOperator::TrimLocalBatch(size_t thread_id) {
    OrderByThreadState& local = thread_states_[thread_id];

    if (local.accum_num_rows == 0) {
        return;
    }

    auto temp_batch = std::make_unique<RecordBatch>();
    temp_batch->num_rows = local.accum_num_rows;
    for (auto& builder : local.column_builders) {
        temp_batch->columns.push_back(builder->Finish());
    }
    local.column_builders.clear();

    std::vector<size_t> indices(local.accum_num_rows);
    for (size_t i = 0; i < local.accum_num_rows; ++i) {
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

    if (total_limit_.has_value() && local.accum_num_rows > total_limit_.value()) {
        uint32_t lim = total_limit_.value();
        std::nth_element(indices.begin(), indices.begin() + lim, indices.end(), cmp);
        indices.resize(lim);
    }
    // if (is_final) {
    //     std::sort(indices.begin(), indices.end(), cmp);
    //     if (offset_.has_value()) {
    //         if (offset_.value() < indices.size()) {
    //             indices.erase(indices.begin(), indices.begin() + offset_.value());
    //         } else {
    //             indices.clear();
    //         }
    //     }
    // }

    for (auto type : column_types_) {
        local.column_builders.push_back(ColumnFactory::MakeColumnBuilder(type));
    }
    for (size_t c = 0; c < local.column_builders.size(); ++c) {
        ExecutionHelper::CopySelection(*local.column_builders[c], *temp_batch->columns[c],
                                       &indices);
    }
    local.accum_num_rows = indices.size();
}
