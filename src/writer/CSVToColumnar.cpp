//
// Created by ragnarokk on 05.01.2026.
//

#include <memory>

#include "CSVToColumnar.h"
#include "RawCSVReader.h"
#include "ColumnarWriter.h"

void CSVToColumnar::Convert(const std::string& csv_file_path,
                            const std::string& columnar_file_path) {
    RawCSVReader reader(csv_file_path);
    std::vector<std::string> raw_header = reader.ReadHeader();

    std::vector<std::unique_ptr<Column> > columns;
    std::vector<std::string> column_names;
    std::vector<ColumnType> column_types;

    for (const auto& token : raw_header) {
        auto [type, name] = ParseHeaderToken(token);
        column_names.push_back(name);
        column_types.push_back(type);

        switch (type) {
            case ColumnType::INT32:
                columns.push_back(std::make_unique<Int32Column>());
                break;
            case ColumnType::FLOAT:
                columns.push_back(std::make_unique<FloatColumn>());
                break;
            case ColumnType::STRING:
                columns.push_back(std::make_unique<StringColumn>());
                break;
            case ColumnType::DATE:
                columns.push_back(std::make_unique<DateColumn>());
                break;
            case ColumnType::TIMESTAMP:
                columns.push_back(std::make_unique<TimestampColumn>());
                break;
        }
    }

    ColumnarWriter writer(columnar_file_path);
    writer.WriteHeader(column_names, column_types);

    std::vector<std::string> row;
    size_t current_batch_size = 0;
    constexpr size_t kBatchSize = 10000;

    while (reader.ReadRow(row)) {
        for (size_t i = 0; i < columns.size(); ++i) {
            columns[i]->Add(row[i]);
        }

        ++current_batch_size;
        if (current_batch_size >= kBatchSize) {
            writer.WriteBatch(columns);
            current_batch_size = 0;
            for (auto& col : columns) {
                col->Clear();
            }
        }
    }
    if (current_batch_size > 0) {
        writer.WriteBatch(columns);
    }
    writer.Finalize();
}

std::pair<ColumnType, std::string> CSVToColumnar::ParseHeaderToken(const std::string& token) {
    size_t colon_pos = token.find(':');
    if (colon_pos == std::string::npos) {
        throw std::runtime_error("Invalid header token: " + token);
    }
    std::string type_str = token.substr(0, colon_pos);
    std::string name_str = token.substr(colon_pos + 1);
    ColumnType type;
    if (type_str == "INT" || type_str == "INT32") {
        type = ColumnType::INT32;
    } else if (type_str == "FLOAT") {
        type = ColumnType::FLOAT;
    } else if (type_str == "STRING") {
        type = ColumnType::STRING;
    } else if (type_str == "DATE") {
        type = ColumnType::DATE;
    } else if (type_str == "TIMESTAMP") {
        type = ColumnType::TIMESTAMP;
    } else {
        throw std::runtime_error("Unknown column type: " + type_str);
    }
    return {type, name_str};
}
