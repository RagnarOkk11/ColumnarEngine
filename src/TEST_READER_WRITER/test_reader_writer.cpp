//
// Created by ragnarokk on 04.01.2026.
//

#include "../../contrib/catch/catch.hpp"
#include <fstream>
#include <filesystem>
#include "../writer/writer.h"
#include "../reader/reader.h"
#include "../object.h"

namespace fs = std::filesystem;

std::string CreateTempCSVFile(const std::string &content, const std::string &filename = "test_rw.csv") {
    std::string filepath = "/tmp/" + filename;
    std::ofstream file(filepath);
    file << content;
    file.close();
    return filepath;
}

void RemoveTempFile(const std::string &filepath) {
    if (fs::exists(filepath)) {
        fs::remove(filepath);
    }
}

std::vector<int32_t> ExtractInt32Values(const std::unique_ptr<Column> &column) {
    Int32Column *int_col = dynamic_cast<Int32Column *>(column.get());
    if (!int_col) {
        return {};
    }

    std::vector<int32_t> result;
    std::ofstream temp_file("/tmp/temp_int.bin", std::ios::binary);
    int_col->WriteToFile(temp_file);
    temp_file.close();

    std::ifstream in_file("/tmp/temp_int.bin", std::ios::binary);
    in_file.seekg(0, std::ios::end);
    size_t size = in_file.tellg();
    in_file.seekg(0, std::ios::beg);

    result.resize(size / sizeof(int32_t));
    in_file.read(reinterpret_cast<char *>(result.data()), size);
    in_file.close();
    fs::remove("/tmp/temp_int.bin");

    return result;
}

std::vector<std::string> ExtractStringValues(const std::unique_ptr<Column> &column) {
    StringColumn *str_col = dynamic_cast<StringColumn *>(column.get());
    if (!str_col) {
        return {};
    }

    std::vector<char> buffer;
    std::ofstream temp_file("/tmp/temp_str.bin", std::ios::binary);
    str_col->WriteToFile(temp_file);
    temp_file.close();

    std::ifstream in_file("/tmp/temp_str.bin", std::ios::binary);
    in_file.seekg(0, std::ios::end);
    size_t size = in_file.tellg();
    in_file.seekg(0, std::ios::beg);

    buffer.resize(size);
    in_file.read(buffer.data(), size);
    in_file.close();
    fs::remove("/tmp/temp_str.bin");

    std::vector<std::string> result;
    size_t offset = 0;
    while (offset < buffer.size()) {
        uint32_t length;
        std::memcpy(&length, buffer.data() + offset, sizeof(length));
        offset += sizeof(length);
        std::string str(buffer.data() + offset, length);
        result.push_back(str);
        offset += length;
    }

    return result;
}

TEST_CASE("Writer: Basic write operation", "[Writer]") {
    std::string csv_content = "INT:id,STRING:name\n"
            "1,Alice\n"
            "2,Bob\n"
            "3,Charlie\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_writer_basic.csv");
    std::string output_path = "/tmp/test_writer_basic.bin";

    SECTION("Write CSV to binary format") {
        Writer writer;
        REQUIRE_NOTHROW(writer.CSVWriteData(csv_path, output_path));
        REQUIRE(fs::exists(output_path));

        std::ifstream file(output_path, std::ios::binary);
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        REQUIRE(file_size > 0);

        file.seekg(0, std::ios::beg);
        char magic[7] = {0};
        file.read(magic, 6);
        REQUIRE(std::string(magic) == "MYPAR1");

        file.close();
    }

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Writer: Multiple types", "[Writer]") {
    std::string csv_content = "INT:id,STRING:name,FLOAT:score\n"
            "1,Alice,95.5\n"
            "2,Bob,87.3\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_writer_multi.csv");
    std::string output_path = "/tmp/test_writer_multi.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    REQUIRE(fs::exists(output_path));
    REQUIRE(fs::file_size(output_path) > 0);

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Reader: Read metadata", "[Reader]") {
    std::string csv_content = "INT:id,STRING:name\n"
            "1,Alice\n"
            "2,Bob\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_reader_meta.csv");
    std::string output_path = "/tmp/test_reader_meta.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    SECTION("Read and verify metadata") {
        Reader reader(output_path);
        const auto &metadata = reader.GetMetadata();

        REQUIRE(metadata.size() == 2);
        REQUIRE(metadata[0].name == "id");
        REQUIRE(metadata[0].type == ColumnType::INT32);
        REQUIRE(metadata[1].name == "name");
        REQUIRE(metadata[1].type == ColumnType::STRING);
    }

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Reader: Read raw column data", "[Reader]") {
    std::string csv_content = "INT:value\n"
            "100\n"
            "200\n"
            "300\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_reader_raw.csv");
    std::string output_path = "/tmp/test_reader_raw.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);

    SECTION("Get raw data for column") {
        std::vector<char> raw_data = reader.GetRawColumnData(0, 0);
        REQUIRE(!raw_data.empty());
        REQUIRE(raw_data.size() == 3 * sizeof(int32_t));
    }

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Reader: Read typed column data", "[Reader]") {
    std::string csv_content = "INT:id,STRING:city\n"
            "1,Moscow\n"
            "2,London\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_reader_typed.csv");
    std::string output_path = "/tmp/test_reader_typed.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);

    SECTION("Read INT column") {
        auto column = reader.GetColumnData(0, 0);
        REQUIRE(column != nullptr);
        REQUIRE(column->GetType() == ColumnType::INT32);
        REQUIRE(column->Size() == 2);
    }

    SECTION("Read STRING column") {
        auto column = reader.GetColumnData(1, 0);
        REQUIRE(column != nullptr);
        REQUIRE(column->GetType() == ColumnType::STRING);
        REQUIRE(column->Size() == 2);
    }

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Writer+Reader: Round-trip test with INT data", "[Writer][Reader][Integration]") {
    std::string csv_content = "INT:numbers\n"
            "10\n"
            "20\n"
            "30\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_roundtrip_int.csv");
    std::string output_path = "/tmp/test_roundtrip_int.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);
    auto column = reader.GetColumnData(0, 0);

    REQUIRE(column->GetType() == ColumnType::INT32);
    REQUIRE(column->Size() == 3);

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Writer+Reader: Round-trip test with STRING data", "[Writer][Reader][Integration]") {
    std::string csv_content = "STRING:cities\n"
            "Moscow\n"
            "London\n"
            "Paris\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_roundtrip_str.csv");
    std::string output_path = "/tmp/test_roundtrip_str.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);
    auto column = reader.GetColumnData(0, 0);

    REQUIRE(column->GetType() == ColumnType::STRING);
    REQUIRE(column->Size() == 3);

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Writer+Reader: Multiple columns round-trip", "[Writer][Reader][Integration]") {
    std::string csv_content = "INT:id,STRING:name,FLOAT:score\n"
            "1,Alice,95.5\n"
            "2,Bob,87.3\n"
            "3,Charlie,92.1\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_roundtrip_multi.csv");
    std::string output_path = "/tmp/test_roundtrip_multi.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);
    const auto &metadata = reader.GetMetadata();

    SECTION("Verify metadata") {
        REQUIRE(metadata.size() == 3);
        REQUIRE(metadata[0].name == "id");
        REQUIRE(metadata[0].type == ColumnType::INT32);
        REQUIRE(metadata[1].name == "name");
        REQUIRE(metadata[1].type == ColumnType::STRING);
        REQUIRE(metadata[2].name == "score");
        REQUIRE(metadata[2].type == ColumnType::FLOAT);
    }

    SECTION("Verify all columns have data") {
        auto col0 = reader.GetColumnData(0, 0);
        auto col1 = reader.GetColumnData(1, 0);
        auto col2 = reader.GetColumnData(2, 0);

        REQUIRE(col0->Size() == 3);
        REQUIRE(col1->Size() == 3);
        REQUIRE(col2->Size() == 3);
    }

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Reader: Invalid file", "[Reader][errors]") {
    REQUIRE_THROWS_AS(Reader("/nonexistent/file.bin"), std::runtime_error);
}

