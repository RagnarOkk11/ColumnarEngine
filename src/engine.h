//
// Created by ragnarokk on 06.01.2026.
//

#ifndef COLUMNAR_ENGINE_ENGINE_H
#define COLUMNAR_ENGINE_ENGINE_H

#include <string>

class Engine {
public:
    void InitDataFromCSV(const std::string& csv_file_path, const std::string& columnar_file_path);

    void ExecuteQuery1(const std::string& columnar_file_path);
};

#endif  // COLUMNAR_ENGINE_ENGINE_H
