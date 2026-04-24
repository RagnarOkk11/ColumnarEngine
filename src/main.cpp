#include "CSVToColumnar.h"
#include "ExecutionAPI.h"

#include <chrono>

class Query {
public:
    Query(std::string columnar_file_path) : columnar_file_path_(std::move(columnar_file_path)) {
    }

    // SELECT COUNT(*) FROM hits;
    void Query00() {
        auto df = DataFrame::Select(columnar_file_path_, {}).Aggregate({}, {Count()}).Collect();

        df.Display();
    }

    // SELECT COUNT(*) FROM hits WHERE AdvEngineID <> 0;
    void Query01() {
        auto df = DataFrame::Select(columnar_file_path_, {})
                      .Filter(NotEq<int16_t>("AdvEngineID", 0))
                      .Aggregate({}, {Count()})
                      .Collect();

        df.Display();
    }

    // SELECT SUM(AdvEngineID), COUNT(*), AVG(ResolutionWidth) FROM hits;
    void Query02() {
        auto df = DataFrame::Select(columnar_file_path_, {})
                      .Aggregate({}, {Sum("AdvEngineID"), Count(), Avg({"ResolutionWidth"})})
                      .Collect();

        df.Display();
    }

    // SELECT AVG(UserID) FROM hits;
    void Query03() {
        auto df = DataFrame::Select(columnar_file_path_, {})
                      .Aggregate({}, {Avg("UserID", "avg")})
                      .Collect();

        df.Display();
    }

    // SELECT COUNT(DISTINCT UserID) FROM hits;
    void Query04() {
        auto df = DataFrame::Select(columnar_file_path_, {})
                      .Aggregate({}, {DistinctCount("UserID", "distinct_user_count")})
                      .Collect();

        df.Display();
    }

    // SELECT COUNT(DISTINCT SearchPhrase) FROM hits;
    void Query05() {
        auto df =
            DataFrame::Select(columnar_file_path_, {})
                .Aggregate({}, {DistinctCount("SearchPhrase", "distinct_search_phrase_count")})
                .Collect();

        df.Display();
    }

private:
    std::string columnar_file_path_;
};

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage " << argv[0] << ": "
                  << "<csv_file_path> <columnar_file_path> <schema_file_path>\n";
        return 1;
    }
    try {
        std::cout << "Converting CSV to columnar format...\n";
        const auto start = std::chrono::steady_clock::now();
        CSVToColumnar converter;
        converter.ConvertWithSchema(argv[1], argv[2], argv[3]);
        const auto time =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() * 1000;
        std::cout << "Conversion completed in " << time << " ms\n";

    } catch (std::exception& e) {
        std::cout << "Convert fail: " << e.what() << '\n';
        return 1;
    }

    // try {
    //     std::cout << "Running...\n";
    //     const auto start = std::chrono::steady_clock::now();
    //     Query query(argv[2]);
    //     query.Query05();
    //     const auto time =
    //         std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() *
    //         1000;
    //     std::cout << "Query 3 completed in " << time << " ms\n";
    // } catch (std::exception& e) {
    //     std::cout << "Query fail: " << e.what() << '\n';
    //     return 1;
    // }
}
