#pragma once

#include "execution/OperatorsBase.h"
#include "column/Column.h"
#include "column/NumericColumn.h"
#include "column/StringColumn.h"
#include "column/CharColumn.h"
#include "column/TemporalColumn.h"
#include "utils/Macro.h"

#include <algorithm>
#include <vector>
#include <string>
#include <memory>

class ExecutionHelper {
public:
    static std::string FormatValue(const Column& col, size_t index) {
        ColumnType type = col.GetType();
        if (type == ColumnType::STRING) {
            return std::string(static_cast<const StringColumn&>(col).GetData()[index]);
        } else if (type == ColumnType::CHAR) {
            return std::string(1, static_cast<const CharColumn&>(col).GetData()[index]);
        } else if (type == ColumnType::DATE) {
            return Date::Format(static_cast<const DateColumn&>(col).GetData()[index]);
        } else if (type == ColumnType::TIMESTAMP) {
            return Timestamp::Format(static_cast<const TimestampColumn&>(col).GetData()[index]);
        } else {
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                       \
    case ColumnType::ENUM_VAL: {                                         \
        auto val = static_cast<const CLASS_TYPE&>(col).GetData()[index]; \
        return std::to_string(val);                                      \
    }

            switch (type) {
                HANDLE_TYPE(INT16, "INT16", Int16Column)
                HANDLE_TYPE(INT32, "INT32", Int32Column)
                HANDLE_TYPE(INT64, "INT64", Int64Column)
                HANDLE_TYPE(FLOAT, "FLOAT", FloatColumn)
                HANDLE_TYPE(DOUBLE, "DOUBLE", DoubleColumn)
                HANDLE_TYPE(LONGDOUBLE, "LONGDOUBLE", LongDoubleColumn)
                case ColumnType::INT128: {
                    Int128 val = static_cast<const Int128Column&>(col).GetData()[index];
                    if (val == 0) {
                        return "0";
                    }
                    std::string res;
                    bool neg = val < 0;
                    if (neg) {
                        val = -val;
                    }
                    while (val > 0) {
                        res += std::to_string(static_cast<int>(val % 10));
                        val /= 10;
                    }
                    if (neg) {
                        res += "-";
                    }
                    std::reverse(res.begin(), res.end());
                    return res;
                }
                default: return "";
            }
#undef HANDLE_TYPE
        }
    }
    static void HashKeys(const Column& col, std::vector<std::string>& keys,
                         const std::vector<size_t>* selection = nullptr) {
        ColumnType type = col.GetType();

        if (type == ColumnType::STRING) {
            auto* str_col = static_cast<const StringColumn*>(&col);
            const auto& data = str_col->GetData();
            size_t n = selection ? selection->size() : data.Size();
            if (keys.capacity() < n) {
                keys.reserve(n);
            }
            for (size_t i = 0; i < n; ++i) {
                size_t idx = selection ? (*selection)[i] : i;
                std::string_view val = data[idx];
                size_t len = val.size();
                keys[i].append(reinterpret_cast<const char*>(&len), sizeof(len));
                keys[i].append(val.data(), val.size());
            }
        } else if (type == ColumnType::CHAR) {
            auto* char_col = static_cast<const CharColumn*>(&col);
            const auto& data = char_col->GetData();
            size_t n = selection ? selection->size() : data.size();
            if (keys.capacity() < n) {
                keys.reserve(n);
            }
            for (size_t i = 0; i < n; ++i) {
                size_t idx = selection ? (*selection)[i] : i;
                keys[i].push_back(data[idx]);
            }
        } else {
            size_t elem_size = 0;
            const void* raw_data = col.GetRawData();

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                                      \
    case ColumnType::ENUM_VAL:                                                          \
        elem_size = sizeof(CLASS_TYPE::ValueType);                                      \
        {                                                                               \
            auto* vec = static_cast<const CLASS_TYPE::ContainerType*>(raw_data);        \
            size_t n = selection ? selection->size() : vec->size();                     \
            if (keys.capacity() < n)                                                    \
                keys.reserve(n);                                                        \
            for (size_t i = 0; i < n; ++i) {                                            \
                size_t idx = selection ? (*selection)[i] : i;                           \
                keys[i].append(reinterpret_cast<const char*>(&(*vec)[idx]), elem_size); \
            }                                                                           \
        }                                                                               \
        break;

            switch (type) {
                FOR_NUMERIC_COLUMN_TYPE(HANDLE_TYPE)
                default: break;
            }
#undef HANDLE_TYPE
        }
    }

    static void CopySelection(ColumnBuilder& dest, const Column& src,
                              const std::vector<size_t>* indices = nullptr) {
        ColumnType type = src.GetType();

        if (type == ColumnType::STRING) {
            auto& d = static_cast<StringColumnBuilder&>(dest);
            const auto& s = static_cast<const StringColumn&>(src).GetData();
            if (!indices) {
                for (size_t i = 0; i < s.Size(); ++i) {
                    d.AddValue(s[i]);
                }
            } else {
                for (size_t idx : *indices) {
                    d.AddValue(s[idx]);
                }
            }
        } else {
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                                   \
    case ColumnType::ENUM_VAL: {                                                     \
        auto& d = static_cast<CLASS_TYPE##Builder&>(dest);                           \
        auto* vec = static_cast<const CLASS_TYPE::ContainerType*>(src.GetRawData()); \
        if (!indices) {                                                              \
            for (size_t i = 0; i < vec->size(); ++i) {                               \
                d.AddValue((*vec)[i]);                                               \
            }                                                                        \
        } else {                                                                     \
            for (size_t idx : *indices) {                                            \
                d.AddValue((*vec)[idx]);                                             \
            }                                                                        \
        }                                                                            \
        break;                                                                       \
    }

            switch (type) {
                FOR_NUMERIC_COLUMN_TYPE(HANDLE_TYPE)
                default: break;
            }
#undef HANDLE_TYPE
        }
    }
};
