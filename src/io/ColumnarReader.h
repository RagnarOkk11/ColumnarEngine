#pragma once

#include "types/ColumnType.h"
#include "column/Column.h"

#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class MetadataTable {
public:
    explicit MetadataTable(const std::string& file_path) {
        ParseMetadata(file_path);
    }

    const std::vector<ColumnMetadata>& GetColumns() const {
        return column_meta_;
    }
    uint64_t GetNumRows() const {
        return num_rows_;
    }
    size_t GetNumColumns() const {
        return column_meta_.size();
    }
    const ColumnMetadata& operator[](size_t ind) const {
        return column_meta_[ind];
    }

private:
    void ParseMetadata(const std::string& file_path);

    std::vector<ColumnMetadata> column_meta_;
    uint64_t num_rows_ = 0;
};

class ColumnarReader {
public:
    ColumnarReader(const std::string& file_name, std::shared_ptr<const MetadataTable> meta);

    uint64_t GetNumRows() const {
        return metadata_->GetNumRows();
    }

    ColumnType GetColumnTypeByName(const std::string& name) const;
    void ReadRawColumnData(size_t column_index, size_t chunk_index,
                           std::vector<char>& buffer) const;
    std::shared_ptr<Column> GetColumnData(size_t column_index, size_t chunk_index) const;

private:
    mutable std::ifstream file_;
    std::shared_ptr<const MetadataTable> metadata_;
};
