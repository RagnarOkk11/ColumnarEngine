//
// Created by ragnarokk on 03.01.2026.
//

#include "CSVParser.h"

CSVParser::CSVParser(const std::string &file_path, char delim) : tokenizer_(file_path, delim) {
}

std::pair<std::vector<std::string>, std::vector<std::unique_ptr<Column> > > CSVParser::Read() {
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

    while (!tokenizer_.IsEOF()) {
        for (size_t i = 0; i < columns.size(); ++i) {
            std::string token = tokenizer_.GetNextToken();

            if (tokenizer_.IsEOF() && token.empty()) {
                return {column_names, std::move(columns)};
            }

            columns[i]->Add(token);
            if (i == columns.size() - 1) {
                if (!tokenizer_.IsEndOfLine() && !tokenizer_.IsEOF()) {
                    throw std::runtime_error("Invalid CSV format: expected end of line after last column");
                }
                tokenizer_.ResetLineFlag();
            }
        }
    }

    return {column_names, std::move(columns)};
}
