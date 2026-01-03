#include <iostream>
#include "CSVParser.h"

int main() {
    std::cout << "Hello Columnar Engine!\n";

    try {
        CSVParser reader("test.csv");
        auto [column_names, columns] = reader.Read();

        std::cout << "Колонки: ";
        for (const auto& name : column_names) {
            std::cout << name << " ";
        }
        std::cout << "\n";

        std::cout << "Количество строк: " << columns[0]->Size() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
