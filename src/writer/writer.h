#ifndef COLUMNAR_ENGINE_WRITER_H
#define COLUMNAR_ENGINE_WRITER_H

#include <fstream>
#include <vector>

#include "../CSVParser.h"

class Writer {
public:
    void CSVWriteData(const std::string& file_to_read_path, const std::string &file_to_write_path) {
        CSVParser parser(file_to_read_path);
        auto [column_names, columns] = parser.Read();

        std::ofstream file_to_write(file_to_write_path, std::ios::binary);
        if (!file_to_write.is_open()) {
            throw std::runtime_error("Could not open file for writing");
        }

        file_to_write.write("MYPAR1", 6);

        for (const auto &column: columns) {
            column->WriteToFile(file_to_write);
        }

        uint32_t num_columns = columns.size();
        uint32_t num_rows = columns.empty() ? 0 : columns[0]->Size();
        file_to_write.write(reinterpret_cast<const char *>(&num_columns), sizeof(num_columns));
        file_to_write.write(reinterpret_cast<const char *>(&num_rows), sizeof(num_rows));

        file_to_write.write("MYPAR1", 6);

        file_to_write.close();
    }
};


#endif //COLUMNAR_ENGINE_WRITER_H
