//
// Created by ragnarokk on 02.01.2026.
//

#ifndef COLUMNAR_ENGINE_READER_H
#define COLUMNAR_ENGINE_READER_H

#include <cstdint>
#include <fstream>
#include <memory>

#include "../object.h"

class Reader {
public:
    Reader(const std::string &file_name);

    ~Reader();

    std::vector<char> GetRawColumnData(size_t column_index, size_t chunk_index);

    std::unique_ptr<Column> GetColumnData(size_t column_index, size_t chunk_index);

    const std::vector<ColumnMetadata> &GetMetadata() const;

private:
    std::ifstream file_;
    std::vector<ColumnMetadata> metadata_;
};


#endif //COLUMNAR_ENGINE_READER_H
