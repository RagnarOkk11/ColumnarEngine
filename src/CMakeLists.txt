add_executable(columnar-engine
        main.cpp)

add_executable(test_csv_parser
        TEST_SCV_PARSER/test_csv_parser.cpp
        csv_parser/CSVParser.cpp
        csv_parser/CSVParser.h
        csv_parser/CSVTokenizer.cpp
        csv_parser/CSVTokenizer.h
        object.h)

target_link_libraries(test_csv_parser
        contrib_catch_main)

add_executable(test_reader_writer
        TEST_READER_WRITER/test_reader_writer.cpp
        csv_parser/CSVParser.cpp
        csv_parser/CSVParser.h
        csv_parser/CSVTokenizer.cpp
        csv_parser/CSVTokenizer.h
        reader/reader.cpp
        reader/reader.h
        writer/writer.cpp
        writer/writer.h
        object.h)

target_link_libraries(test_reader_writer
        contrib_catch_main)
