//
// Created by ragnarokk on 02.01.2026.
//

#include <array>

#include "ColumnarReader.h"

ColumnarReader::ColumnarReader(const std::string& file_name) : file_(file_name) {
    if (!file_.is_open()) {
        throw std::runtime_error("Could not open file for reading");
    }
    file_.seekg(0, std::ios::end);
    const std::streamoff file_size = file_.tellg();
    if (file_size < 16) {
        throw std::runtime_error("File is too small to contain a valid footer");
    }

    file_.seekg(-4, std::ios::end);
    std::array<char, 4> magic{};
    file_.read(magic.data(), magic.size());
    if (!file_ || std::string(magic.data(), magic.size()) != "TUFF") {
        throw std::runtime_error("Invalid file footer magic");
    }

    file_.seekg(-12, std::ios::end);
    uint64_t metadata_start;
    file_.read(reinterpret_cast<char*>(&metadata_start), sizeof(metadata_start));
    if (!file_) {
        throw std::runtime_error("Failed to read metadata start offset");
    }
    if (metadata_start >= static_cast<uint64_t>(file_size)) {
        throw std::runtime_error("Corrupted metadata offset");
    }

    file_.seekg(metadata_start, std::ios::beg);

    uint32_t num_cols;
    file_.read(reinterpret_cast<char*>(&num_cols), sizeof(num_cols));
    if (!file_) {
        throw std::runtime_error("Failed to read number of columns");
    }

    for (uint32_t i = 0; i < num_cols; ++i) {
        ColumnMetadata column_meta;
        uint32_t name_length;
        file_.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));
        if (!file_) {
            throw std::runtime_error("Failed to read column name length");
        }
        column_meta.name.resize(name_length);
        if (name_length > 0) {
            file_.read(&column_meta.name[0], name_length);
            if (!file_) {
                throw std::runtime_error("Failed to read column name");
            }
        }

        uint8_t type;
        file_.read(reinterpret_cast<char*>(&type), sizeof(type));
        if (!file_) {
            throw std::runtime_error("Failed to read column type");
        }
        column_meta.type = static_cast<ColumnType>(type);

        uint32_t num_chunks;
        file_.read(reinterpret_cast<char*>(&num_chunks), sizeof(num_chunks));
        if (!file_) {
            throw std::runtime_error("Failed to read number of chunks");
        }
        for (uint32_t j = 0; j < num_chunks; ++j) {
            uint64_t offset, size;
            file_.read(reinterpret_cast<char*>(&offset), sizeof(offset));
            file_.read(reinterpret_cast<char*>(&size), sizeof(size));
            if (!file_) {
                throw std::runtime_error("Failed to read chunk metadata");
            }
            column_meta.offsets.push_back(offset);
            column_meta.sizes.push_back(size);
        }

        metadata_.push_back(std::move(column_meta));
    }
}

ColumnarReader::~ColumnarReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

std::vector<char> ColumnarReader::GetRawColumnData(size_t column_index, size_t chunk_index) {
    std::vector<char> buffer;
    if (column_index >= metadata_.size()) {
        THROW_RUNTIME_ERROR("Column index out of range");
    }
    if (chunk_index >= metadata_[column_index].offsets.size()) {
        THROW_RUNTIME_ERROR("Chunk index out of range");
    }
    uint64_t offset = metadata_[column_index].offsets[chunk_index];
    uint64_t size = metadata_[column_index].sizes[chunk_index];

    buffer.resize(size);
    file_.seekg(offset, std::ios::beg);
    file_.read(buffer.data(), size);
    return buffer;
}

std::shared_ptr<Column> ColumnarReader::GetColumnData(size_t column_index, size_t chunk_index) {
    std::vector<char> raw_data = GetRawColumnData(column_index, chunk_index);
    ColumnType type = metadata_[column_index].type;
    std::shared_ptr<Column> column;

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
    if (type == ColumnType::ENUM_VAL) {            \
        column = std::make_shared<CLASS_TYPE>();   \
    } else

    FOR_EACH_COLUMN_TYPE(HANDLE_TYPE) {
        THROW_RUNTIME_ERROR("Unknown column type");
    }
#undef HANDLE_TYPE

    column->ReadFromRawData(raw_data);
    return column;
}

const std::vector<ColumnMetadata>& ColumnarReader::GetMetadata() const {
    return metadata_;
}

ColumnType ColumnarReader::GetColumnTypeByName(const std::string& name) const {
    for (const auto& meta : metadata_) {
        if (meta.name == name) {
            return meta.type;
        }
    }
    THROW_RUNTIME_ERROR("Column " + name + " not found in metadata");
}
