//
// Created by ragnarokk on 03.01.2026.
//

#include "CSVTokenizer.h"

CSVTokenizer::CSVTokenizer(const std::string& file_path, char delim)
    : file_(file_path), delim_(delim) {
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
                    throw std::runtime_error(
                        "Invalid CSV format: unexpected character after closing quote");
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
    if (file_.eof()) {
        eof_reached_ = true;
    }
    return token;
}

bool CSVTokenizer::IsEndOfLine() const {
    return end_of_line_;
}

bool CSVTokenizer::IsEOF() const {
    return eof_reached_;
}

void CSVTokenizer::ResetLineFlag() {
    end_of_line_ = false;
}
