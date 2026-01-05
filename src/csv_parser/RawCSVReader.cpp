//
// Created by ragnarokk on 05.01.2026.
//

#include "RawCSVReader.h"

RawCSVReader::RawCSVReader(const std::string& file_path, char delim)
    : tokenizer_(file_path, delim) {
}

std::vector<std::string> RawCSVReader::ReadHeader() {
    std::vector<std::string> header;
    while (!tokenizer_.IsEndOfLine() && !tokenizer_.IsEOF()) {
        header.push_back(tokenizer_.GetNextToken());
    }
    tokenizer_.ResetLineFlag();
    expected_columns_ = header.size();
    return header;
}

bool RawCSVReader::ReadRow(std::vector<std::string>& row) {
    if (tokenizer_.IsEOF()) {
        return false;
    }
    row.clear();
    if (expected_columns_ > 0) {
        row.reserve(expected_columns_);
    }
    while (!tokenizer_.IsEndOfLine() && !tokenizer_.IsEOF()) {
        std::string token = tokenizer_.GetNextToken();
        if (row.empty() && token.empty() && tokenizer_.IsEOF()) {
            break;
        }
        row.push_back(token);
    }
    if (row.empty() && tokenizer_.IsEOF()) {
        return false;
    }
    tokenizer_.ResetLineFlag();
    return true;
}
