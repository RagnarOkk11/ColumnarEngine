#include "column/ColumnFactory.h"
#include "io/ColumnarReader.h"

#include "utils/Assert.h"
#include "utils/Macro.h"

#include <array>

void MetadataTable::ParseMetadata(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        THROW_RUNTIME_ERROR("Could not open file for metadata read");
    }

    file.seekg(0, std::ios::end);
    std::streamoff file_size = file.tellg();

    if (file_size < 24) {  // 20 байт мета + 4 байта магии
        THROW_RUNTIME_ERROR("File is too small to contain a valid footer");
    }

    file.seekg(-4, std::ios::end);
    std::array<char, 4> magic{};
    file.read(magic.data(), magic.size());
    if (!file || std::string(magic.data(), magic.size()) != "TUFF") {
        THROW_RUNTIME_ERROR("Invalid file footer magic");
    }

    file.seekg(file_size - 20, std::ios::beg);
    file.read(reinterpret_cast<char*>(&num_rows_), sizeof(num_rows_));

    uint64_t metadata_start;
    file.read(reinterpret_cast<char*>(&metadata_start), sizeof(metadata_start));

    size_t metadata_size = (file_size - 20) - metadata_start;
    std::vector<char> buffer(metadata_size);
    file.seekg(metadata_start, std::ios::beg);
    file.read(buffer.data(), metadata_size);

    size_t offset = 0;

    auto read_from_buf = [&](void* dest, size_t size) {
        std::memcpy(dest, buffer.data() + offset, size);
        offset += size;
    };

    uint32_t num_cols;
    read_from_buf(&num_cols, sizeof(num_cols));

    for (uint32_t i = 0; i < num_cols; ++i) {
        ColumnMetadata column_meta;
        uint32_t name_length;
        read_from_buf(&name_length, sizeof(name_length));

        column_meta.name.resize(name_length);
        if (name_length > 0) {
            read_from_buf(column_meta.name.data(), name_length);
        }

        uint8_t type;
        read_from_buf(&type, sizeof(type));
        column_meta.type = static_cast<ColumnType>(type);

        uint32_t num_chunks;
        read_from_buf(&num_chunks, sizeof(num_chunks));

        column_meta.offsets.reserve(num_chunks);
        column_meta.sizes.reserve(num_chunks);

        for (uint32_t j = 0; j < num_chunks; ++j) {
            uint64_t chunk_offset, chunk_size;
            read_from_buf(&chunk_offset, sizeof(chunk_offset));
            read_from_buf(&chunk_size, sizeof(chunk_size));

            column_meta.offsets.push_back(chunk_offset);
            column_meta.sizes.push_back(chunk_size);
        }
        column_meta_.push_back(std::move(column_meta));
    }
}

ColumnarReader::ColumnarReader(const std::string& file_name,
                               std::shared_ptr<const MetadataTable> meta)
    : file_(file_name, std::ios::binary), metadata_(std::move(meta)) {
    if (!file_.is_open()) {
        THROW_RUNTIME_ERROR("Could not open file for reading data");
    }
}

ColumnType ColumnarReader::GetColumnTypeByName(const std::string& name) const {
    for (const auto& meta : metadata_->GetColumns()) {
        if (meta.name == name) {
            return meta.type;
        }
    }
    THROW_RUNTIME_ERROR("Column " + name + " not found");
}

void ColumnarReader::ReadRawColumnData(size_t column_index, size_t chunk_index,
                                       std::vector<char>& buffer) const {
    const ColumnMetadata& meta = (*metadata_)[column_index];

    if (column_index >= metadata_->GetNumColumns()) {
        THROW_RUNTIME_ERROR("Column index out of range");
    }
    if (chunk_index >= meta.offsets.size()) {
        THROW_RUNTIME_ERROR("Chunk index out of range");
    }

    uint64_t offset = meta.offsets[chunk_index];
    uint64_t size = meta.sizes[chunk_index];

    buffer.resize(size);
    file_.seekg(offset, std::ios::beg);
    file_.read(buffer.data(), size);
}

std::shared_ptr<Column> ColumnarReader::GetColumnData(size_t column_index,
                                                      size_t chunk_index) const {
    std::vector<char> raw_data;
    ReadRawColumnData(column_index, chunk_index, raw_data);

    ColumnType type = (*metadata_)[column_index].type;
    std::shared_ptr<Column> column = ColumnFactory::MakeColumn(type);

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)                              \
    case ColumnType::ENUM_VAL:                                                  \
        std::static_pointer_cast<CLASS_TYPE>(column)->ReadFromBuffer(raw_data); \
        break;

    switch (type) {
        FOR_EACH_COLUMN_TYPE(HANDLE_TYPE)
        default: THROW_RUNTIME_ERROR("Unknown column type");
    }
#undef HANDLE_TYPE

    return column;
}
