#include "io/CsvReader.h"
#include "utils/Macro.h"

#include <string>
#include <string_view>

CsvReader::CsvReader(const std::string& file_path, char delim) : tokenizer_(file_path, delim) {
}

void CsvReader::ReadHeader(VectorOfStrings& header_names, std::vector<ColumnType>& header_types) {
    VectorOfStrings2D raw_header;
    tokenizer_.GetNextRow(raw_header);
    tokenizer_.ResetLineFlag();
    expected_columns_ = raw_header.Width();

    header_names.Clear();
    header_types.clear();

    for (size_t i = 0; i < expected_columns_; ++i) {
        std::string_view token = raw_header.GetString(i);
        size_t colon_pos = token.find(':');
        if (colon_pos == std::string_view::npos) {
            THROW_RUNTIME_ERROR("Invalid header token: " + std::string(token));
        }
        std::string_view type_str = token.substr(0, colon_pos);
        std::string_view name_str = token.substr(colon_pos + 1);

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)    \
    if (type_str == STR_VAL) {                        \
        header_names.PushBack(name_str);              \
        header_types.push_back(ColumnType::ENUM_VAL); \
    } else

        FOR_EACH_COLUMN_TYPE(HANDLE_TYPE) {
            THROW_RUNTIME_ERROR("Invalid header token: " + std::string(token));
        }
#undef HANDLE_TYPE
    }
}

bool CsvReader::ReadRow(VectorOfStrings2D& row) {
    if (tokenizer_.IsEOF()) {
        return false;
    }

    tokenizer_.GetNextRow(row);

    if (row.ColumnsInCurrentLine() == 0 && tokenizer_.IsEOF()) {
        return false;
    }

    if (expected_columns_ != 0 && row.ColumnsInCurrentLine() != expected_columns_) {
        THROW_RUNTIME_ERROR(
            "CSV row width mismatch: expected " + std::to_string(expected_columns_) + ", got " +
            std::to_string(row.ColumnsInCurrentLine()) + " row: " + std::to_string(row.Height()));
    }

    tokenizer_.ResetLineFlag();
    return true;
}
