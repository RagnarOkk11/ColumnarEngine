#ifndef COLUMNAR_ENGINE_WRITER_H
#define COLUMNAR_ENGINE_WRITER_H

#include <cstdint>
#include <fstream>
#include <vector>

class Writer {
public:
    template<typename T>
    void Run(std::vector<std::vector<T> > &data, const std::string &file_path) {
        std::ofstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file for writing");
        }

        file.write("MYPAR1", 6);

        for (const auto &column: data) {
            for (const T &value: column) {
                file.write(reinterpret_cast<const char *>(&value), sizeof(value));
            }
        }

        uint32_t num_columns = data.size();
        uint32_t num_rows = data.empty() ? 0 : data[0].size();
        file.write(reinterpret_cast<const char *>(&num_columns), sizeof(num_columns));
        file.write(reinterpret_cast<const char *>(&num_rows), sizeof(num_rows));

        file.write("MYPAR1", 6);

        file.close();
    }
};


#endif //COLUMNAR_ENGINE_WRITER_H
