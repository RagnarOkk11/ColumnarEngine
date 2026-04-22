//
// Created by ragnarokk on 22.04.2026.
//

#ifndef COLUMNAR_ENGINE_FILTERFUNCTIONS_H
#define COLUMNAR_ENGINE_FILTERFUNCTIONS_H

#include "Schema.h"

class FilterFunction {
public:
    virtual ~FilterFunction() = default;
    virtual std::vector<uint32_t> Evaluate(const RecordBatch& batch) = 0;
};

template <typename ColumnType, typename ValueType>
class NotEqFilterFunction : public FilterFunction {
public:
    NotEqFilterFunction(size_t column_ind, ValueType target_val)
        : column_ind_(column_ind), target_val_(target_val) {
    }

    std::vector<uint32_t> Evaluate(const RecordBatch& batch) override {
        if constexpr (std::is_same_v<ColumnType, StringColumn> ||
            std::is_same_v<ColumnType, DateColumn> ||
            std::is_same_v<ColumnType, TimestampColumn>) {
            // TODO: add GetData() to this columns
            THROW_NOT_IMPLEMENTED;
        } else {
            auto* column = static_cast<ColumnType*>(batch.columns[column_ind_].get());
            const auto& data = column->GetData();

            std::vector<uint32_t> selection_vector;
            selection_vector.reserve(batch.num_rows);

            if (!batch.selection_vector.empty()) {
                for (uint32_t ind : batch.selection_vector) {
                    if (data[ind] != target_val_) {
                        selection_vector.push_back(ind);
                    }
                }
            } else {
                for (size_t i = 0; i < batch.num_rows; ++i) {
                    if (data[i] != target_val_) {
                        selection_vector.push_back(i);
                    }
                }
            }
            return selection_vector;
        }
    }

private:
    size_t column_ind_;
    ValueType target_val_;
};

#endif  // COLUMNAR_ENGINE_FILTERFUNCTIONS_H
