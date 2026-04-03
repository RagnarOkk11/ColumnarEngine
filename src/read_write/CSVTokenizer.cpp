//
// Created by ragnarokk on 03.01.2026.
//

#include "CSVTokenizer.h"

#include "macro.h"

CSVTokenizer::CSVTokenizer(const std::string& file_path, char delim)
    : file_(file_path), buffer_(kBufferSize), delim_(delim) {
    if (!file_.is_open()) {
        THROW_RUNTIME_ERROR("Could not open CSV file");
    }
    is_special_[static_cast<unsigned char>(delim_)] = true;
    is_special_[static_cast<unsigned char>('\n')] = true;
    is_special_[static_cast<unsigned char>('"')] = true;

    is_whitespace_[static_cast<unsigned char>(' ')] = true;
    is_whitespace_[static_cast<unsigned char>('\t')] = true;
    is_whitespace_[static_cast<unsigned char>('\r')] = true;
}

bool CSVTokenizer::RefillBuffer(char& ch) {
    if (eof_reached_) {
        return false;
    }
    file_.read(buffer_.data(), buffer_.size());
    buffer_end_ = file_.gcount();
    buffer_pos_ = 0;
    if (buffer_end_ == 0) {
        eof_reached_ = true;
        return false;
    }
    ch = buffer_[buffer_pos_++];
    return true;
}

void CSVTokenizer::GetNextRow(VectorOfStrings2D& vector_of_strings) {
    char ch;
    bool in_quotes = false;
    bool token_started = false;
    bool token_materialized = false;

    auto start_token_if_needed = [&]() {
        if (!token_materialized) {
            vector_of_strings.StartAddString();
            token_materialized = true;
        }
    };

    while (true) {
        if (buffer_pos_ >= buffer_end_) [[unlikely]] {
            char tmp;
            if (!RefillBuffer(tmp)) {
                break;
            }
            --buffer_pos_;
        }

        if (!in_quotes) {
            if (!token_started) {
                while (buffer_pos_ < buffer_end_ && (buffer_[buffer_pos_] == ' ' || buffer_[buffer_pos_] == '\t')) {
                    ++buffer_pos_;
                }
            }

            size_t start = buffer_pos_;
            while (buffer_pos_ < buffer_end_) {
                if (is_special_[static_cast<unsigned char>(buffer_[buffer_pos_])]) {
                    break;
                }
                ++buffer_pos_;
            }
            if (buffer_pos_ > start) {
                start_token_if_needed();
                vector_of_strings.ContinueAddString(std::string_view(buffer_.data() + start, buffer_pos_ - start));
                token_started = true;
            }
            if (buffer_pos_ >= buffer_end_) {
                continue;
            }
        }

        ch = buffer_[buffer_pos_++];

        if (!token_started && (ch == ' ' || ch == '\t')) {
            continue;
        }

        if (ch == '"' && !in_quotes && !token_started) {
            start_token_if_needed();
            in_quotes = true;
            token_started = true;
            continue;
        }

        if (ch == '"' && in_quotes) {
            in_quotes = false;
            while (GetChar(ch) && ch != delim_ && ch != '\n') {
                if (!is_whitespace_[static_cast<unsigned char>(ch)]) {
                    THROW_RUNTIME_ERROR(
                        "Invalid CSV format: unexpected character after closing quote");
                }
            }
            start_token_if_needed();
            vector_of_strings.EndAddString();
            token_started = false;
            token_materialized = false;
            if (ch == '\n' || eof_reached_) {
                end_of_line_ = true;
                return;
            }
            continue;
        }

        if (!in_quotes && (ch == delim_ || ch == '\n')) {
            start_token_if_needed();
            while (!vector_of_strings.EmptyLastString() &&
                   is_whitespace_[static_cast<unsigned char>(vector_of_strings.BackLastString())]) {
                vector_of_strings.PopLastChar();
            }
            vector_of_strings.EndAddString();
            token_started = false;
            token_materialized = false;
            if (ch == '\n') {
                end_of_line_ = true;
                return;
            }
            continue;
        }

        start_token_if_needed();
        vector_of_strings.ContinueAddString(ch);
        token_started = true;
    }

    if (!token_materialized) {
        end_of_line_ = true;
        return;
    }

    while (!vector_of_strings.EmptyLastString() &&
           is_whitespace_[static_cast<unsigned char>(vector_of_strings.BackLastString())]) {
        vector_of_strings.PopLastChar();
    }
    end_of_line_ = true;
    vector_of_strings.EndAddString();
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
