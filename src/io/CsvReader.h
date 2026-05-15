#pragma once

#include "types/ColumnType.h"
#include "io/CsvTokenizer.h"
#include "utils/VectorOfStrings.h"

#include <vector>
#include <string>

class CsvReader {
public:
    CsvReader(const std::string& file_path, char delim = ',');

    void ReadHeader(VectorOfStrings& header_names, std::vector<ColumnType>& header_types);
    bool ReadRow(VectorOfStrings2D& row);

private:
    CsvTokenizer tokenizer_;
    size_t expected_columns_ = 0;
};
