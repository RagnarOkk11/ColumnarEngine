//
// Created by ragnarokk on 06.01.2026.
//

#include <iostream>

#include "CSVToColumnar.h"
#include "engine.h"
#include "operators_base.h"
#include "reader.h"

void Engine::InitDataFromCSV(const std::string& csv_file_path, const std::string& columnar_file_path) {
    CSVToColumnar converter;
    converter.Convert(csv_file_path, columnar_file_path);
}

void Engine::ExecuteQuery1(const std::string& columnar_file_path) {
    Reader reader(columnar_file_path);
    auto scan = std::make_unique<ScanOperator>(reader, std::vector<std::string>{"prices"});

    ColumnType type = reader.GetColumnTypeByName("prices");

    std::unique_ptr<Operator> aggregation;
    switch (type) {
        case ColumnType::INT32:
            aggregation = std::make_unique<AggregationOperator<Int32Column>>(std::move(scan));
            break;
        case ColumnType::FLOAT:
            aggregation = std::make_unique<AggregationOperator<FloatColumn>>(std::move(scan));
            break;
        default:
            throw std::runtime_error("Unsupported column type for aggregation");
    }

    auto result_batch = aggregation->Run();
    if (result_batch && !result_batch->columns.empty()) {
        Column* sum_column = result_batch->columns[0].get();
        Column* count_column = result_batch->columns[1].get();
        std::string sum_str = sum_column->GetDataAsString(0);
        std::string count_str = count_column->GetDataAsString(0);
        std::cout << "Sum: " << sum_str << ", Count: " << count_str << std::endl;
    }
}
