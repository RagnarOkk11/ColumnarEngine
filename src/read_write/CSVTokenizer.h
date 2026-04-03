//
// Created by ragnarokk on 03.01.2026.
//

#ifndef COLUMNAR_ENGINE_CSVTOKENIZER_H
#define COLUMNAR_ENGINE_CSVTOKENIZER_H

#include "VectorOfStrings.h"

#include <fstream>
#include <vector>

class CSVTokenizer {
public:
    CSVTokenizer(const std::string& file_path, char delim = ',');
    CSVTokenizer(const CSVTokenizer& other) = delete;
    CSVTokenizer& operator=(const CSVTokenizer& other) = delete;

    void GetNextRow(VectorOfStrings2D& vector_of_strings);
    bool IsEndOfLine() const;
    bool IsEOF() const;
    void ResetLineFlag();

private:
    static constexpr size_t kBufferSize = 1 << 20;

    std::ifstream file_;
    std::vector<char> buffer_;
    size_t buffer_pos_ = 0;
    size_t buffer_end_ = 0;
    char delim_;
    bool end_of_line_ = false;
    bool eof_reached_ = false;

    bool is_special_[256] = {false};
    bool is_whitespace_[256] = {false};

    bool GetChar(char& ch) {
        if (buffer_pos_ >= buffer_end_) [[unlikely]] {
            return RefillBuffer(ch);
        }
        ch = buffer_[buffer_pos_++];
        return true;
    }

    bool RefillBuffer(char& ch);
};

#endif  // COLUMNAR_ENGINE_CSVTOKENIZER_H
