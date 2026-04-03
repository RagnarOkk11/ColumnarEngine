//
// Created by ragnarokk on 06.01.2026.
//

#include <iostream>

#include "read_write/CSVToColumnar.h"
#include "Engine.h"

void Engine::InitDataFromCSV(const std::string& csv_file_path, const std::string& columnar_file_path) {
    CSVToColumnar converter;
    converter.Convert(csv_file_path, columnar_file_path);
}

// void Engine::ExecuteQuery(const std::string& columnar_file_path) {}
