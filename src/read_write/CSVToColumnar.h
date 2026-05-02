//
// Created by ragnarokk on 05.01.2026.
//

#ifndef COLUMNAR_ENGINE_CSVTOCOLUMNAR_H
#define COLUMNAR_ENGINE_CSVTOCOLUMNAR_H

#include <memory>
#include <string>

#include "CSVReader.h"
#include "Types.h"

class CSVToColumnar {
public:
    void Convert(const std::string& csv_file_path, const std::string& columnar_file_path);
    void ConvertWithSchema(const std::string& csv_file_path, const std::string& columnar_file_path,
                           const std::string& schema_file_path);

private:
    void ParseHeaderToken(std::string_view token, VectorOfStrings2D& column_names,
                          std::vector<ColumnType>& column_types);
    void AddColumn(ColumnType type, std::vector<std::shared_ptr<Column>>& columns);

    void WriteToColumnar(const std::string& columnar_file_path,
                         const VectorOfStrings2D& column_names,
                         const std::vector<ColumnType>& column_types, CSVReader& reader,
                         const std::vector<std::shared_ptr<Column>>& columns);
};

#endif  // COLUMNAR_ENGINE_CSVTOCOLUMNAR_H