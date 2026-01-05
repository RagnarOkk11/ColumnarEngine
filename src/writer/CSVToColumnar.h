//
// Created by ragnarokk on 05.01.2026.
//

#ifndef COLUMNAR_ENGINE_CSVTOCOLUMNAR_H
#define COLUMNAR_ENGINE_CSVTOCOLUMNAR_H

#include <string>

#include "object.h"

class CSVToColumnar {
public:
    void Convert(const std::string& csv_file_path, const std::string& columnar_file_path);

private:
    std::pair<ColumnType, std::string> ParseHeaderToken(const std::string& token);
};

#endif  // COLUMNAR_ENGINE_CSVTOCOLUMNAR_H