add_library(project_includes INTERFACE)

target_link_libraries(project_includes INTERFACE
        absl::flat_hash_map
        re2::re2
)

target_include_directories(project_includes INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/column
        ${CMAKE_CURRENT_SOURCE_DIR}/execution
        ${CMAKE_CURRENT_SOURCE_DIR}/execution/expressions
        ${CMAKE_CURRENT_SOURCE_DIR}/io
        ${CMAKE_CURRENT_SOURCE_DIR}/io/test
        ${CMAKE_CURRENT_SOURCE_DIR}/types
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/test
)

find_package(Threads REQUIRED)

set(ENGINE_SOURCES
        main.cpp
        io/CsvTokenizer.cpp
        io/CsvTokenizer.h
        io/ColumnarReader.cpp
        io/ColumnarReader.h
        io/CsvReader.cpp
        io/CsvReader.h
        io/ColumnarWriter.cpp
        io/ColumnarWriter.h
        io/CsvToColumnar.cpp
        io/CsvToColumnar.h
        execution/OperatorsBase.h
        column/Codec.h
        execution/ExecutionLogic.h
        execution/ExecutionLogic.cpp
        execution/expressions/AggregationFunctions.h
        column/Schema.h
        utils/Macro.h
        execution/ExecutionApi.h
        execution/expressions/AggregationExpressions.h
        execution/OperatorsBase.cpp
        utils/VectorOfStrings.h
        execution/expressions/FilterExpressions.h
        execution/expressions/FilterFunctions.h
        execution/expressions/ScalarFunctions.h
        execution/expressions/ScalarExpressions.h
        execution/ExecutionHelper.h
        column/ColumnFactory.h
        column/CharColumn.h
        column/NumericColumn.h
        column/StringColumn.h
        column/TemporalColumn.h
        column/Column.h
        column/ColumnBuilder.h
        types/ColumnType.h
        types/Date.h
        types/Timestamp.h
        execution/expressions/AggExpHelper.h
        types/String.h
        column/ColumnView.h
        types/TimeUnit.h
        utils/Concurrency.h
        execution/Pipeline.h
        execution/PipelineExecutor.h
)

add_executable(columnar-engine ${ENGINE_SOURCES}
        utils/ThreadPool.h)
target_link_libraries(columnar-engine PRIVATE project_includes)
target_compile_options(columnar-engine PRIVATE -Werror)

add_executable(columnar-engine-mt ${ENGINE_SOURCES})
target_link_libraries(columnar-engine-mt PRIVATE project_includes Threads::Threads)
target_compile_definitions(columnar-engine-mt PRIVATE ENABLE_MULTITHREADING)
target_compile_options(columnar-engine-mt PRIVATE -Werror)

add_executable(test_csv_parser
        io/test/TestCsvParser.cpp
        io/CsvTokenizer.cpp
        io/CsvReader.cpp
)

target_link_libraries(test_csv_parser PRIVATE
        contrib_catch_main
        project_includes)

add_executable(test_reader_writer
        io/test/TestReaderWriter.cpp
        io/CsvTokenizer.cpp
        io/ColumnarReader.cpp
        io/CsvReader.cpp
        io/ColumnarWriter.cpp
        io/CsvToColumnar.cpp
)

target_link_libraries(test_reader_writer PRIVATE
        contrib_catch_main
        project_includes)

add_executable(test_vector_of_strings
        utils/test/TestVectorOfStrings.cpp
        utils/VectorOfStrings.h
)

target_link_libraries(test_vector_of_strings PRIVATE
        contrib_catch_main
        project_includes
)
