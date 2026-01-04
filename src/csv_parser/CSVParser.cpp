//
// Created by ragnarokk on 03.01.2026.
//

#include "../object.h"
#include "CSVParser.h"

CSVParser::CSVParser(const std::string &file_path, char delim) : tokenizer_(file_path, delim) {
}

std::pair<std::vector<std::string>, std::vector<std::unique_ptr<Column> > > CSVParser::CreateColumnStructure() {
    std::vector<std::unique_ptr<Column> > columns;
    std::vector<std::string> column_names;

    while (tokenizer_.InHeader()) {
        auto [type, name] = tokenizer_.GetNextTokenHeader();
        column_names.push_back(name);

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

    return {std::move(column_names), std::move(columns)};
}

bool CSVParser::ReadNextBatch(std::vector<std::unique_ptr<Column> > &columns, size_t batch_size) {
    for (size_t i = 0; i < columns.size(); ++i) {
        columns[i]->Clear();
    }

    size_t rows_read = 0;

    while (!tokenizer_.IsEOF() && batch_size > 0) {
        for (size_t i = 0; i < columns.size(); ++i) {
            std::string token = tokenizer_.GetNextToken();

            if (tokenizer_.IsEOF() && token.empty()) {
                if (i == 0) {
                    return rows_read > 0;
                }
                throw std::runtime_error("Unexpected end of file while reading data");
            }

            if (tokenizer_.IsEOF() && token.empty() && i > 0) {
                throw std::runtime_error("Unexpected end of file while reading data");
            }

            columns[i]->Add(token);

            if (i == columns.size() - 1) {
                if (!tokenizer_.IsEndOfLine() && !tokenizer_.IsEOF()) {
                    throw std::runtime_error("Invalid CSV format: expected end of line after last column");
                }
                --batch_size;
                ++rows_read;
                tokenizer_.ResetLineFlag();
            }
        }
    }

    return rows_read > 0;
}
