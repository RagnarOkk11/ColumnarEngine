//
// Created by ragnarokk on 02.01.2026.
//

#include "reader.h"

Reader::Reader(const std::string& file_name) : file_(file_name) {
    if (!file_.is_open()) {
        throw std::runtime_error("Could not open file for reading");
    }
    file_.seekg(-14, std::ios::end);
    uint64_t metadata_start;
    file_.read(reinterpret_cast<char*>(&metadata_start), sizeof(metadata_start));
    file_.seekg(metadata_start, std::ios::beg);

    uint32_t num_cols;
    file_.read(reinterpret_cast<char*>(&num_cols), sizeof(num_cols));

    for (uint32_t i = 0; i < num_cols; ++i) {
        ColumnMetadata column_meta;

        uint32_t name_length;
        file_.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));
        column_meta.name.resize(name_length);
        file_.read(&column_meta.name[0], name_length);

        uint8_t type;
        file_.read(reinterpret_cast<char*>(&type), sizeof(type));
        column_meta.type = static_cast<ColumnType>(type);

        uint32_t num_chunks;
        file_.read(reinterpret_cast<char*>(&num_chunks), sizeof(num_chunks));
        for (uint32_t j = 0; j < num_chunks; ++j) {
            uint64_t offset, size;
            file_.read(reinterpret_cast<char*>(&offset), sizeof(offset));
            file_.read(reinterpret_cast<char*>(&size), sizeof(size));
            column_meta.offsets.push_back(offset);
            column_meta.sizes.push_back(size);
        }

        metadata_.push_back(column_meta);
    }
}

Reader::~Reader() {
    if (file_.is_open()) {
        file_.close();
    }
}

std::vector<char> Reader::GetRawColumnData(size_t column_index, size_t chunk_index) {
    std::vector<char> buffer;
    if (chunk_index >= metadata_[column_index].offsets.size()) {
        throw std::out_of_range("Chunk index out of range");
    }
    uint64_t offset = metadata_[column_index].offsets[chunk_index];
    uint64_t size = metadata_[column_index].sizes[chunk_index];

    buffer.resize(size);
    file_.seekg(offset, std::ios::beg);
    file_.read(buffer.data(), size);
    return buffer;
}

std::unique_ptr<Column> Reader::GetColumnData(size_t column_index, size_t chunk_index) {
    std::vector<char> raw_data = GetRawColumnData(column_index, chunk_index);
    ColumnType type = metadata_[column_index].type;
    std::unique_ptr<Column> column;
    switch (type) {
        case ColumnType::INT32:
            column = std::make_unique<Int32Column>();
            break;
        case ColumnType::FLOAT:
            column = std::make_unique<FloatColumn>();
            break;
        case ColumnType::STRING:
            column = std::make_unique<StringColumn>();
            break;
        case ColumnType::DATE:
            column = std::make_unique<DateColumn>();
            break;
        case ColumnType::TIMESTAMP:
            column = std::make_unique<TimestampColumn>();
            break;
        default:
            throw std::runtime_error("Unknown column type");
    }
    column->ReadFromRawData(raw_data);
    return column;
}

const std::vector<ColumnMetadata>& Reader::GetMetadata() const {
    return metadata_;
}
