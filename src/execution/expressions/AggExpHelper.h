#pragma once

#include "Macro.h"
#include "execution/OperatorsBase.h"

class AggExpHelper {
public:
    template <typename ColumnType, typename Func>
    static void IterateColumnData(const RecordBatch& batch, size_t column_index, Func&& func) {
        auto* column = static_cast<const ColumnType*>(batch.columns[column_index].get());
        const auto& data = column->GetData();

        if (!batch.selection_vector) {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                func(data[i], i, i);
            }
        } else {
            const auto& sel = *batch.selection_vector;
            for (size_t i = 0; i < batch.num_rows; ++i) {
                func(data[sel[i]], i, sel[i]);
            }
        }
    }

    template <typename Functor>
    static auto DispatchNumeric(ColumnType type, Functor&& f) {
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
        case ColumnType::ENUM_VAL: return f.template operator()<CLASS_TYPE>(ColumnType::ENUM_VAL);

        switch (type) {
            FOR_NUMERIC_COLUMN_TYPE(HANDLE_TYPE)
            default: THROW_NOT_IMPLEMENTED;
        }
#undef HANDLE_TYPE
    }

    template <typename Functor>
    static auto DispatchAll(ColumnType type, Functor&& f) {
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
        case ColumnType::ENUM_VAL: return f.template operator()<CLASS_TYPE>(ColumnType::ENUM_VAL);

        switch (type) {
            FOR_EACH_COLUMN_TYPE(HANDLE_TYPE)
            default: THROW_NOT_IMPLEMENTED;
        }
#undef HANDLE_TYPE
    }
};