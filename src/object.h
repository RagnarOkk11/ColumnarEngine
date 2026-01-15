//
// Created by ragnarokk on 03.01.2026.
//

#ifndef COLUMNAR_ENGINE_OBJECT_H
#define COLUMNAR_ENGINE_OBJECT_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

enum class ColumnType : uint8_t {
    INT32,
    FLOAT,
    STRING,
    DATE,
    TIMESTAMP
};

struct ColumnMetadata {
    std::string name;
    ColumnType type;
    std::vector<uint64_t> offsets;
    std::vector<uint64_t> sizes;
};

class Column {
public:
    virtual ~Column() = default;

    virtual ColumnType GetType() const = 0;

    virtual size_t Size() const = 0;

    virtual void Add(const std::string& value) = 0;

    virtual uint64_t WriteToFile(std::ofstream& file) = 0;

    virtual std::string GetDataAsString(size_t index) const = 0;

    virtual void ReadFromRawData(const std::vector<char>& buffer) = 0;

    virtual void Clear() = 0;
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

    // TODO: add decoding logic
    uint64_t WriteToFile(std::ofstream& file) override {
        for (const auto& val : data_) {
            file.write(reinterpret_cast<const char*>(&val), sizeof(val));
        }
        return data_.size() * sizeof(int32_t);
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of range");
        }
        return std::to_string(data_[index]);
    }

    void ReadFromRawData(const std::vector<char>& buffer) override {
        size_t num_ints = buffer.size() / sizeof(int32_t);
        data_.resize(num_ints);
        std::memcpy(data_.data(), buffer.data(), buffer.size());
    }

    void Clear() override {
        data_.clear();
    }

    void Add(int32_t value) {
        data_.push_back(value);
    }

    const std::vector<int32_t>& GetData() const {
        return data_;
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

    uint64_t WriteToFile(std::ofstream& file) override {
        for (const auto& val : data_) {
            file.write(reinterpret_cast<const char*>(&val), sizeof(val));
        }
        return data_.size() * sizeof(float);
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of range");
        }
        return std::to_string(data_[index]);
    }

    void ReadFromRawData(const std::vector<char>& buffer) override {
        size_t num_floats = buffer.size() / sizeof(float);
        data_.resize(num_floats);
        std::memcpy(data_.data(), buffer.data(), buffer.size());
    }

    void Clear() override {
        data_.clear();
    }

    void Add(float value) {
        data_.push_back(value);
    }

    const std::vector<float>& GetData() const {
        return data_;
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

    uint64_t WriteToFile(std::ofstream& file) override {
        uint64_t total_size = 0;
        for (const auto& str : data_) {
            uint32_t length = str.size();
            file.write(reinterpret_cast<const char*>(&length), sizeof(length));
            file.write(str.data(), length);
            total_size += sizeof(length) + length;
        }
        return total_size;
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    void ReadFromRawData(const std::vector<char>& buffer) override {
        data_.clear();
        size_t offset = 0;
        while (offset < buffer.size()) {
            uint32_t length;
            std::memcpy(&length, buffer.data() + offset, sizeof(length));
            offset += sizeof(length);
            std::string str(buffer.data() + offset, length);
            data_.push_back(str);
            offset += length;
        }
    }

    void Clear() override {
        data_.clear();
    }

    void AddString(const std::string& value) {
        data_.push_back(value);
    }

    const std::vector<std::string>& GetData() const {
        return data_;
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

    uint64_t WriteToFile([[maybe_unused]] std::ofstream& file) override {
        // TODO
        return 0;
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    void ReadFromRawData([[maybe_unused]] const std::vector<char>& buffer) override {
        // TODO
    }

    void Clear() override {
        data_.clear();
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

    void Add([[maybe_unused]] const std::string& value) override {
        // TODO
    }

    uint64_t WriteToFile([[maybe_unused]] std::ofstream& file) override {
        // TODO
        return 0;
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    void ReadFromRawData([[maybe_unused]] const std::vector<char>& buffer) override {
        // TODO
    }

    void Clear() override {
        data_.clear();
    }

private:
    std::vector<std::string> data_;
    // TODO: Implement timestamp-specific methods and storage
};

#endif  // COLUMNAR_ENGINE_OBJECT_H
