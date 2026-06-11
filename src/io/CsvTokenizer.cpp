#include "io/CsvTokenizer.h"

CsvTokenizer::CsvTokenizer(const std::string& file_path, char delim)
    : file_(file_path), buffer_(kBufferSize), delim_(delim) {
    if (!file_.is_open()) {
        THROW_RUNTIME_ERROR("Could not open CSV file");
    }

    is_whitespace_[static_cast<unsigned char>(' ')] = true;
    is_whitespace_[static_cast<unsigned char>('\t')] = true;
    is_whitespace_[static_cast<unsigned char>('\r')] = true;
}

bool CsvTokenizer::RefillBuffer(char& ch) {
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

void CsvTokenizer::GetNextRow(VectorOfStrings2D& rows) {
    bool token_started = false;
    bool in_quotes = false;

    auto start_token_if_needed = [&]() {
        if (!token_started) {
            rows.StartAddString();
            token_started = true;
        }
    };

    while (true) {
        if (buffer_pos_ >= buffer_end_) {
            char tmp;
            if (!RefillBuffer(tmp)) {
                break;
            }
            --buffer_pos_;
        }

        char ch = buffer_[buffer_pos_++];

        if (!token_started && !in_quotes && is_whitespace_[static_cast<unsigned char>(ch)]) {
            continue;
        }

        start_token_if_needed();
        if (in_quotes) {
            if (ch == '"') {
                bool is_escaped_quote = false;
                if (buffer_pos_ < buffer_end_) {
                    if (buffer_[buffer_pos_] == '"') {
                        is_escaped_quote = true;
                    }
                } else {
                    char tmp;
                    if (RefillBuffer(tmp)) {
                        --buffer_pos_;
                        if (buffer_[buffer_pos_] == '"') {
                            is_escaped_quote = true;
                        }
                    }
                }

                if (is_escaped_quote) {
                    rows.ContinueAddString('"');
                    buffer_pos_++;
                } else {
                    in_quotes = false;
                }
            } else if (ch != '\r') {
                rows.ContinueAddString(ch);
            }
        } else {
            if (ch == '"') {
                in_quotes = true;
            } else if (ch == delim_ || ch == '\n') {
                while (!rows.EmptyLastString() &&
                       is_whitespace_[static_cast<unsigned char>(
                           rows.BackLastString())]) {
                    rows.PopLastChar();
                }

                rows.EndAddString();
                token_started = false;

                if (ch == '\n') {
                    end_of_line_ = true;
                    return;
                }
            } else if (ch != '\r') {
                rows.ContinueAddString(ch);
            }
        }
    }

    if (token_started || !eof_reached_) {
        start_token_if_needed();
        while (!rows.EmptyLastString() &&
               is_whitespace_[static_cast<unsigned char>(rows.BackLastString())]) {
            rows.PopLastChar();
        }
        rows.EndAddString();
    }
    end_of_line_ = true;
}

bool CsvTokenizer::IsEndOfLine() const {
    return end_of_line_;
}

bool CsvTokenizer::IsEOF() const {
    return eof_reached_;
}

void CsvTokenizer::ResetLineFlag() {
    end_of_line_ = false;
}
