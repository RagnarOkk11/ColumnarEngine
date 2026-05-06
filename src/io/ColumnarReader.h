#pragma once

#include "types/ColumnType.h"
#include "column/Column.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

class ColumnarReader {
public:
    explicit ColumnarReader(const std::string& file_name);
    
    void ReadRawColumnData(size_t column_index, size_t chunk_index, std::vector<char>& buffer) const;

    std::shared_ptr<Column> GetColumnData(size_t column_index, size_t chunk_index) const;

    const std::vector<ColumnMetadata>& GetMetadata() const;

    ColumnType GetColumnTypeByName(const std::string& name) const;

private:
    void ValidateMagicAndGetFileSize(std::streamoff& file_size);
    void ReadMetadata();

    mutable std::ifstream file_;
    std::vector<ColumnMetadata> metadata_;
};
