//
// Created by ragnarokk on 02.01.2026.
//

#ifndef COLUMNAR_ENGINE_READER_H
#define COLUMNAR_ENGINE_READER_H

#include <cstdint>
#include <fstream>
#include <memory>

#include "Types.h"

class ColumnarReader {
public:
    ColumnarReader(const std::string& file_name);
    ~ColumnarReader();

    std::vector<char> GetRawColumnData(size_t column_index, size_t chunk_index);

    std::shared_ptr<Column> GetColumnData(size_t column_index, size_t chunk_index);

    const std::vector<ColumnMetadata>& GetMetadata() const;

    ColumnType GetColumnTypeByName(const std::string& name) const;

private:
    std::ifstream file_;
    std::vector<ColumnMetadata> metadata_;
};

#endif  // COLUMNAR_ENGINE_READER_H
