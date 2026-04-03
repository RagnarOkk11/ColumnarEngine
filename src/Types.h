//
// Created by ragnarokk on 03.01.2026.
//

#ifndef COLUMNAR_ENGINE_OBJECT_H
#define COLUMNAR_ENGINE_OBJECT_H

#include "macro.h"
#include "VectorOfStrings.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

enum class ColumnType : uint8_t {
    INT16,
    INT32,
    INT64,
    INT128,
    FLOAT,
    DOUBLE,
    CHAR,
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
    virtual void Reserve([[maybe_unused]] size_t rows) {}
    virtual void Add(const std::string& value) = 0;
    virtual void AddView(std::string_view value) = 0;
    virtual void AddBatch(const VectorOfStrings2D& batch, size_t j) = 0;
    virtual uint64_t WriteToFile(std::ofstream& file) = 0;
    virtual std::string GetDataAsString(size_t index) const = 0;
    virtual void ReadFromRawData(const std::vector<char>& buffer) = 0;
    virtual void Clear() = 0;
};

template <typename T>
struct ColumnTypeTraits;

template <>
struct ColumnTypeTraits<int16_t> {
    static constexpr ColumnType kType = ColumnType::INT16;
    static int16_t FromString(const std::string_view& value) {
        int16_t parsed = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec != std::errc() || ptr != value.data() + value.size()) {
            THROW_RUNTIME_ERROR("Invalid INT16 value: " + std::string(value));
        }
        return parsed;
    }
};

template <>
struct ColumnTypeTraits<int32_t> {
    static constexpr ColumnType kType = ColumnType::INT32;
    static int32_t FromString(const std::string_view& value) {
        int32_t parsed = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec != std::errc() || ptr != value.data() + value.size()) {
            THROW_RUNTIME_ERROR("Invalid INT32 value: " + std::string(value));
        }
        return parsed;
    }
};

template <>
struct ColumnTypeTraits<int64_t> {
    static constexpr ColumnType kType = ColumnType::INT64;
    static int64_t FromString(const std::string_view& value) {
        int64_t parsed = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec != std::errc() || ptr != value.data() + value.size()) {
            THROW_RUNTIME_ERROR("Invalid INT64 value: " + std::string(value));
        }
        return parsed;
    }
};

template <>
struct ColumnTypeTraits<__int128_t> {
    static constexpr ColumnType kType = ColumnType::INT128;
    static __int128_t FromString(const std::string_view& value) {
        __int128_t result = 0;
        bool negative = false;
        std::string_view value_view = value;
        if (value[0] == '-') {
            negative = true;
            value_view = value_view.substr(1);
        }
        for (char c : value_view) {
            if (c < '0' || c > '9') [[unlikely]] {
                THROW_RUNTIME_ERROR("Invalid integer value: " + std::string(value));
            }
            result = result * 10 + (c - '0');
        }
        if (negative) {
            result = -result;
        }
        return result;
    }
};

template <>
struct ColumnTypeTraits<float> {
    static constexpr ColumnType kType = ColumnType::FLOAT;
    static float FromString(const std::string_view& value) {
        return std::stof(std::string(value));
    }
};

template <>
struct ColumnTypeTraits<double> {
    static constexpr ColumnType kType = ColumnType::DOUBLE;
    static double FromString(const std::string_view& value) {
        return std::stod(std::string(value));
    }
};

template <typename T>
    requires std::is_integral_v<T> || std::is_floating_point_v<T>
std::string ToString(T value) {
    return std::to_string(value);
}

template <>
inline std::string ToString<__int128_t>(__int128_t value) {
    if (value == 0) {
        return "0";
    }
    std::string result;
    bool negative = value < 0;
    if (negative) {
        value = -value;
    }
    while (value > 0) {
        result.push_back('0' + (value % 10));
        value /= 10;
    }
    if (negative) {
        result.push_back('-');
    }
    std::reverse(result.begin(), result.end());
    return result;
}

template <typename T>
class NumericColumn : public Column {
public:
    ColumnType GetType() const override {
        return ColumnTypeTraits<T>::kType;
    }

    size_t Size() const override {
        return data_.size();
    }

    void Add(const std::string& value) override {
        data_.push_back(ColumnTypeTraits<T>::FromString(value));
    }

    void AddView(std::string_view value) override {
        data_.push_back(ColumnTypeTraits<T>::FromString(value));
    }

    void AddBatch(const VectorOfStrings2D& batch, size_t j) override {
        data_.reserve(data_.size() + batch.Height());
        for (size_t i = 0; i < batch.Height(); ++i) {
            data_.push_back(ColumnTypeTraits<T>::FromString(batch.GetString2D(i, j)));
        }
    }

