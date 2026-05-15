#pragma once

#include <string>

class CsvToColumnar {
public:
    static void Convert(const std::string& csv_file_path, const std::string& columnar_file_path);
    static void ConvertWithSchema(const std::string& csv_file_path,
                                  const std::string& columnar_file_path,
                                  const std::string& schema_file_path);
};
