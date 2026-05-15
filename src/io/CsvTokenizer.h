#pragma once

#include "utils/VectorOfStrings.h"

#include <fstream>
#include <vector>

class CsvTokenizer {
public:
    CsvTokenizer(const std::string& file_path, char delim = ',');
    CsvTokenizer(const CsvTokenizer& other) = delete;
    CsvTokenizer& operator=(const CsvTokenizer& other) = delete;

    void GetNextRow(VectorOfStrings2D& vector_of_strings);
    bool IsEndOfLine() const;
    bool IsEOF() const;
    void ResetLineFlag();

private:
    static constexpr size_t kBufferSize = 1 << 20;

    bool is_special_[256] = {false};
    bool is_whitespace_[256] = {false};
    std::ifstream file_;
    std::vector<char> buffer_;
    size_t buffer_pos_ = 0;
    size_t buffer_end_ = 0;
    char delim_;
    bool end_of_line_ = false;
    bool eof_reached_ = false;
    bool GetChar(char& ch) {
        if (buffer_pos_ >= buffer_end_) [[unlikely]] {
            return RefillBuffer(ch);
        }
        ch = buffer_[buffer_pos_++];
        return true;
    }

    bool RefillBuffer(char& ch);
};
