//
// Created by ragnarokk on 03.01.2026.
//

// OPTIMIZE: function AddBatch may be done with better logic of reserve (as in StringColumn)

#ifndef COLUMNAR_ENGINE_OBJECT_H
#define COLUMNAR_ENGINE_OBJECT_H

#include "macro.h"
#include "VectorOfStrings.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <fstream>
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
    LONGDOUBLE,
    CHAR,
    STRING,
    DATE,
    TIMESTAMP,
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
    virtual void Reserve([[maybe_unused]] size_t rows) {
    }
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

template <>
struct ColumnTypeTraits<long double> {
    static constexpr ColumnType kType = ColumnType::LONGDOUBLE;
    static long double FromString(const std::string_view& value) {
        return std::stold(std::string(value));
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
    using ValueType = T;

    ColumnType GetType() const override {
        return ColumnTypeTraits<T>::kType;
    }

    size_t Size() const override {
        return data_.size();
    }

    void Add(T value) {
        data_.push_back(value);
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
using LongDoubleColumn = NumericColumn<long double>;

class CharColumn : public Column {
public:
    using ValueType = char;

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

    const std::vector<char>& GetData() const {
        return data_;
    }

private:
    std::vector<char> data_;
};

// OPTIMIZE: perf decreased because i made VectorOfStrings logic and moved this logic into a
// particular class
class StringColumn : public Column {
public:
    using ValueType = std::string;

    ColumnType GetType() const override {
        return ColumnType::STRING;
    }

    size_t Size() const override {
        return data_.Size();
    }

    void Add(const std::string& value) override {
        data_.PushBack(value);
    }

    void AddView(std::string_view value) override {
        data_.PushBack(value);
    }

    void AddBatch(const VectorOfStrings2D& batch, size_t j) override {
        size_t h = batch.Height();
        EnsureCapacity(batch.ColumnSize(j), h);
        // data_.ReserveOffsets(data_.Size() + h + 1);
        // data_.ReserveData(data_.DataSize() + batch.ColumnSize(j));
        for (size_t i = 0; i < h; ++i) {
            data_.PushBack(batch.GetString2D(i, j));
        }
    }

    uint64_t WriteToFile(std::ofstream& file) override {
        const std::vector<size_t>& offsets = data_.GetOffsets();
        const std::vector<char>& data = data_.GetData();

        uint64_t total_size = 0;
        std::vector<char> buffer;
        size_t h = Size();
        size_t total_len = 0;
        for (size_t i = 0; i < h; ++i) {
            total_len += sizeof(uint32_t) + (offsets[i + 1] - offsets[i]);
        }
        buffer.reserve(total_len);
        for (size_t i = 0; i < h; ++i) {
            size_t start = offsets[i];
            size_t end = offsets[i + 1];
            uint32_t length = end - start;
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&length),
                          reinterpret_cast<const char*>(&length) + sizeof(length));
            if (length > 0) {
                buffer.insert(buffer.end(), data.begin() + start, data.begin() + end);
            }
            total_size += sizeof(length) + length;
        }
        file.write(buffer.data(), buffer.size());
        return total_size;
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= Size()) [[unlikely]] {
            THROW_RUNTIME_ERROR("Index out of range");
        }
        return std::string{data_[index]};
    }

    void ReadFromRawData(const std::vector<char>& buffer) override {
        Clear();
        size_t offset = 0;
        while (offset < buffer.size()) {
            uint32_t length;
            std::memcpy(&length, buffer.data() + offset, sizeof(length));
            offset += sizeof(length);
            std::string_view str(buffer.data() + offset, length);
            data_.PushBack(str);
            offset += length;
        }
    }

    void Clear() override {
        data_.Clear();
    }

    const VectorOfStrings& GetData() {
        return data_;
    }

private:
    VectorOfStrings data_;

    void EnsureCapacity(size_t data_size, size_t elements) {
        size_t required_sz = data_.DataSize() + data_size;
        size_t data_cap = data_.DataCapacity();
        if (required_sz > data_cap) {
            size_t cap = data_cap == 0 ? 1024 : data_cap * 2;
            data_.ReserveData(std::max(required_sz, cap));
        }

        size_t required_offset_size = data_.Size() + elements;
        size_t off_cap = data_.OffsetsCapacity();
        if (required_offset_size > off_cap) {
            size_t cap = off_cap == 0 ? 1024 : off_cap * 2;
            data_.ReserveOffsets(std::max(required_offset_size, cap));
        }
    }
};

template <typename Derived, typename T, ColumnType CType>
class TemporalColumn : public Column {
public:
    using ValueType = T;

    ColumnType GetType() const override {
        return CType;
    }

    size_t Size() const override {
        return data_.size();
    }

    void Add(T value) {
        data_.push_back(value);
    }

