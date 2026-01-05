add_executable(columnar-engine
        main.cpp)

add_library(project_includes INTERFACE)
target_include_directories(project_includes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/csv_parser
    ${CMAKE_CURRENT_SOURCE_DIR}/reader
    ${CMAKE_CURRENT_SOURCE_DIR}/writer)

target_link_libraries(columnar-engine PRIVATE project_includes)

add_executable(test_csv_parser
        TEST_SCV_PARSER/test_csv_parser.cpp
        csv_parser/CSVTokenizer.cpp
        csv_parser/CSVTokenizer.h
        object.h
        csv_parser/RawCSVReader.cpp
        csv_parser/RawCSVReader.h
        writer/ColumnarWriter.cpp
        writer/ColumnarWriter.h
        writer/CSVToColumnar.cpp
        writer/CSVToColumnar.h)

target_link_libraries(test_csv_parser PRIVATE
        contrib_catch_main
        project_includes)

add_executable(test_reader_writer
        TEST_READER_WRITER/test_reader_writer.cpp
        csv_parser/CSVTokenizer.cpp
        csv_parser/CSVTokenizer.h
        reader/reader.cpp
        reader/reader.h
        object.h
        csv_parser/RawCSVReader.cpp
        csv_parser/RawCSVReader.h
        writer/ColumnarWriter.cpp
        writer/ColumnarWriter.h
        writer/CSVToColumnar.cpp
        writer/CSVToColumnar.h)

target_link_libraries(test_reader_writer PRIVATE
        contrib_catch_main
        project_includes)
