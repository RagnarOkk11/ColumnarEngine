//
// Created by ragnarokk on 30.03.2026.
//

#ifndef COLUMNAR_ENGINE_SCHEMA_H
#define COLUMNAR_ENGINE_SCHEMA_H

#include "io/ColumnarReader.h"
#include "types/ColumnType.h"
#include "utils/Macro.h"

#include <string>
#include <sstream>
#include <fstream>

struct Field {
    std::string name;
    ColumnType type;
};

class Schema {
public:
    static constexpr char kDelimiter = ',';

    Schema() = default;
    explicit Schema(std::vector<Field>&& fields) : fields_(std::move(fields)) {
    }

    void AddField(Field&& field) {
        fields_.emplace_back(std::move(field));
    }

    void AddColumn(const std::string& name, ColumnType type) {
        fields_.emplace_back(Field{name, type});
    }

    const std::vector<Field>& GetFields() const {
        return fields_;
    }

    ColumnType GetColumnTypeByName(const std::string& column_name) const {
        for (const Field& field : fields_) {
            if (field.name == column_name) {
                return field.type;
            }
        }
        THROW_RUNTIME_ERROR("Column " + column_name + " not found in schema");
    }

    size_t GetColumnIndexByName(const std::string& column_name) const {
        for (size_t i = 0; i < fields_.size(); ++i) {
            if (fields_[i].name == column_name) {
                return i;
            }
        }
        THROW_RUNTIME_ERROR("Column " + column_name + " not found in schema");
    }

    static Schema DeserializeSchema(const std::string& schema_file_path) {
        std::vector<Field> fields;
        std::ifstream file(schema_file_path);
        if (!file.is_open()) {
            THROW_RUNTIME_ERROR("Could not open schema file: " + schema_file_path);
        }
        std::string field;
        while (std::getline(file, field, '\n')) {
            if (field.empty() || field.front() == '#') {
                continue;
            }
            fields.emplace_back(DeserializeField(field));
        }
        return Schema(std::move(fields));
    }

private:
    std::vector<Field> fields_;

    static Field DeserializeField(const std::string& field_str) {
        std::istringstream ss(field_str);
        std::string name, type_str;
        if (!std::getline(ss, name, kDelimiter) || !std::getline(ss, type_str, kDelimiter)) {
            THROW_RUNTIME_ERROR("Invalid field format in schema");
        }
        if (!type_str.empty() && type_str.back() == '\r') {
            type_str.pop_back();
        }

#define HANDLE_TYPE(ENUM_VAL, STR_VAL, CLASS_TYPE) \
    if (type_str == STR_VAL) {                     \
        return {name, ColumnType::ENUM_VAL};       \
    }
        FOR_EACH_COLUMN_TYPE(HANDLE_TYPE)
#undef HANDLE_TYPE
        THROW_RUNTIME_ERROR("Unsupported column type in schema: " + type_str);
    }
};

#endif  // COLUMNAR_ENGINE_SCHEMA_H
