add_library(project_includes INTERFACE)
target_include_directories(project_includes INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        read_write/test
        ${CMAKE_CURRENT_SOURCE_DIR}/read_write
)

add_executable(columnar-engine
        main.cpp
        read_write/CSVTokenizer.cpp
        read_write/CSVTokenizer.h
        read_write/ColumnarReader.cpp
        read_write/ColumnarReader.h
        Types.h
        read_write/CSVReader.cpp
        read_write/CSVReader.h
        read_write/ColumnarWriter.cpp
        read_write/ColumnarWriter.h
        read_write/CSVToColumnar.cpp
        read_write/CSVToColumnar.h
        OperatorsBase.h
        Engine.cpp
        Engine.h
        Codec.h
        Query.h
        ExecutionLogic.h
        AggregationFunctions.h
        Schema.h
        macro.h
        ExecutionAPI.h
        AggregationExpressions.h
        OperatorsBase.cpp
        VectorOfStrings.cpp
        VectorOfStrings.h
        FilterExpressions.h
        FilterFunctions.h
)

target_link_libraries(columnar-engine PRIVATE project_includes)

add_executable(test_csv_parser
        read_write/test/test_csv_parser.cpp
        read_write/CSVTokenizer.cpp
        read_write/CSVReader.cpp
        VectorOfStrings.cpp
)

target_link_libraries(test_csv_parser PRIVATE
        contrib_catch_main
        project_includes)

add_executable(test_reader_writer
        read_write/test/test_reader_writer.cpp
        read_write/CSVTokenizer.cpp
        read_write/ColumnarReader.cpp
        read_write/CSVReader.cpp
        read_write/ColumnarWriter.cpp
        read_write/CSVToColumnar.cpp
        VectorOfStrings.cpp
)

target_link_libraries(test_reader_writer PRIVATE
        contrib_catch_main
        project_includes)

add_executable(test_vector_of_strings
        test_vector_of_strings.cpp
        VectorOfStrings.cpp
        VectorOfStrings.h
)

target_link_libraries(test_vector_of_strings PRIVATE
        contrib_catch_main
        project_includes
)
