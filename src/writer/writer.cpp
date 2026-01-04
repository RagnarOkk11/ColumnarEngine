#include "writer.h"
#include "../object.h"
#include "../csv_parser/CSVParser.h"

#include <vector>

void Writer::CSVWriteData(const std::string &file_to_read_path, const std::string &file_to_write_path) {
    CSVParser parser(file_to_read_path);
    auto [column_names, columns] = parser.CreateColumnStructure();

    std::ofstream file_to_write(file_to_write_path, std::ios::binary);
    if (!file_to_write.is_open()) {
        throw std::runtime_error("Could not open file for writing");
    }

    // magic, columns by batches, number of columns, name, type, offsets, sizes, number of rows, metadata_start, magic

    file_to_write.write("MYPAR1", 6);

    std::vector<ColumnMetadata> meta;
    for (size_t i = 0; i < columns.size(); ++i) {
        ColumnMetadata column_meta;
        column_meta.name = column_names[i];
        column_meta.type = columns[i]->GetType();
        meta.push_back(column_meta);
    }

    uint64_t current_offset = 6;
    while (parser.ReadNextBatch(columns)) {
        for (size_t i = 0; i < columns.size(); ++i) {
            meta[i].offsets.push_back(current_offset);
            uint64_t written_bytes = columns[i]->WriteToFile(file_to_write);
            meta[i].sizes.push_back(written_bytes);
            current_offset += written_bytes;
        }
    }

    uint64_t metadata_start = current_offset;
    uint32_t num_cols = meta.size();
    file_to_write.write(reinterpret_cast<const char *>(&num_cols), sizeof(num_cols));
    for (const auto &column_meta: meta) {
        uint32_t name_length = column_meta.name.size();
        file_to_write.write(reinterpret_cast<const char *>(&name_length), sizeof(name_length));
        file_to_write.write(column_meta.name.data(), name_length);

        uint8_t type = static_cast<uint8_t>(column_meta.type);
        file_to_write.write(reinterpret_cast<const char *>(&type), sizeof(type));

        uint32_t num_chunks = column_meta.offsets.size();
        file_to_write.write(reinterpret_cast<const char *>(&num_chunks), sizeof(num_chunks));
        for (size_t i = 0; i < num_chunks; ++i) {
            file_to_write.write(reinterpret_cast<const char *>(&column_meta.offsets[i]), sizeof(uint64_t));
            file_to_write.write(reinterpret_cast<const char *>(&column_meta.sizes[i]), sizeof(uint64_t));
        }
    }

    uint32_t num_rows = columns.empty() ? 0 : columns[0]->Size();
    file_to_write.write(reinterpret_cast<const char *>(&num_rows), sizeof(num_rows));

    file_to_write.write(reinterpret_cast<const char *>(&metadata_start), sizeof(metadata_start));

    file_to_write.write("MYPAR1", 6);
    file_to_write.close();
}
