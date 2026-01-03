//
// Created by ragnarokk on 03.01.2026.
//

#include "CSVTokenizer.h"

CSVTokenizer::CSVTokenizer(const std::string &file_path, char delim) : file_(file_path), delim_(delim) {
    if (!file_.is_open()) {
        throw std::runtime_error("Could not open CSV file");
    }
}

std::string CSVTokenizer::GetNextToken() {
    char ch;
    std::string token;
    bool in_quotes = false;
    bool token_started = false;

    while (file_.get(ch)) {
        if (!token_started && (ch == ' ' || ch == '\t')) {
            continue;
        }

        if (ch == '"' && !in_quotes && !token_started) {
            in_quotes = true;
            token_started = true;
            continue;
        }

        if (ch == '"' && in_quotes) {
            in_quotes = false;
            while (file_.get(ch) && ch != delim_ && ch != '\n') {
                if (ch != ' ' && ch != '\t') {
                    throw std::runtime_error("Invalid CSV format: unexpected character after closing quote");
                }
            }
            if (ch == '\n') {
                end_of_line_ = true;
            }
            return token;
        }

        if (!in_quotes && (ch == delim_ || ch == '\n')) {
            if (ch == '\n') {
                end_of_line_ = true;
            }
            while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) {
                token.pop_back();
            }
            return token;
        }

        token += ch;
        token_started = true;
    }

    if (!token.empty()) {
        while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) {
            token.pop_back();
        }
    }
    eof_reached_ = true;
    return token;
}

std::pair<ColumnType, std::string> CSVTokenizer::GetNextTokenHeader() {
    std::string type_str;
    char ch;

    while (file_.get(ch) && (ch == ' ' || ch == '\t')) {
    }
    while (ch != ':' && ch != '\n' && !file_.eof()) {
        type_str += ch;
        if (!file_.get(ch)) {
            throw std::runtime_error("Invalid CSV format: expected ':' after type");
        }
    }

    if (ch != ':') {
        throw std::runtime_error("Invalid CSV format: expected ':' after type");
    }

    while (!type_str.empty() && (type_str.back() == ' ' || type_str.back() == '\t')) {
        type_str.pop_back();
    }

    std::string name_str;
    bool in_quotes = false;
    bool name_started = false;

    while (file_.get(ch)) {
        if (!name_started && (ch == ' ' || ch == '\t')) {
            continue;
        }

        if (ch == '"' && !in_quotes && !name_started) {
            in_quotes = true;
            name_started = true;
            continue;
        }

        if (ch == '"' && in_quotes) {
            in_quotes = false;
            while (file_.get(ch) && ch != delim_ && ch != '\n') {
                if (ch != ' ' && ch != '\t') {
                    throw std::runtime_error("Invalid CSV format: unexpected character after closing quote");
                }
            }
            if (ch == '\n') {
                in_header_ = false;
            }
            break;
        }

        if (!in_quotes && (ch == delim_ || ch == '\n')) {
            if (ch == '\n') {
                in_header_ = false;
            }
            break;
        }

        name_str += ch;
        name_started = true;
    }

    while (!name_str.empty() && (name_str.back() == ' ' || name_str.back() == '\t')) {
        name_str.pop_back();
    }

    ColumnType type;
    if (type_str == "INT32" || type_str == "INT") {
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

bool CSVTokenizer::IsEndOfLine() const {
    return end_of_line_;
}

bool CSVTokenizer::IsEOF() const {
    return eof_reached_;
}
