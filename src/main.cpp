#include "io/CsvToColumnar.h"
#include "execution/ExecutionApi.h"

#include <chrono>
#include <sys/stat.h>

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
                      .Filter(NotEq("AdvEngineID", 0))
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
        auto df = DataFrame::Select(columnar_file_path_, {})
                      .Aggregate({}, {Min("EventDate", "min_event_date"),
                                      Max("EventDate", "max_event_date")})
                      .Collect();

        df.Display();
    }

    // SELECT AdvEngineID, COUNT(*) FROM hits WHERE AdvEngineID <> 0 GROUP BY AdvEngineID ORDER BY
    // COUNT(*) DESC;
    void Query07() {
        auto df = DataFrame::Select(columnar_file_path_, {"AdvEngineID"})
                      .Filter(NotEq("AdvEngineID", 0))
                      .Aggregate({"AdvEngineID"}, {Count()})
                      .OrderBy({{"count", true}})
                      .Collect();
        df.Display();
    }

    // SELECT RegionID, COUNT(DISTINCT UserID) AS u FROM hits GROUP BY RegionID ORDER BY u DESC
    // LIMIT 10;
    void Query08() {
        auto df = DataFrame::Select(columnar_file_path_, {"RegionID", "UserID"})
                      .Aggregate({"RegionID"}, {DistinctCount("UserID", "u")})
                      .OrderBy({{"u", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT RegionID, SUM(AdvEngineID), COUNT(*) AS c, AVG(ResolutionWidth), COUNT(DISTINCT
    // UserID) FROM hits GROUP BY RegionID ORDER BY c DESC LIMIT 10;
    void Query09() {
        auto df = DataFrame::Select(columnar_file_path_,
                                    {"RegionID", "AdvEngineID", "ResolutionWidth", "UserID"})
                      .Aggregate({"RegionID"}, {Sum("AdvEngineID"), Count("*", "c"),
                                                Avg("ResolutionWidth"), DistinctCount("UserID")})
                      .OrderBy({{"c", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits WHERE MobilePhoneModel <> ''
    // GROUP BY MobilePhoneModel ORDER BY u DESC LIMIT 10;
    void Query10() {
        auto df = DataFrame::Select(columnar_file_path_, {"MobilePhoneModel", "UserID"})
                      .Filter(NotEq("MobilePhoneModel", ""))
                      .Aggregate({"MobilePhoneModel"}, {DistinctCount("UserID", "u")})
                      .OrderBy({{"u", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT MobilePhone, MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits WHERE
    // MobilePhoneModel <> '' GROUP BY MobilePhone, MobilePhoneModel ORDER BY u DESC LIMIT 10;
    void Query11() {
        auto df =
            DataFrame::Select(columnar_file_path_, {"MobilePhone", "MobilePhoneModel", "UserID"})
                .Filter(NotEq("MobilePhoneModel", ""))
                .Aggregate({"MobilePhone", "MobilePhoneModel"}, {DistinctCount("UserID", "u")})
                .OrderBy({{"u", true}}, 10)
                .Collect();
        df.Display();
    }

    // SELECT SearchPhrase, COUNT(*) AS c FROM hits WHERE SearchPhrase <> '' GROUP BY SearchPhrase
    // ORDER BY c DESC LIMIT 10;
    void Query12() {
        auto df = DataFrame::Select(columnar_file_path_, {"SearchPhrase"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .Aggregate({"SearchPhrase"}, {Count("*", "c")})
                      .OrderBy({{"c", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT SearchPhrase, COUNT(DISTINCT UserID) AS u FROM hits WHERE SearchPhrase <> '' GROUP BY
    // SearchPhrase ORDER BY u DESC LIMIT 10;
    void Query13() {
        auto df = DataFrame::Select(columnar_file_path_, {"SearchPhrase", "UserID"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .Aggregate({"SearchPhrase"}, {DistinctCount("UserID", "u")})
                      .OrderBy({{"u", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT SearchEngineID, SearchPhrase, COUNT(*) AS c FROM hits WHERE SearchPhrase <> '' GROUP
    // BY SearchEngineID, SearchPhrase ORDER BY c DESC LIMIT 10;
    void Query14() {
        auto df = DataFrame::Select(columnar_file_path_, {"SearchEngineID", "SearchPhrase"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .Aggregate({"SearchEngineID", "SearchPhrase"}, {Count("*", "c")})
                      .OrderBy({{"c", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT UserID, COUNT(*) FROM hits GROUP BY UserID ORDER BY COUNT(*) DESC LIMIT 10;
    void Query15() {
        auto df = DataFrame::Select(columnar_file_path_, {"UserID"})
                      .Aggregate({"UserID"}, {Count("*")})
                      .OrderBy({{"count", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase ORDER BY
    // COUNT(*) DESC LIMIT 10;
    void Query16() {
        auto df = DataFrame::Select(columnar_file_path_, {"UserID", "SearchPhrase"})
                      .Aggregate({"UserID", "SearchPhrase"}, {Count("*")})
                      .OrderBy({{"count", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase LIMIT 10;
    void Query17() {
        auto df = DataFrame::Select(columnar_file_path_, {"UserID", "SearchPhrase"})
                      .Aggregate({"UserID", "SearchPhrase"}, {Count("*")})
                      .Limit(10)
                      .Collect();
        df.Display();
    }

    // SELECT UserID, extract(minute FROM EventTime) AS m, SearchPhrase, COUNT(*) FROM hits GROUP BY
    // UserID, m, SearchPhrase ORDER BY COUNT(*) DESC LIMIT 10;
    void Query18() {
        auto df = DataFrame::Select(columnar_file_path_, {})
                      .Project({ExtractMinute("EventTime", "m")})
                      .Aggregate({"UserID", "m", "SearchPhrase"}, {Count()})
                      .OrderBy({{"count", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT UserID FROM hits WHERE UserID = 435090932899640449;
    void Query19() {
        auto df = DataFrame::Select(columnar_file_path_, {"UserID"})
                      .Filter(Eq("UserID", 435090932899640449))
                      .Aggregate({"UserID"}, {Count("*")})
                      .Collect();
    }

    // SELECT COUNT(*) FROM hits WHERE URL LIKE '%google%';
    void Query20() {
        auto df = DataFrame::Select(columnar_file_path_, {"URL"})
                      .Filter(Like("URL", "%google%"))
                      .Aggregate({}, {Count()})
                      .Collect();
        df.Display();
    }

    // SELECT SearchPhrase, MIN(URL), COUNT(*) AS c FROM hits WHERE URL LIKE '%google%' AND
    // SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10;
    void Query21() {
        auto df = DataFrame::Select(columnar_file_path_, {"SearchPhrase", "URL"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .Filter(Like("URL", "%google%"))
                      .Aggregate({"SearchPhrase"}, {Min("URL"), Count("*", "c")})
                      .OrderBy({{"c", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT SearchPhrase, MIN(URL), MIN(Title), COUNT(*) AS c, COUNT(DISTINCT UserID) FROM hits
    // WHERE Title LIKE '%Google%' AND URL NOT LIKE '%.google.%' AND SearchPhrase <> '' GROUP BY
    // SearchPhrase ORDER BY c DESC LIMIT 10;
    void Query22() {
        auto df = DataFrame::Select(columnar_file_path_, {"SearchPhrase", "URL", "Title", "UserID"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .Filter(Like("Title", "%Google%"))
                      .Filter(NotLike("URL", "%.google.%"))
                      .Aggregate({"SearchPhrase"}, {Min("URL"), Min("Title"), Count("*", "c"),
                                                    DistinctCount("UserID")})
                      .OrderBy({{"c", true}}, 10)
                      .Collect();
        df.Display();
    }

    // TODO: support SELECT *
    // SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
    void Query23() {
        auto df = DataFrame::Select(columnar_file_path_, {"URL", "EventTime"})
                      .Filter(Like("URL", "%google%"))
                      .OrderBy({{"EventTime", false}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY EventTime LIMIT 10;
    void Query24() {
        auto df = DataFrame::Select(columnar_file_path_, {"SearchPhrase", "EventTime"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .OrderBy({{"EventTime", false}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY SearchPhrase LIMIT 10;
    void Query25() {
        auto df = DataFrame::Select(columnar_file_path_, {"SearchPhrase"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .OrderBy({{"SearchPhrase", false}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY EventTime, SearchPhrase LIMIT
    // 10;
    void Query26() {
        auto df = DataFrame::Select(columnar_file_path_, {"SearchPhrase", "EventTime"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .OrderBy({{"EventTime", false}, {"SearchPhrase", false}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT CounterID, AVG(length(URL)) AS l, COUNT(*) AS c FROM hits WHERE URL <> '' GROUP BY
    // CounterID HAVING COUNT(*) > 100000 ORDER BY l DESC LIMIT 25;
    void Query27() {
        auto df = DataFrame::Select(columnar_file_path_, {"CounterID", "URL"})
                      .Filter(NotEq("URL", ""))
                      .Project({Length("URL", "length_URL")})
                      .Aggregate({"CounterID"}, {Avg("length_URL", "l"), Count("*", "c")})
                      .Filter(Greater("c", static_cast<int64_t>(100000)))
                      .OrderBy({{"l", true}}, 25)
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
            case 7: Query07(); break;
            case 8: Query08(); break;
            case 9: Query09(); break;
            case 10: Query10(); break;
            case 11: Query11(); break;
            case 12: Query12(); break;
            case 13: Query13(); break;
            case 14: Query14(); break;
            case 15: Query15(); break;
            case 16: Query16(); break;
            case 17: Query17(); break;
            case 18: Query18(); break;
            case 19: Query19(); break;
            case 20: Query20(); break;
            case 21: Query21(); break;
            case 22: Query22(); break;
            case 23: Query23(); break;
            case 24: Query24(); break;
            case 25: Query25(); break;
            case 26: Query26(); break;
            case 27: Query27(); break;

            default: THROW_RUNTIME_ERROR("Unknown query number: " + std::to_string(query_number));
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
            std::cerr << "Usage: " << argv[0]
                      << " convert <csv_file> <columnar_file> <schema_file>\n";
            return 1;
        }
        try {
            std::cerr << "Converting CSV to columnar format...\n";
            const auto start = std::chrono::steady_clock::now();
            CsvToColumnar::ConvertWithSchema(argv[2], argv[3], argv[4]);
            const auto time =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() *
                1000;
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
            std::cerr << "Running query " << query_number << "...\n";
            const auto start = std::chrono::steady_clock::now();
            query.RunQuery(query_number);
            const auto time =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() *
                1000;
            std::cerr << "Query " << query_number << " completed in " << time << " ms\n";
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
