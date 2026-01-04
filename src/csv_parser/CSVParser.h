//
// Created by ragnarokk on 03.01.2026.
//

#ifndef COLUMNAR_ENGINE_CSVPARSER_H
#define COLUMNAR_ENGINE_CSVPARSER_H

#include <memory>
#include <string>
#include <vector>

#include "CSVTokenizer.h"

class CSVParser {
public:
    CSVParser(const std::string &file_path, char delim = ',');

    CSVParser(const CSVParser &other) = delete;

    CSVParser &operator=(const CSVParser &other) = delete;

    std::pair<std::vector<std::string>, std::vector<std::unique_ptr<Column>>> CreateColumnStructure();

    bool ReadNextBatch(std::vector<std::unique_ptr<Column>> &columns, size_t batch_size = 10000);

private:
    CSVTokenizer tokenizer_;
};


#endif //COLUMNAR_ENGINE_CSVPARSER_H
