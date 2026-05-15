#pragma once

#include "types/ColumnType.h"
#include "column/Column.h"

#include "utils/VectorOfStrings.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

class ColumnarWriter {
public:
    explicit ColumnarWriter(const std::string& file_path);
    
    void WriteHeader(const VectorOfStrings& column_names,
                     const std::vector<ColumnType>& column_types);

    void WriteBatch(const std::vector<std::shared_ptr<Column>>& columns);

    // File structure:
    // - magic "TUFF"
    // - columns by batches
    // - metadata:
    //   - number of columns
    //   - for each column:
    //     - name length
    //     - name
    //     - type
    //     - number of chunks
    //     - for each chunk: offset, size
    // - number of rows
    // - metadata start offset
    // - magic "TUFF"
    void Finalize() &&;

private:
    void WriteMetadata();

    std::ofstream file_;
    std::vector<ColumnMetadata> metadata_;
    bool header_written_ = false;
    uint64_t current_offset_ = 0;
    uint64_t num_rows_ = 0;
};