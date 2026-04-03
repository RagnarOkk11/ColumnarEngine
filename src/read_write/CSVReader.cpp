//
// Created by ragnarokk on 05.01.2026.
//

#include "CSVReader.h"
#include "macro.h"

#include <string>

CSVReader::CSVReader(const std::string& file_path, char delim)
    : tokenizer_(file_path, delim) {
}

void CSVReader::ReadHeader(VectorOfStrings2D& header) {
    header.Clear();
    tokenizer_.GetNextRow(header);
    tokenizer_.ResetLineFlag();
    expected_columns_ = header.Width();
}

bool CSVReader::ReadRow(VectorOfStrings2D& row) {
    if (tokenizer_.IsEOF()) {
        return false;
    }

    tokenizer_.GetNextRow(row);

    if (row.ColumnsInCurrentLine() == 0 && tokenizer_.IsEOF()) {
        return false;
    }

    if (expected_columns_ != 0 && row.ColumnsInCurrentLine() != expected_columns_) {
        THROW_RUNTIME_ERROR("CSV row width mismatch: expected " + std::to_string(expected_columns_) +
                            ", got " + std::to_string(row.ColumnsInCurrentLine()) + " row: " + std::to_string(row.Height()));
    }

    tokenizer_.ResetLineFlag();
    return true;
}
