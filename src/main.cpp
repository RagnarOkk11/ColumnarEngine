// Всем привет, сегодня мы будем... вам показывать... короче идите ...спать)
// "Не судите строго, это мое первое задержание..."
// Я честно делал все сам, особенно тесты, и тем более следующие строчки (до отступа)
// Copyright 2024 Your Name
// Вообще я хотел написать на Rust, но потом подумал, что C++ это тоже неплохо
// И вообще я люблю C++, особенно современные стандарты
// Ну и конечно же я использовал C++20, потому что это круто (на самом деле С++23)

// Эта штука умеет пока что инициализировать данные из CSV файла и выполнять простой запрос, где sum
// и count считается для колонки с названием "prices", затем все это выводится в консоль

#include <iostream>

#include "engine.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0]
                  << " <1: init_data | 2: execute_query> <csv_file_path | columnar_file_path> "
                     "<columnar_file_path | ->"
                  << std::endl;
        return 1;
    }
    Engine engine;
    int mode = std::stoi(argv[1]);
    if (mode == 1) {
        if (argc < 4) {
            std::cout << "Usage: " << argv[0]
                      << " <1: init_data | 2: execute_query> <csv_file_path | columnar_file_path> "
                         "<columnar_file_path | ->"
                      << std::endl;
        }
        std::string csv_file_path = argv[2];
        std::string columnar_file_path = argv[3];
        engine.InitDataFromCSV(csv_file_path, columnar_file_path);
    } else if (mode == 2) {
        std::string columnar_file_path = argv[2];
        engine.ExecuteQuery1(columnar_file_path);
    } else {
        std::cout << "Invalid mode. Use 1 for init_data or 2 for execute_query." << std::endl;
    }

    return 0;
}
