//
// Created by ragnarokk on 05.01.2026.
//

#ifndef COLUMNAR_ENGINE_RAWCSVREADER_H
#define COLUMNAR_ENGINE_RAWCSVREADER_H

#include "CSVTokenizer.h"

#include <vector>

class RawCSVReader {
public:
    RawCSVReader(const std::string& file_path, char delim = ',');

    std::vector<std::string> ReadHeader();

    bool ReadRow(std::vector<std::string>& row);

private:
    CSVTokenizer tokenizer_;
    size_t expected_columns_ = 0;
};

#endif  // COLUMNAR_ENGINE_RAWCSVREADER_H