//
// Created by ragnarokk on 04.01.2026.
//

#include <catch.hpp>
#include <fstream>
#include <filesystem>
#include "CSVTokenizer.h"
#include "RawCSVReader.h"

namespace fs = std::filesystem;

std::string CreateTempCSVFile(const std::string& content, const std::string& filename) {
    std::string path = "/tmp/" + filename;
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}

TEST_CASE("CSVTokenizer: Basic Tokenization", "[CSVTokenizer]") {
    std::string content = "col1,col2\nval1,val2";
    std::string path = CreateTempCSVFile(content, "test_tokenizer_basic.csv");

    CSVTokenizer tokenizer(path);

    REQUIRE(tokenizer.GetNextToken() == "col1");
    REQUIRE_FALSE(tokenizer.IsEndOfLine());

    REQUIRE(tokenizer.GetNextToken() == "col2");
    REQUIRE(tokenizer.IsEndOfLine());

    tokenizer.ResetLineFlag();
    REQUIRE_FALSE(tokenizer.IsEndOfLine());

    REQUIRE(tokenizer.GetNextToken() == "val1");
    REQUIRE_FALSE(tokenizer.IsEndOfLine());

    REQUIRE(tokenizer.GetNextToken() == "val2");
    REQUIRE(tokenizer.IsEndOfLine());

    tokenizer.ResetLineFlag();
    REQUIRE(tokenizer.GetNextToken().empty());
    REQUIRE(tokenizer.IsEOF());

    fs::remove(path);
}

TEST_CASE("CSVTokenizer: Empty Values", "[CSVTokenizer]") {
    std::string content = "col1,,col3\n,val2,";
    std::string path = CreateTempCSVFile(content, "test_tokenizer_empty.csv");

    CSVTokenizer tokenizer(path);

    REQUIRE(tokenizer.GetNextToken() == "col1");
    REQUIRE(tokenizer.GetNextToken().empty());
    REQUIRE(tokenizer.GetNextToken() == "col3");
    REQUIRE(tokenizer.IsEndOfLine());
    tokenizer.ResetLineFlag();

    REQUIRE(tokenizer.GetNextToken().empty());
    REQUIRE(tokenizer.GetNextToken() == "val2");
    REQUIRE(tokenizer.GetNextToken().empty());
    REQUIRE(tokenizer.IsEndOfLine());

    fs::remove(path);
}

TEST_CASE("CSVTokenizer: Custom Delimiter", "[CSVTokenizer]") {
    std::string content = "col1|col2\nval1|val2";
    std::string path = CreateTempCSVFile(content, "test_tokenizer_delim.csv");

    CSVTokenizer tokenizer(path, '|');

    REQUIRE(tokenizer.GetNextToken() == "col1");
    REQUIRE(tokenizer.GetNextToken() == "col2");
    REQUIRE(tokenizer.IsEndOfLine());
    tokenizer.ResetLineFlag();

    REQUIRE(tokenizer.GetNextToken() == "val1");
    REQUIRE(tokenizer.GetNextToken() == "val2");
    REQUIRE(tokenizer.IsEndOfLine());

    fs::remove(path);
}

TEST_CASE("RawCSVReader: Read Header and Rows", "[RawCSVReader]") {
    std::string content = "HEADER1,HEADER2,HEADER3\n1,2,3\n4,5,6";
    std::string path = CreateTempCSVFile(content, "test_reader.csv");

    RawCSVReader reader(path);

    auto header = reader.ReadHeader();
    REQUIRE(header.size() == 3);
    REQUIRE(header[0] == "HEADER1");
    REQUIRE(header[1] == "HEADER2");
    REQUIRE(header[2] == "HEADER3");

    std::vector<std::string> row;

    REQUIRE(reader.ReadRow(row));
    REQUIRE(row.size() == 3);
    REQUIRE(row[0] == "1");
    REQUIRE(row[1] == "2");
    REQUIRE(row[2] == "3");

    REQUIRE(reader.ReadRow(row));
    REQUIRE(row.size() == 3);
    REQUIRE(row[0] == "4");
    REQUIRE(row[1] == "5");
    REQUIRE(row[2] == "6");

    REQUIRE_FALSE(reader.ReadRow(row));

    fs::remove(path);
}
