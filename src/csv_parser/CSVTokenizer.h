//
// Created by ragnarokk on 03.01.2026.
//

#ifndef COLUMNAR_ENGINE_CSVTOKENIZER_H
#define COLUMNAR_ENGINE_CSVTOKENIZER_H

#include <fstream>

class CSVTokenizer {
public:
    CSVTokenizer(const std::string& file_path, char delim = ',');

    CSVTokenizer(const CSVTokenizer& other) = delete;

    CSVTokenizer& operator=(const CSVTokenizer& other) = delete;

    std::string GetNextToken();

    bool IsEndOfLine() const;

    bool IsEOF() const;

    void ResetLineFlag();

private:
    std::ifstream file_;
    char delim_;
    bool end_of_line_ = false;
    bool eof_reached_ = false;
};

#endif  // COLUMNAR_ENGINE_CSVTOKENIZER_H