    // TODO: add decoding logic
    uint64_t WriteToFile(std::ofstream& file) override {
        if (!data_.empty()) {
            file.write(reinterpret_cast<const char*>(data_.data()), data_.size() * sizeof(T));
        }
        return data_.size() * sizeof(T);
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= data_.size()) {
            THROW_RUNTIME_ERROR("Index out of range");
        }
        return ToString(data_[index]);
    }

    void ReadFromRawData(const std::vector<char>& buffer) override {
        if (buffer.size() % sizeof(T) != 0) {
            THROW_RUNTIME_ERROR("Raw buffer size is not aligned with type size");
        }
        size_t n = buffer.size() / sizeof(T);
        data_.resize(n);
        std::memcpy(data_.data(), buffer.data(), buffer.size());
    }

    void Clear() override {
        data_.clear();
    }

    void Add(T value) {
        data_.push_back(value);
    }

    const std::vector<T>& GetData() const {
        return data_;
    }

private:
    std::vector<T> data_;
};

using Int16Column = NumericColumn<int16_t>;
using Int32Column = NumericColumn<int32_t>;
using Int64Column = NumericColumn<int64_t>;
using Int128Column = NumericColumn<__int128_t>;
using FloatColumn = NumericColumn<float>;
using DoubleColumn = NumericColumn<double>;

class CharColumn : public Column {
public:
    ColumnType GetType() const override {
        return ColumnType::CHAR;
    }
    size_t Size() const override {
        return data_.size();
    }

    void Add(const std::string& value) override {
        ASSERT(value.size() == 1);
        data_.push_back(value[0]);
    }

    void AddView(std::string_view value) override {
        ASSERT(value.size() == 1);
        data_.push_back(value[0]);
    }

    void AddBatch(const VectorOfStrings2D& batch, size_t j) override {
        data_.reserve(data_.size() + batch.Height());
        for (size_t i = 0; i < batch.Height(); ++i) {
            std::string_view val = batch.GetString2D(i, j);
            ASSERT(val.size() == 1);
            data_.push_back(val[0]);
        }
    }

    uint64_t WriteToFile(std::ofstream& file) override {
        if (!data_.empty()) {
            file.write(data_.data(), data_.size());
        }
        return data_.size();
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= data_.size()) {
            THROW_RUNTIME_ERROR("Index out of range");
        }
        return std::string(1, data_[index]);
    }

    void ReadFromRawData(const std::vector<char>& buffer) override {
        data_.clear();
        data_.insert(data_.end(), buffer.begin(), buffer.end());
    }

    void Clear() override {
        data_.clear();
    }

private:
    std::vector<char> data_;
};

class StringColumn : public Column {
public:
    StringColumn() {
        offsets_.push_back(0);
    }

    ColumnType GetType() const override {
        return ColumnType::STRING;
    }

    size_t Size() const override {
        return offsets_.size() - 1;
    }

    void Add(const std::string& value) override {
        AddString(value);
    }

    void AddView(std::string_view value) override {
        AddString(value);
    }

    void AddBatch(const VectorOfStrings2D& batch, size_t j) override {
        size_t h = batch.Height();
        offsets_.reserve(offsets_.size() + h);
        data_.reserve(data_.size() + batch.ColumnSize(j));
        for (size_t i = 0; i < h; ++i) {
            AddString(batch.GetString2D(i, j));
        }
    }

    uint64_t WriteToFile(std::ofstream& file) override {
        uint64_t total_size = 0;
        std::vector<char> buffer;
        size_t h = Size();
        size_t total_len = 0;
        for (size_t i = 0; i < h; ++i) {
            total_len += sizeof(uint32_t) + (offsets_[i + 1] - offsets_[i]);
        }
        buffer.reserve(total_len);
        for (size_t i = 0; i < h; ++i) {
            size_t start = offsets_[i];
            size_t end = offsets_[i + 1];
            uint32_t length = end - start;
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&length), reinterpret_cast<const char*>(&length) + sizeof(length));
            if (length > 0) {
                buffer.insert(buffer.end(), data_.begin() + start, data_.begin() + end);
            }
            total_size += sizeof(length) + length;
        }
        file.write(buffer.data(), buffer.size());
        return total_size;
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= Size()) {
            throw std::out_of_range("Index out of range");
        }
        size_t start = offsets_[index];
        size_t end = offsets_[index + 1];
        return std::string(data_.data() + start, end - start);
    }

    void ReadFromRawData(const std::vector<char>& buffer) override {
        Clear();
        size_t offset = 0;
        while (offset < buffer.size()) {
            uint32_t length;
            std::memcpy(&length, buffer.data() + offset, sizeof(length));
            offset += sizeof(length);
            std::string_view str(buffer.data() + offset, length);
            AddString(str);
            offset += length;
        }
    }

    void Clear() override {
        data_.clear();
        offsets_.clear();
        offsets_.push_back(0);
    }

    void AddString(std::string_view value) {
        data_.insert(data_.end(), value.begin(), value.end());
        offsets_.push_back(data_.size());
    }


private:
    std::vector<char> data_;
    std::vector<size_t> offsets_;
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

    void AddView([[maybe_unused]] std::string_view value) override {
        // TODO
    }

    void AddBatch([[maybe_unused]] const VectorOfStrings2D& batch, [[maybe_unused]] size_t j) override {
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

    void AddView([[maybe_unused]] std::string_view value) override {
        // TODO
    }

    void AddBatch([[maybe_unused]] const VectorOfStrings2D& batch, [[maybe_unused]] size_t j) override {
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