    void Add(const std::string& value) override {
        data_.push_back(Derived::Parse(value));
    }

    void AddView(std::string_view value) override {
        data_.push_back(Derived::Parse(value));
    }

    void AddBatch(const VectorOfStrings2D& batch, size_t j) override {
        size_t h = batch.Height();
        data_.reserve(data_.size() + h);
        for (size_t i = 0; i < h; ++i) {
            std::string_view cur = batch.GetString2D(i, j);
            data_.push_back(Derived::Parse(cur));
        }
    }

    uint64_t WriteToFile(std::ofstream& file) override {
        if (!data_.empty()) {
            file.write(reinterpret_cast<const char*>(data_.data()), data_.size() * sizeof(T));
        }
        return data_.size() * sizeof(T);
    }

    std::string GetDataAsString(size_t index) const override {
        if (index >= data_.size()) [[unlikely]] {
            THROW_RUNTIME_ERROR("Index out of range");
        }
        return Derived::Format(data_[index]);
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

    const std::vector<T>& GetData() const {
        return data_;
    }

private:
    std::vector<T> data_;
};

class DateColumn : public TemporalColumn<DateColumn, int32_t, ColumnType::DATE> {
public:
    static int32_t Parse(std::string_view unit) {
        if (unit.size() != 10 || unit[4] != '-' || unit[7] != '-') [[unlikely]] {
            THROW_RUNTIME_ERROR("Invalid date format: " + std::string(unit));
        }

        int32_t year =
            (unit[0] - '0') * 1000 + (unit[1] - '0') * 100 + (unit[2] - '0') * 10 + (unit[3] - '0');
        int32_t month = (unit[5] - '0') * 10 + (unit[6] - '0');
        int32_t day = (unit[8] - '0') * 10 + (unit[9] - '0');

        year -= (month <= 2);
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(year - era * 400);
        const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

        return era * 146097 + static_cast<int32_t>(doe) - 719468;
    }

    static std::string Format(int32_t days) {
        days += 719468;
        const int era = (days >= 0 ? days : days - 146096) / 146097;
        const unsigned doe = static_cast<unsigned>(days - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int y = static_cast<int>(yoe) + era * 400;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        const unsigned d = doy - (153 * mp + 2) / 5 + 1;
        const unsigned m = mp + (mp < 10 ? 3 : -9);
        const int year = y + (m <= 2);

        std::string res = "0000-00-00";
        auto write_digit = [&](int val, int pos, int len) {
            for (int i = 0; i < len; ++i) {
                res[pos + len - 1 - i] = (val % 10) + '0';
                val /= 10;
            }
        };
        write_digit(year, 0, 4);
        write_digit(m, 5, 2);
        write_digit(d, 8, 2);
        return res;
    }
};

class TimestampColumn : public TemporalColumn<TimestampColumn, int64_t, ColumnType::TIMESTAMP> {
public:
    static int64_t Parse(std::string_view unit) {
        if (unit.size() != 19 || unit[4] != '-' || unit[7] != '-' || unit[10] != ' ' ||
            unit[13] != ':' || unit[16] != ':') [[unlikely]] {
            THROW_RUNTIME_ERROR("Invalid date format: " + std::string(unit));
        }

        auto to_int2 = [](char a, char b) { return (a - '0') * 10 + (b - '0'); };

        int year =
            (unit[0] - '0') * 1000 + (unit[1] - '0') * 100 + (unit[2] - '0') * 10 + (unit[3] - '0');
        int month = to_int2(unit[5], unit[6]);
        int day = to_int2(unit[8], unit[9]);
        int hour = to_int2(unit[11], unit[12]);
        int minute = to_int2(unit[14], unit[15]);
        int second = to_int2(unit[17], unit[18]);

        year -= (month <= 2);
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(year - era * 400);
        const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

        int32_t days = era * 146097 + static_cast<int>(doe) - 719468;
        int64_t seconds = hour * 3600 + minute * 60 + second;
        return static_cast<int64_t>(days) * 86400 + seconds;
    }

    static std::string Format(int64_t total_seconds) {
        int64_t days = total_seconds / 86400;
        int64_t seconds_in_day = total_seconds % 86400;
        if (seconds_in_day < 0) {
            seconds_in_day += 86400;
            days -= 1;
        }

        std::string res = DateColumn::Format(static_cast<int32_t>(days));
        res += " 00:00:00";
        int hour = seconds_in_day / 3600;
        int minute = (seconds_in_day % 3600) / 60;
        int second = seconds_in_day % 60;
        auto write_digit = [&](int val, int pos) {
            res[pos] = (val / 10) + '0';
            res[pos + 1] = (val % 10) + '0';
        };

        write_digit(hour, 11);
        write_digit(minute, 14);
        write_digit(second, 17);

        return res;
    }
};

#endif  // COLUMNAR_ENGINE_OBJECT_H
