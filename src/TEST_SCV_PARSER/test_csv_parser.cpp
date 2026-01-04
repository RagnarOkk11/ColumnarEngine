//
// Created by ragnarokk on 04.01.2026.
//

#include <catch.hpp>
#include <fstream>
#include <filesystem>
#include "../csv_parser/CSVParser.h"
#include "../object.h"

namespace fs = std::filesystem;

std::string CreateTempCSVFile(const std::string& content, const std::string& filename = "test.csv") {
    std::string filepath = "/tmp/" + filename;
    std::ofstream file(filepath);
    file << content;
    file.close();
    return filepath;
}

void RemoveTempFile(const std::string& filepath) {
    if (fs::exists(filepath)) {
        fs::remove(filepath);
    }
}

TEST_CASE("CSVParser: Basic parsing with INT and STRING columns", "[CSVParser]") {
    std::string content = "STRING:cities,INT:prices\n"
                         "Moscow,100\n"
                         "London,200\n";
    std::string filepath = CreateTempCSVFile(content);

    SECTION("Parse header correctly") {
        CSVParser parser(filepath);
        auto [names, columns] = parser.CreateColumnStructure();

        REQUIRE(names.size() == 2);
        REQUIRE(names[0] == "cities");
        REQUIRE(names[1] == "prices");

        REQUIRE(columns.size() == 2);
        REQUIRE(columns[0]->GetType() == ColumnType::STRING);
        REQUIRE(columns[1]->GetType() == ColumnType::INT32);
    }

    SECTION("Parse data correctly") {
        CSVParser parser(filepath);
        auto [names, columns] = parser.CreateColumnStructure();

        bool has_more = parser.ReadNextBatch(columns);

        REQUIRE(columns[0]->Size() == 2);
        REQUIRE(columns[1]->Size() == 2);
        REQUIRE(has_more == true);

        has_more = parser.ReadNextBatch(columns);
        REQUIRE(columns[0]->Size() == 0);
        REQUIRE(columns[1]->Size() == 0);
        REQUIRE(has_more == false);
    }

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Multiple types", "[CSVParser]") {
    std::string content = "INT:id,STRING:name,FLOAT:score\n"
                         "1,Alice,95.5\n"
                         "2,Bob,87.3\n"
                         "3,Charlie,92.1\n";
    std::string filepath = CreateTempCSVFile(content, "test_multi.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();

    SECTION("Header parsing") {
        REQUIRE(names.size() == 3);
        REQUIRE(names[0] == "id");
        REQUIRE(names[1] == "name");
        REQUIRE(names[2] == "score");

        REQUIRE(columns[0]->GetType() == ColumnType::INT32);
        REQUIRE(columns[1]->GetType() == ColumnType::STRING);
        REQUIRE(columns[2]->GetType() == ColumnType::FLOAT);
    }

    SECTION("Data parsing") {
        parser.ReadNextBatch(columns);

        REQUIRE(columns[0]->Size() == 3);
        REQUIRE(columns[1]->Size() == 3);
        REQUIRE(columns[2]->Size() == 3);
    }

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Whitespace handling", "[CSVParser]") {
    std::string content = "STRING:city , INT:price \n"
                         " Moscow , 100 \n"
                         " London , 200 \n";
    std::string filepath = CreateTempCSVFile(content, "test_whitespace.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();

    SECTION("Header with whitespace") {
        REQUIRE(names.size() == 2);
        REQUIRE(names[0] == "city");
        REQUIRE(names[1] == "price");
    }

    SECTION("Data with whitespace") {
        parser.ReadNextBatch(columns);
        REQUIRE(columns[0]->Size() == 2);
        REQUIRE(columns[1]->Size() == 2);
    }

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Quoted strings", "[CSVParser]") {
    std::string content = "STRING:name,INT:value\n"
                         "\"New York\",100\n"
                         "\"Los Angeles\",200\n";
    std::string filepath = CreateTempCSVFile(content, "test_quotes.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();

    parser.ReadNextBatch(columns);

    REQUIRE(columns[0]->Size() == 2);
    REQUIRE(columns[1]->Size() == 2);

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Batch reading", "[CSVParser]") {
    std::string content = "INT:id,STRING:name\n"
                         "1,Alice\n"
                         "2,Bob\n"
                         "3,Charlie\n"
                         "4,David\n"
                         "5,Eve\n";
    std::string filepath = CreateTempCSVFile(content, "test_batch.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();

    SECTION("Read in batches") {
        bool has_more = parser.ReadNextBatch(columns, 2);
        REQUIRE(has_more == true);
        REQUIRE(columns[0]->Size() == 2);

        has_more = parser.ReadNextBatch(columns, 2);
        REQUIRE(has_more == true);
        REQUIRE(columns[0]->Size() == 2);

        has_more = parser.ReadNextBatch(columns, 2);
        REQUIRE(has_more == true);
        REQUIRE(columns[0]->Size() == 1);

        has_more = parser.ReadNextBatch(columns, 2);
        REQUIRE(has_more == false);
        REQUIRE(columns[0]->Size() == 0);
    }

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Empty file", "[CSVParser]") {
    std::string content = "STRING:name,INT:value\n";
    std::string filepath = CreateTempCSVFile(content, "test_empty.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();

    REQUIRE(names.size() == 2);
    REQUIRE(columns.size() == 2);

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Single row", "[CSVParser]") {
    std::string content = "STRING:city,INT:price\n"
                         "Moscow,100\n";
    std::string filepath = CreateTempCSVFile(content, "test_single.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();
    parser.ReadNextBatch(columns);

    REQUIRE(columns[0]->Size() == 1);
    REQUIRE(columns[1]->Size() == 1);

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: INT32 alias", "[CSVParser]") {
    std::string content = "INT32:id,STRING:name\n"
                         "1,Alice\n"
                         "2,Bob\n";
    std::string filepath = CreateTempCSVFile(content, "test_int32.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();

    REQUIRE(columns[0]->GetType() == ColumnType::INT32);
    parser.ReadNextBatch(columns);
    REQUIRE(columns[0]->Size() == 2);

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Invalid format - file not found", "[CSVParser][errors]") {
    REQUIRE_THROWS_AS(CSVParser("/nonexistent/file.csv"), std::runtime_error);
}

TEST_CASE("CSVParser: Invalid format - unknown type", "[CSVParser][errors]") {
    std::string content = "UNKNOWN:field,INT:value\n"
                         "data,100\n";
    std::string filepath = CreateTempCSVFile(content, "test_unknown.csv");

    CSVParser parser(filepath);
    REQUIRE_THROWS_AS(parser.CreateColumnStructure(), std::runtime_error);

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Invalid format - missing colon", "[CSVParser][errors]") {
    std::string content = "STRINGname,INT:value\n"
                         "test,100\n";
    std::string filepath = CreateTempCSVFile(content, "test_no_colon.csv");

    CSVParser parser(filepath);
    REQUIRE_THROWS_AS(parser.CreateColumnStructure(), std::runtime_error);

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Complex string values", "[CSVParser]") {
    std::string content = "STRING:description,INT:count\n"
                         "Simple text,10\n"
                         "\"Text with, comma\",20\n"
                         "Regular text,30\n";
    std::string filepath = CreateTempCSVFile(content, "test_complex.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();
    parser.ReadNextBatch(columns);

    REQUIRE(columns[0]->Size() == 3);
    REQUIRE(columns[1]->Size() == 3);

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Float values", "[CSVParser]") {
    std::string content = "STRING:name,FLOAT:value\n"
                         "pi,3.14159\n"
                         "e,2.71828\n"
                         "golden,1.618\n";
    std::string filepath = CreateTempCSVFile(content, "test_float.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();
    parser.ReadNextBatch(columns);

    REQUIRE(columns[0]->Size() == 3);
    REQUIRE(columns[1]->Size() == 3);
    REQUIRE(columns[1]->GetType() == ColumnType::FLOAT);

    RemoveTempFile(filepath);
}

TEST_CASE("CSVParser: Quoted header names", "[CSVParser]") {
    std::string content = "STRING:\"city name\",INT:\"price value\"\n"
                         "Moscow,100\n"
                         "London,200\n";
    std::string filepath = CreateTempCSVFile(content, "test_quoted_header.csv");

    CSVParser parser(filepath);
    auto [names, columns] = parser.CreateColumnStructure();

    REQUIRE(names.size() == 2);
    REQUIRE(names[0] == "city name");
    REQUIRE(names[1] == "price value");

    parser.ReadNextBatch(columns);
    REQUIRE(columns[0]->Size() == 2);

    RemoveTempFile(filepath);
}