TEST_CASE("Reader: Invalid chunk index", "[Reader][errors]") {
    std::string csv_content = "INT:value\n100\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_invalid_chunk.csv");
    std::string output_path = "/tmp/test_invalid_chunk.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);

    REQUIRE_THROWS_AS(reader.GetRawColumnData(0, 999), std::out_of_range);

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Writer: Empty CSV file", "[Writer]") {
    std::string csv_content = "INT:id,STRING:name\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_empty.csv");
    std::string output_path = "/tmp/test_empty.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    REQUIRE(fs::exists(output_path));

    Reader reader(output_path);
    const auto &metadata = reader.GetMetadata();
    REQUIRE(metadata.size() == 2);

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Writer+Reader: Large dataset", "[Writer][Reader][Integration]") {
    std::string csv_content = "INT:id,STRING:name\n";
    for (int i = 0; i < 1000; ++i) {
        csv_content += std::to_string(i) + ",User" + std::to_string(i) + "\n";
    }

    std::string csv_path = CreateTempCSVFile(csv_content, "test_large.csv");
    std::string output_path = "/tmp/test_large.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);
    auto col0 = reader.GetColumnData(0, 0);
    auto col1 = reader.GetColumnData(1, 0);

    REQUIRE(col0->Size() == 1000);
    REQUIRE(col1->Size() == 1000);

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Writer+Reader: Special characters in strings", "[Writer][Reader][Integration]") {
    std::string csv_content = "STRING:text\n"
            "\"Hello, World!\"\n"
            "Simple text\n"
            "\"Text with, comma\"\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_special.csv");
    std::string output_path = "/tmp/test_special.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);
    auto column = reader.GetColumnData(0, 0);

    REQUIRE(column->GetType() == ColumnType::STRING);
    REQUIRE(column->Size() == 3);

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Writer+Reader: Float precision", "[Writer][Reader][Integration]") {
    std::string csv_content = "FLOAT:values\n"
            "3.14159\n"
            "2.71828\n"
            "1.618\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_float.csv");
    std::string output_path = "/tmp/test_float.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);
    auto column = reader.GetColumnData(0, 0);

    REQUIRE(column->GetType() == ColumnType::FLOAT);
    REQUIRE(column->Size() == 3);

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}

TEST_CASE("Reader: Metadata chunks count", "[Reader]") {
    std::string csv_content = "INT:id\n1\n2\n3\n";
    std::string csv_path = CreateTempCSVFile(csv_content, "test_chunks.csv");
    std::string output_path = "/tmp/test_chunks.bin";

    Writer writer;
    writer.CSVWriteData(csv_path, output_path);

    Reader reader(output_path);
    const auto &metadata = reader.GetMetadata();

    REQUIRE(metadata[0].offsets.size() == metadata[0].sizes.size());
    REQUIRE(!metadata[0].offsets.empty());

    RemoveTempFile(csv_path);
    RemoveTempFile(output_path);
}
