//
// Created by ragnarokk on 05.01.2026.
//

#ifndef COLUMNAR_ENGINE_RAWCSVREADER_H
#define COLUMNAR_ENGINE_RAWCSVREADER_H

#include "CSVTokenizer.h"
#include "VectorOfStrings.h"

#include <vector>

class CSVReader {
public:
    CSVReader(const std::string& file_path, char delim = ',');

    void ReadHeader(VectorOfStrings2D& header);
    bool ReadRow(VectorOfStrings2D& row);

private:
    CSVTokenizer tokenizer_;
    size_t expected_columns_ = 0;
};

#endif  // COLUMNAR_ENGINE_RAWCSVREADER_H