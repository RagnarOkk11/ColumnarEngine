#include "column/Schema.h"
#include "column/ColumnFactory.h"

#include "io/CsvToColumnar.h"
#include "io/CsvReader.h"
#include "io/ColumnarWriter.h"

#include <memory>
#include <vector>

void WriteToColumnar(const std::string& columnar_file_path, const VectorOfStrings& column_names,
                     const std::vector<ColumnType>& column_types, CsvReader& reader,
                     std::vector<std::shared_ptr<ColumnBuilder>>& builders) {
    ColumnarWriter writer(columnar_file_path);
    writer.WriteHeader(column_names, column_types);

    static constexpr size_t kByteSize = 1 << 20;
    bool end_flag = true;
    VectorOfStrings2D column_batch;

    while (end_flag) {
        size_t cur_batch_size = 0;
        column_batch.Clear();

        while (column_batch.ApproxByteSize() < kByteSize) {
            bool has_row = reader.ReadRow(column_batch);
            if (has_row) {
                column_batch.StartNewLine();
                ++cur_batch_size;
            } else {
                end_flag = false;
                break;
            }
        }

        if (cur_batch_size == 0) {
            break;
        }

        size_t batch_width = column_batch.Width();
        for (size_t j = 0; j < batch_width; ++j) {
            builders[j]->AddBatch(column_batch, j);
        }

        std::vector<std::shared_ptr<Column>> columns;
        for (auto& builder : builders) {
            columns.push_back(builder->Finish());
        }

        if (!columns.empty()) {
            writer.WriteBatch(columns);
        }
    }

    std::move(writer).Finalize();
}

void CsvToColumnar::Convert(const std::string& csv_file_path,
                            const std::string& columnar_file_path) {
    CsvReader reader(csv_file_path);
    VectorOfStrings column_names;
    std::vector<ColumnType> column_types;
    reader.ReadHeader(column_names, column_types);

    std::vector<std::shared_ptr<ColumnBuilder>> builders;
    for (ColumnType type : column_types) {
        builders.push_back(ColumnFactory::MakeColumnBuilder(type));
    }

    WriteToColumnar(columnar_file_path, column_names, column_types, reader, builders);
}

void CsvToColumnar::ConvertWithSchema(const std::string& csv_file_path,
                                      const std::string& columnar_file_path,
                                      const std::string& schema_file_path) {
    CsvReader reader(csv_file_path);
    Schema schema = Schema::DeserializeSchema(schema_file_path);
    const std::vector<Field> fields = schema.GetFields();

    std::vector<std::shared_ptr<ColumnBuilder>> builders;
    VectorOfStrings column_names;
    std::vector<ColumnType> column_types;

    for (const Field& field : fields) {
        std::string_view header = field.name;
        ColumnType type = field.type;
        column_names.PushBack(header);
        column_types.push_back(type);
        builders.push_back(ColumnFactory::MakeColumnBuilder(type));
    }

    WriteToColumnar(columnar_file_path, column_names, column_types, reader, builders);
}
