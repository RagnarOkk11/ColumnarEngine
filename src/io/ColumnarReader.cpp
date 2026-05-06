#include "column/ColumnFactory.h"
#include "io/ColumnarReader.h"

#include "utils/Assert.h"
#include "utils/Macro.h"

#include <array>

ColumnarReader::ColumnarReader(const std::string& file_name) : file_(file_name, std::ios::binary) {
    if (!file_.is_open()) {
        THROW_RUNTIME_ERROR("Could not open file for reading");
    }
    std::streamoff file_size;
    ValidateMagicAndGetFileSize(file_size);
    ReadMetadata();
}

void ColumnarReader::ValidateMagicAndGetFileSize(std::streamoff& file_size) {
    file_.seekg(0, std::ios::end);
    file_size = file_.tellg();
    if (file_size < 8) {
        THROW_RUNTIME_ERROR("File is too small to contain a valid footer");
    }

    file_.seekg(-4, std::ios::end);
    std::array<char, 4> magic{};
    file_.read(magic.data(), magic.size());
    if (!file_ || std::string(magic.data(), magic.size()) != "TUFF") {
        THROW_RUNTIME_ERROR("Invalid file footer magic");
    }
}

void ColumnarReader::ReadMetadata() {
    file_.seekg(-12, std::ios::end);
    uint64_t metadata_start;
    file_.read(reinterpret_cast<char*>(&metadata_start), sizeof(metadata_start));
    if (!file_) {
        THROW_RUNTIME_ERROR("Failed to read metadata start offset");
    }

    file_.seekg(metadata_start, std::ios::beg);

    uint32_t num_cols;
    file_.read(reinterpret_cast<char*>(&num_cols), sizeof(num_cols));
    if (!file_) {
        THROW_RUNTIME_ERROR("Failed to read number of columns");
    }

    for (uint32_t i = 0; i < num_cols; ++i) {
        ColumnMetadata column_meta;
        uint32_t name_length;
        file_.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));
        if (!file_) {
            THROW_RUNTIME_ERROR("Failed to read column name length");
        }
        column_meta.name.resize(name_length);
        if (name_length > 0) {
            file_.read(&column_meta.name[0], name_length);
            if (!file_) {
                THROW_RUNTIME_ERROR("Failed to read column name");
            }
        }

        uint8_t type;
        file_.read(reinterpret_cast<char*>(&type), sizeof(type));
        if (!file_) {
            THROW_RUNTIME_ERROR("Failed to read column type");
        }
        column_meta.type = static_cast<ColumnType>(type);

        uint32_t num_chunks;
        file_.read(reinterpret_cast<char*>(&num_chunks), sizeof(num_chunks));
        if (!file_) {
            THROW_RUNTIME_ERROR("Failed to read number of chunks");
        }
        for (uint32_t j = 0; j < num_chunks; ++j) {
            uint64_t offset, size;
            file_.read(reinterpret_cast<char*>(&offset), sizeof(offset));
            file_.read(reinterpret_cast<char*>(&size), sizeof(size));
            if (!file_) {
                THROW_RUNTIME_ERROR("Failed to read chunk metadata");
            }
            column_meta.offsets.push_back(offset);
            column_meta.sizes.push_back(size);
        }

        metadata_.push_back(std::move(column_meta));
    }
}

void ColumnarReader::ReadRawColumnData(size_t column_index, size_t chunk_index, std::vector<char>& buffer) const {
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
}

std::shared_ptr<Column> ColumnarReader::GetColumnData(size_t column_index, size_t chunk_index) const {
    std::vector<char> raw_data;
    ReadRawColumnData(column_index, chunk_index, raw_data);
    ColumnType type = metadata_[column_index].type;
    
    std::shared_ptr<Column> column = ColumnFactory::MakeColumn(type);

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
    case ColumnType::ENUM_VAL: \
        std::static_pointer_cast<CLASS_TYPE>(column)->ReadFromBuffer(raw_data); \
        break;

    switch (type) {
        FOR_EACH_COLUMN_TYPE(HANDLE_TYPE)
        default:
            THROW_RUNTIME_ERROR("Unknown column type");
    }
#undef HANDLE_TYPE

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
