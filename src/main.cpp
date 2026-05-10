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

    // SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
    void Query23() {
        auto df = DataFrame::Select(columnar_file_path_, {"*"})
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

    // TODO:
    // SELECT REGEXP_REPLACE(Referer, '^https?://(?:www\.)?([^/]+)/.*$', '\1') AS k,
    // AVG(length(Referer)) AS l, COUNT(*) AS c, MIN(Referer) FROM hits WHERE Referer <> '' GROUP BY
    // k HAVING COUNT(*) > 100000 ORDER BY l DESC LIMIT 25;
    void Query28() {
        THROW_NOT_IMPLEMENTED;
    }

    // WARNING: DataFrame::Display output does not affect that query
    // SELECT SUM(ResolutionWidth), SUM(ResolutionWidth + 1), SUM(ResolutionWidth + 2),
    // SUM(ResolutionWidth + 3), SUM(ResolutionWidth + 4), SUM(ResolutionWidth + 5),
    // SUM(ResolutionWidth + 6), SUM(ResolutionWidth + 7), SUM(ResolutionWidth + 8),
    // SUM(ResolutionWidth + 9), SUM(ResolutionWidth + 10), SUM(ResolutionWidth + 11),
    // SUM(ResolutionWidth + 12), SUM(ResolutionWidth + 13), SUM(ResolutionWidth + 14),
    // SUM(ResolutionWidth + 15), SUM(ResolutionWidth + 16), SUM(ResolutionWidth + 17),
    // SUM(ResolutionWidth + 18), SUM(ResolutionWidth + 19), SUM(ResolutionWidth + 20),
    // SUM(ResolutionWidth + 21), SUM(ResolutionWidth + 22), SUM(ResolutionWidth + 23),
    // SUM(ResolutionWidth + 24), SUM(ResolutionWidth + 25), SUM(ResolutionWidth + 26),
    // SUM(ResolutionWidth + 27), SUM(ResolutionWidth + 28), SUM(ResolutionWidth + 29),
    // SUM(ResolutionWidth + 30), SUM(ResolutionWidth + 31), SUM(ResolutionWidth + 32),
    // SUM(ResolutionWidth + 33), SUM(ResolutionWidth + 34), SUM(ResolutionWidth + 35),
    // SUM(ResolutionWidth + 36), SUM(ResolutionWidth + 37), SUM(ResolutionWidth + 38),
    // SUM(ResolutionWidth + 39), SUM(ResolutionWidth + 40), SUM(ResolutionWidth + 41),
    // SUM(ResolutionWidth + 42), SUM(ResolutionWidth + 43), SUM(ResolutionWidth + 44),
    // SUM(ResolutionWidth + 45), SUM(ResolutionWidth + 46), SUM(ResolutionWidth + 47),
    // SUM(ResolutionWidth + 48), SUM(ResolutionWidth + 49), SUM(ResolutionWidth + 50),
    // SUM(ResolutionWidth + 51), SUM(ResolutionWidth + 52), SUM(ResolutionWidth + 53),
    // SUM(ResolutionWidth + 54), SUM(ResolutionWidth + 55), SUM(ResolutionWidth + 56),
    // SUM(ResolutionWidth + 57), SUM(ResolutionWidth + 58), SUM(ResolutionWidth + 59),
    // SUM(ResolutionWidth + 60), SUM(ResolutionWidth + 61), SUM(ResolutionWidth + 62),
    // SUM(ResolutionWidth + 63), SUM(ResolutionWidth + 64), SUM(ResolutionWidth + 65),
    // SUM(ResolutionWidth + 66), SUM(ResolutionWidth + 67), SUM(ResolutionWidth + 68),
    // SUM(ResolutionWidth + 69), SUM(ResolutionWidth + 70), SUM(ResolutionWidth + 71),
    // SUM(ResolutionWidth + 72), SUM(ResolutionWidth + 73), SUM(ResolutionWidth + 74),
    // SUM(ResolutionWidth + 75), SUM(ResolutionWidth + 76), SUM(ResolutionWidth + 77),
    // SUM(ResolutionWidth + 78), SUM(ResolutionWidth + 79), SUM(ResolutionWidth + 80),
    // SUM(ResolutionWidth + 81), SUM(ResolutionWidth + 82), SUM(ResolutionWidth + 83),
    // SUM(ResolutionWidth + 84), SUM(ResolutionWidth + 85), SUM(ResolutionWidth + 86),
    // SUM(ResolutionWidth + 87), SUM(ResolutionWidth + 88), SUM(ResolutionWidth + 89) FROM hits;
    void Query29() {
        auto df = DataFrame::Select(columnar_file_path_, {"ResolutionWidth"})
                      .Aggregate({}, {Sum("ResolutionWidth", "a"), Count("*", "b")})
                      .Collect();

        std::shared_ptr<Column> cnt = df.GetResult("b");
        std::shared_ptr<Column> sum = df.GetResult("a");

        const auto* cnt_vec = static_cast<const std::vector<int64_t>*>(cnt->GetRawData());
        int64_t delta = (*cnt_vec)[0];

        int64_t cur = 0;
        const auto* sum_vec = static_cast<const std::vector<int32_t>*>(sum->GetRawData());
        cur = (*sum_vec)[0];

        for (size_t i = 0; i < 90; ++i) {
            std::cout << cur << ",";
            cur += delta;
        }
        std::cout << "\n";
    }

    // SELECT SearchEngineID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM
    // hits WHERE SearchPhrase <> '' GROUP BY SearchEngineID, ClientIP ORDER BY c DESC LIMIT 10;
    void Query30() {
        auto df = DataFrame::Select(columnar_file_path_,
                                    {"SearchEngineID", "ClientIP", "IsRefresh", "ResolutionWidth"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .Aggregate({"SearchEngineID", "ClientIP"},
                                 {Count("*", "c"), Sum("IsRefresh", "sum_refresh"),
                                  Avg("ResolutionWidth", "avg_res_width")})
                      .OrderBy({{"c", true}}, 10)
                      .Collect();

        df.Display();
    }

    // SELECT WatchID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits WHERE
    // SearchPhrase <> '' GROUP BY WatchID, ClientIP ORDER BY c DESC LIMIT 10;
    void Query31() {
        auto df = DataFrame::Select(columnar_file_path_,
                                    {"WatchID", "ClientIP", "IsRefresh", "ResolutionWidth"})
                      .Filter(NotEq("SearchPhrase", ""))
                      .Aggregate({"WatchID", "ClientIP"},
                                 {Count("*", "c"), Sum("IsRefresh", "sum_refresh"),
                                  Avg("ResolutionWidth", "avg_res_width")})
                      .OrderBy({{"c", true}}, 10)
                      .Collect();

        df.Display();
    }

    // SELECT WatchID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits GROUP
    // BY WatchID, ClientIP ORDER BY c DESC LIMIT 10;
    void Query32() {
        auto df = DataFrame::Select(columnar_file_path_,
                                    {"WatchID", "ClientIP", "IsRefresh", "ResolutionWidth"})
                      .Aggregate({"WatchID", "ClientIP"},
                                 {Count("*", "c"), Sum("IsRefresh", "sum_refresh"),
                                  Avg("ResolutionWidth", "avg_res_width")})
                      .OrderBy({{"c", true}}, 10)
                      .Collect();

        df.Display();
    }

    // SELECT URL, COUNT(*) AS c FROM hits GROUP BY URL ORDER BY c DESC LIMIT 10;
    void Query33() {
        auto df = DataFrame::Select(columnar_file_path_, {"URL"})
                      .Aggregate({"URL"}, {Count("*", "c")})
                      .OrderBy({{"c", true}}, 10)
                      .Collect();
        df.Display();
    }

    // SELECT 1, URL, COUNT(*) AS c FROM hits GROUP BY 1, URL ORDER BY c DESC LIMIT 10;
    void Query34() {
        auto df = DataFrame::Select(columnar_file_path_, {"URL"})
                      .Aggregate({"URL"}, {Count("*", "c")})
                      .OrderBy({{"c", true}}, 10)
                      .Project({Literal<int16_t>("const_1", 1)})
                      .Reoder({"const_1", "URL", "c"})
                      .Collect();
        df.Display();
    }

    // SELECT ClientIP, ClientIP - 1, ClientIP - 2, ClientIP - 3, COUNT(*) AS c FROM hits GROUP BY
    // ClientIP, ClientIP - 1, ClientIP - 2, ClientIP - 3 ORDER BY c DESC LIMIT 10;
    void Query35() {
        auto df = DataFrame::Select(columnar_file_path_, {"ClientIP"})
                      .Aggregate({"ClientIP"}, {Count("*", "c")})
                      .OrderBy({{"c", true}}, 10)
                      .Project({AddConst<int32_t>("ClientIP", -1, "ClientIP_minus_1"),
                                AddConst<int32_t>("ClientIP", -2, "ClientIP_minus_2"),
                                AddConst<int32_t>("ClientIP", -3, "ClientIP_minus_3")})
                      .Reoder({"ClientIP", "ClientIP_minus_1", "ClientIP_minus_2",
                               "ClientIP_minus_3", "c"})
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
            case 28: Query28(); break;
            case 29: Query29(); break;
            case 30: Query30(); break;
            case 31: Query31(); break;
            case 32: Query32(); break;
            case 33: Query33(); break;
            case 34: Query34(); break;
            case 35: Query35(); break;

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
