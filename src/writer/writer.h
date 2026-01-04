#ifndef COLUMNAR_ENGINE_WRITER_H
#define COLUMNAR_ENGINE_WRITER_H

#include <fstream>

class Writer {
public:
    void CSVWriteData(const std::string& file_to_read_path, const std::string &file_to_write_path);
};


#endif //COLUMNAR_ENGINE_WRITER_H
