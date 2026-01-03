//
// Created by ragnarokk on 03.01.2026.
//

#ifndef COLUMNAR_ENGINE_CSVTOKENIZER_H
#define COLUMNAR_ENGINE_CSVTOKENIZER_H


#include <fstream>

#include "object.h"

class CSVTokenizer {
public:
    CSVTokenizer(const std::string &file_path, char delim = ',');

    CSVTokenizer(const CSVTokenizer &other) = delete;

    CSVTokenizer &operator=(const CSVTokenizer &other) = delete;

    std::string GetNextToken();

    std::pair<ColumnType, std::string> GetNextTokenHeader();

    bool IsEndOfLine() const;

    bool IsEOF() const;

    void ResetLineFlag() {
        end_of_line_ = false;
    }

    bool InHeader() const {
        return in_header_;
    }

private:
    std::ifstream file_;
    char delim_;
    bool in_header_ = true;
    bool end_of_line_ = false;
    bool eof_reached_ = false;
};


#endif //COLUMNAR_ENGINE_CSVTOKENIZER_H
