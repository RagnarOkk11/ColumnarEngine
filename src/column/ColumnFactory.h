#pragma once

#include "column/ColumnBuilder.h"
#include "types/ColumnType.h"
#include "utils/Macro.h"

#include <memory>

class ColumnFactory {
public:
    static std::shared_ptr<Column> MakeColumn(ColumnType type) {
        switch (type) {
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
            case ColumnType::ENUM_VAL: return std::make_shared<CLASS_TYPE>();

            FOR_EACH_COLUMN_TYPE(HANDLE_TYPE)
#undef HANDLE_TYPE
            default:
                THROW_RUNTIME_ERROR("Unknown column type");
        }
    }

    static std::shared_ptr<ColumnBuilder> MakeColumnBuilder(ColumnType type) {
        switch (type) {
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
            case ColumnType::ENUM_VAL: return std::make_shared<CLASS_TYPE##Builder>();

            FOR_EACH_COLUMN_TYPE(HANDLE_TYPE)
#undef HANDLE_TYPE
            default:
                THROW_RUNTIME_ERROR("Unknown column type");
        }
    }
};
