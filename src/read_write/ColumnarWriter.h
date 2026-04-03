//
// Created by ragnarokk on 05.01.2026.
//

#ifndef COLUMNAR_ENGINE_COLUMNARWRITER_H
#define COLUMNAR_ENGINE_COLUMNARWRITER_H

#include <fstream>
#include <memory>

#include "Types.h"

class ColumnarWriter {
public:
    ColumnarWriter(const std::string& file_path);
    ~ColumnarWriter();

    void WriteHeader(const VectorOfStrings2D& column_names,
                     const std::vector<ColumnType>& column_types);
    void WriteBatch(const std::vector<std::unique_ptr<Column>>& columns);

    // magic, columns by batches, number of columns, name, type, offsets, sizes, number of rows,
    // metadata_start, magic
    void Finalize();

private:
    std::ofstream file_;
    std::vector<ColumnMetadata> metadata_;
    uint64_t current_offset_ = 0;
    uint32_t num_rows_ = 0;
    bool header_written_ = false;
};

#endif  // COLUMNAR_ENGINE_COLUMNARWRITER_H