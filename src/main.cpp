#include "CSVToColumnar.h"
#include "ExecutionAPI.h"

#include <chrono>

class Query {
public:
    Query(std::string columnar_file_path) : columnar_file_path_(std::move(columnar_file_path)) {
    }

    // SELECT COUNT(*) FROM hits;
    void Query0() {
        auto df = DataFrame::Select(columnar_file_path_, {})
                      .Aggregate({}, {Count()})
                      .Collect();

        df.Display();
    }

private:
    std::string columnar_file_path_;
};

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage " << argv[0] << ": " << "<csv_file_path> <columnar_file_path> <schema_file_path>\n";
        return 1;
    }
    try {
        std::cout << "Converting CSV to columnar format...\n";
        const auto start = std::chrono::steady_clock::now();
        CSVToColumnar converter;
        converter.ConvertWithSchema(argv[1], argv[2], argv[3]);
        const auto time = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Conversion completed in " << time << " seconds\n";

    } catch (std::exception& e) {
        std::cout << "Convert fail: " << e.what() << '\n';
        return 1;
    }

    try {
        std::cout << "Running...\n";
        const auto start = std::chrono::steady_clock::now();
        Query query(argv[2]);
        query.Query0();
        const auto time = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Query 1 completed in " << time << " seconds\n";
    } catch (std::exception& e) {
        std::cout << "Query fail: " << e.what() << '\n';
        return 1;
    }

}
