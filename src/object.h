//
// Created by ragnarokk on 03.01.2026.
//

#ifndef COLUMNAR_ENGINE_OBJECT_H
#define COLUMNAR_ENGINE_OBJECT_H

#include <string>
#include <vector>

enum class ColumnType : uint8_t {
    INT32,
    FLOAT,
    STRING,
    DATE,
    TIMESTAMP
};

class Column {
public:
    virtual ~Column() = default;

    virtual ColumnType GetType() const = 0;

    virtual size_t Size() const = 0;

    virtual void Add(const std::string& value) = 0;

    virtual void WriteToFile(std::ofstream &file) = 0; // TODO: implement
};

class Int32Column : public Column {
public:
    ColumnType GetType() const override {
        return ColumnType::INT32;
    }

    size_t Size() const override {
        return data_.size();
    }

    void Add(const std::string& value) override {
        data_.push_back(std::stoi(value));
    }

    void WriteToFile(std::ofstream &file) override {
        for (const auto& val : data_) {
            file.write(reinterpret_cast<const char*>(&val), sizeof(val));
        }
    }

private:
    std::vector<int32_t> data_;
};

class FloatColumn : public Column {
public:
    ColumnType GetType() const override {
        return ColumnType::FLOAT;
    }

    size_t Size() const override {
        return data_.size();
    }

    void Add(const std::string& value) override {
        data_.push_back(std::stof(value));
    }

    void WriteToFile(std::ofstream &file) override {
        for (const auto& val : data_) {
            file.write(reinterpret_cast<const char*>(&val), sizeof(val));
        }
    }

private:
    std::vector<float> data_;
};

class StringColumn : public Column {
public:
    ColumnType GetType() const override {
        return ColumnType::STRING;
    }

    size_t Size() const override {
        return data_.size();
    }

    void Add(const std::string& value) override {
        data_.push_back(value);
    }

    void WriteToFile(std::ofstream &file) override {
        for (const auto& str : data_) {
            uint32_t length = str.size();
            file.write(reinterpret_cast<const char*>(&length), sizeof(length));
            file.write(str.data(), length);
        }
    }

private:
    std::vector<std::string> data_;
};

class DateColumn : public Column {
public:
    ColumnType GetType() const override {
        return ColumnType::DATE;
    }

    size_t Size() const override {
        return data_.size();
    }

    void Add([[maybe_unused]] const std::string& value) override {
        // TODO
    }

    void WriteToFile([[maybe_unused]] std::ofstream &file) override {
        // TODO
    }

private:
    std::vector<std::string> data_;
    // TODO: Implement date-specific methods and storage
};

class TimestampColumn : public Column {
public:
    ColumnType GetType() const override {
        return ColumnType::TIMESTAMP;
    }

    size_t Size() const override {
        return data_.size();
    }

    void Add([[maybe_unused]] const std::string &value) override {
        // TODO
    }

    void WriteToFile([[maybe_unused]] std::ofstream &file) override {
        // TODO
    }
private:
    std::vector<std::string> data_;
    // TODO: Implement timestamp-specific methods and storage
};

#endif //COLUMNAR_ENGINE_OBJECT_H
