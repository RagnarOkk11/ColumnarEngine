//
// Created by ragnarokk on 05.01.2026.
//

#include <memory>

#include "CSVToColumnar.h"
#include "CSVReader.h"
#include "ColumnarWriter.h"
#include "Schema.h"
#include "macro.h"
#include "VectorOfStrings.h"

#include <unordered_map>

void CSVToColumnar::Convert(const std::string& csv_file_path,
                            const std::string& columnar_file_path) {
    CSVReader reader(csv_file_path);
    VectorOfStrings2D raw_header;
    reader.ReadHeader(raw_header);

    std::vector<std::shared_ptr<Column>> columns;
    VectorOfStrings2D column_names;
    std::vector<ColumnType> column_types;

    size_t num_columns = raw_header.Size();
    for (size_t i = 0; i < num_columns; ++i) {
        std::string_view token = raw_header.GetString(i);
        ParseHeaderToken(token, column_names, column_types);
        auto type = column_types.back();
        AddColumn(type, columns);
    }

    WriteToColumnar(columnar_file_path, column_names, column_types, reader, columns);
}

void CSVToColumnar::ConvertWithSchema(const std::string& csv_file_path,
                                      const std::string& columnar_file_path,
                                      const std::string& schema_file_path) {
    CSVReader reader(csv_file_path);
    Schema schema = Schema::DeserializeSchema(schema_file_path);
    const std::vector<Field> fields = schema.GetFields();

    std::vector<std::shared_ptr<Column>> columns;
    VectorOfStrings2D column_names;
    std::vector<ColumnType> column_types;

    for (const Field& field : fields) {
        std::string_view header = field.name;
        ColumnType type = field.type;
        column_names.AddString(header);
        column_types.push_back(type);
        AddColumn(type, columns);
    }

    WriteToColumnar(columnar_file_path, column_names, column_types, reader, columns);
}

void CSVToColumnar::ParseHeaderToken(std::string_view token, VectorOfStrings2D& column_names,
                                     std::vector<ColumnType>& column_types) {
    size_t colon_pos = token.find(':');
    if (colon_pos == std::string::npos) {
        THROW_RUNTIME_ERROR("Invalid header token: " + std::string(token));
    }
    std::string_view type_str = token.substr(0, colon_pos);
    std::string_view name_str = token.substr(colon_pos + 1);

    // OPTIMIZE: copying in AddString
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE)    \
    if (type_str == STR_VAL) {                        \
        column_names.AddString(name_str);             \
        column_types.push_back(ColumnType::ENUM_VAL); \
    } else
    FOR_EACH_COLUMN_TYPE(HANDLE_TYPE) {
        THROW_RUNTIME_ERROR("Invalid header token: " + std::string(token));
    }
#undef HANDLE_TYPE
}

void CSVToColumnar::AddColumn(ColumnType type, std::vector<std::shared_ptr<Column>>& columns) {
    switch (type) {
#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
    case ColumnType::ENUM_VAL: columns.push_back(std::make_shared<CLASS_TYPE>()); break;

        FOR_EACH_COLUMN_TYPE(HANDLE_TYPE)
#undef HANDLE_TYPE

        default:
            throw std::runtime_error("Unsupported column type");
    }
}

void CSVToColumnar::WriteToColumnar(const std::string& columnar_file_path,
                                    const VectorOfStrings2D& column_names,
                                    const std::vector<ColumnType>& column_types, CSVReader& reader,
                                    const std::vector<std::shared_ptr<Column>>& columns) {
    ColumnarWriter writer(columnar_file_path);
    writer.WriteHeader(column_names, column_types);

    static constexpr size_t kByteSize = 1 << 20;
    bool end_flag = true;
    VectorOfStrings2D column_batch;

    while (end_flag) {
        size_t cur_batch_size = 0;
        column_batch.Clear();

        while (column_batch.ApproxByteSize() < kByteSize) {
            bool has_row = reader.ReadRow(column_batch);
            if (has_row) {
                column_batch.StartNewLine();
                ++cur_batch_size;
            } else {
                end_flag = false;
                break;
            }
        }

        if (cur_batch_size == 0) {
            break;
        }

        size_t batch_width  = column_batch.Width();
        for (size_t j = 0; j < batch_width; ++j) {
            columns[j]->AddBatch(column_batch, j);
        }

        if (!columns.empty()) {
            writer.WriteBatch(columns);
        }
        for (auto& col : columns) {
            col->Clear();
        }
    }


    writer.Finalize();
}
