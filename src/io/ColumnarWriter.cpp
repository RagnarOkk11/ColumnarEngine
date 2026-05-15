#include "io/ColumnarWriter.h"

#include "column/NumericColumn.h"
#include "column/StringColumn.h"
#include "column/CharColumn.h"
#include "column/TemporalColumn.h"

#include "utils/Macro.h"

ColumnarWriter::ColumnarWriter(const std::string& file_path) {
    file_.open(file_path, std::ios::binary);
    if (!file_.is_open()) {
        THROW_RUNTIME_ERROR("Could not open file for writing");
    }
}

void ColumnarWriter::WriteHeader(const VectorOfStrings& column_names,
                                 const std::vector<ColumnType>& column_types) {
    if (header_written_) {
        THROW_RUNTIME_ERROR("Header already written");
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
        THROW_RUNTIME_ERROR("Header must be written before writing batches");
    }
    if (columns.size() != metadata_.size()) {
        THROW_RUNTIME_ERROR("Number of columns does not match metadata");
    }
    for (size_t i = 0; i < columns.size(); ++i) {
        uint64_t offset = current_offset_;
        uint64_t size = 0;

        ColumnType type = metadata_[i].type;
        
        if (type == ColumnType::STRING) {
            const auto* data = static_cast<const VectorOfStrings*>(columns[i]->GetRawData());
            const std::vector<size_t>& offsets = data->GetOffsets();
            const std::vector<char>& raw = data->GetData();
            
            uint64_t total_size = 0;
            std::vector<char> buffer;
            size_t h = columns[i]->Size();
            size_t total_len = 0;
            for (size_t k = 0; k < h; ++k) {
                total_len += sizeof(uint32_t) + (offsets[k + 1] - offsets[k]);
            }
            buffer.reserve(total_len);
            for (size_t k = 0; k < h; ++k) {
                size_t start = offsets[k];
                size_t end = offsets[k + 1];
                uint32_t length = end - start;
                buffer.insert(buffer.end(), reinterpret_cast<const char*>(&length),
                              reinterpret_cast<const char*>(&length) + sizeof(length));
                if (length > 0) {
                    buffer.insert(buffer.end(), raw.begin() + start, raw.begin() + end);
                }
                total_size += sizeof(length) + length;
            }
            file_.write(buffer.data(), buffer.size());
            size = total_size;
        } else {
            size_t elem_size = 0;
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
            case ColumnType::ENUM_VAL: { \
                elem_size = sizeof(CLASS_TYPE::ValueType); \
                size = columns[i]->Size() * elem_size; \
                if (size > 0) { \
                    auto* vec = static_cast<const CLASS_TYPE::ContainerType*>(columns[i]->GetRawData()); \
                    file_.write(reinterpret_cast<const char*>(vec->data()), size); \
                } \
                break; \
            }
            
            switch (type) {
                FOR_NUMERIC_COLUMN_TYPE(HANDLE_TYPE)
                default: break;
            }
#undef HANDLE_TYPE
        }

        metadata_[i].offsets.push_back(offset);
        metadata_[i].sizes.push_back(size);
        current_offset_ += size;
    }
    if (!columns.empty()) {
        num_rows_ += columns[0]->Size();
    }
}

void ColumnarWriter::Finalize() && {
    if (!header_written_) {
        THROW_RUNTIME_ERROR("Header must be written before finalizing");
    }

    WriteMetadata();

    file_.write("TUFF", 4);
}

void ColumnarWriter::WriteMetadata() {
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
}
