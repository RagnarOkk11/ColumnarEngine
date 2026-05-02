//
// Created by ragnarokk on 05.01.2026.
//

#include "ColumnarWriter.h"

ColumnarWriter::ColumnarWriter(const std::string& file_path) {
    file_.open(file_path, std::ios::binary);
    if (!file_.is_open()) {
        throw std::runtime_error("Could not open file for writing");
    }
}

ColumnarWriter::~ColumnarWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

void ColumnarWriter::WriteHeader(const VectorOfStrings2D& column_names,
                                 const std::vector<ColumnType>& column_types) {
    if (header_written_) {
        throw std::runtime_error("Header already written");
    }

    file_.write("TUFF", 4);
    current_offset_ = 4;

    for (size_t i = 0; i < column_names.Size(); ++i) {
        ColumnMetadata column_meta;
        column_meta.name = column_names.GetString(i);
        column_meta.type = column_types[i];
        metadata_.push_back(column_meta);
    }

    header_written_ = true;
}

void ColumnarWriter::WriteBatch(const std::vector<std::shared_ptr<Column>>& columns) {
    if (!header_written_) {
        throw std::runtime_error("Header must be written before writing batches");
    }
    if (columns.size() != metadata_.size()) {
        throw std::runtime_error("Number of columns does not match metadata");
    }
    for (size_t i = 0; i < columns.size(); ++i) {
        uint64_t offset = current_offset_;
        uint64_t size = columns[i]->WriteToFile(file_);
        metadata_[i].offsets.push_back(offset);
        metadata_[i].sizes.push_back(size);
        current_offset_ += size;
    }
    if (!columns.empty()) {
        num_rows_ += columns[0]->Size();
    }
}

void ColumnarWriter::Finalize() {
    if (!header_written_) {
        throw std::runtime_error("Header must be written before finalizing");
    }

    uint64_t metadata_start = current_offset_;

    uint32_t num_columns = metadata_.size();
    file_.write(reinterpret_cast<const char*>(&num_columns), sizeof(num_columns));

    for (const auto& meta : metadata_) {
        uint32_t name_length = meta.name.size();
        file_.write(reinterpret_cast<const char*>(&name_length), sizeof(name_length));
        file_.write(meta.name.data(), name_length);

        uint8_t type = static_cast<uint8_t>(meta.type);
        file_.write(reinterpret_cast<const char*>(&type), sizeof(type));

        uint32_t num_batches = meta.offsets.size();
        file_.write(reinterpret_cast<const char*>(&num_batches), sizeof(num_batches));
        for (size_t i = 0; i < num_batches; ++i) {
            file_.write(reinterpret_cast<const char*>(&meta.offsets[i]), sizeof(uint64_t));
            file_.write(reinterpret_cast<const char*>(&meta.sizes[i]), sizeof(uint64_t));
        }
    }

    file_.write(reinterpret_cast<const char*>(&num_rows_), sizeof(num_rows_));

    file_.write(reinterpret_cast<const char*>(&metadata_start), sizeof(metadata_start));
    file_.write("TUFF", 4);
}
