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

    // SELECT MIN(EventDate), MAX(EventDate) FROM hits;
    void Query06() {
        auto df =
            DataFrame::Select(columnar_file_path_, {})
                .Aggregate({}, {Min("EventDate", "min_event_date"), Max("EventDate", "max_event_date")})
                .Collect();

        df.Display();
    }

    void RunQuery(int query_number) {
        switch (query_number) {
            case 0: Query00(); break;
            case 1: Query01(); break;
            case 2: Query02(); break;
            case 3: Query03(); break;
            case 4: Query04(); break;
            case 5: Query05(); break;
            case 6: Query06(); break;
            default:
                THROW_RUNTIME_ERROR("Unknown query number: " + std::to_string(query_number));
        }
    }

private:
    std::string columnar_file_path_;
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " convert <csv_file> <columnar_file> <schema_file>\n"
                  << "  " << argv[0] << " query <columnar_file> <query_number>\n";
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "convert") {
        if (argc < 5) {
            std::cerr << "Usage: " << argv[0] << " convert <csv_file> <columnar_file> <schema_file>\n";
            return 1;
        }
        try {
            std::cerr << "Converting CSV to columnar format...\n";
            const auto start = std::chrono::steady_clock::now();
            CSVToColumnar converter;
            converter.ConvertWithSchema(argv[2], argv[3], argv[4]);
            const auto time = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() * 1000;
            std::cerr << "Conversion completed in " << time << " ms\n";
        } catch (std::exception& e) {
            std::cerr << "Convert fail: " << e.what() << '\n';
            return 1;
        }
    } else if (mode == "query") {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " query <columnar_file> <query_number>\n";
            return 1;
        }
        try {
            int query_number = std::stoi(argv[3]);
            Query query(argv[2]);
            query.RunQuery(query_number);
        } catch (std::exception& e) {
            std::cerr << "Query fail: " << e.what() << '\n';
            return 1;
        }
    } else {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
