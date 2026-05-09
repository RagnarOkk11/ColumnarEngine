#pragma once

#include "column/Column.h"
#include "column/ColumnBuilder.h"
#include "utils/Assert.h"

#include <vector>
#include <string>
#include <string_view>
#include <charconv>
#include <cstring>

using Int128 = __int128_t;

template <typename T>
struct ColumnTypeTraits;

template <>
struct ColumnTypeTraits<int16_t> {
    static constexpr ColumnType kType = ColumnType::INT16;
    static int16_t FromString(std::string_view value) {
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
    static int32_t FromString(std::string_view value) {
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
    static int64_t FromString(std::string_view value) {
        int64_t parsed = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec != std::errc() || ptr != value.data() + value.size()) {
            THROW_RUNTIME_ERROR("Invalid INT64 value: " + std::string(value));
        }
        return parsed;
    }
};

template <>
struct ColumnTypeTraits<Int128> {
    static constexpr ColumnType kType = ColumnType::INT128;
    static Int128 FromString(std::string_view value) {
        Int128 result = 0;
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
    static float FromString(std::string_view value) {
        return std::stof(std::string(value));
    }
};

template <>
struct ColumnTypeTraits<double> {
    static constexpr ColumnType kType = ColumnType::DOUBLE;
    static double FromString(std::string_view value) {
        return std::stod(std::string(value));
    }
};

template <>
struct ColumnTypeTraits<long double> {
    static constexpr ColumnType kType = ColumnType::LONGDOUBLE;
    static long double FromString(std::string_view value) {
        return std::stold(std::string(value));
    }
};

template <typename T>
class NumericColumn : public Column {
public:
    using ValueType = T;
    using ContainerType = std::vector<ValueType>;

    NumericColumn() = default;
    NumericColumn(ContainerType&& data) noexcept : data_(std::move(data)) {
    }

    ~NumericColumn() override = default;

    ColumnType GetType() const override {
        return ColumnTypeTraits<T>::kType;
    }

    size_t Size() const override {
        return data_.size();
    }

    const void* GetRawData() const override {
        return &data_;
    }

    void Clear() override {
        data_.clear();
    }

    const ContainerType& GetData() const {
        return data_;
    }

    void ReadFromBuffer(const std::vector<char>& buffer) {
        if (buffer.size() % sizeof(T) != 0) {
            THROW_RUNTIME_ERROR("Raw buffer size is not aligned with type size");
        }
        size_t n = buffer.size() / sizeof(T);
        data_.resize(n);
        std::memcpy(data_.data(), buffer.data(), buffer.size());
    }

private:
    ContainerType data_;
};

template <typename T>
class NumericColumnBuilder : public ColumnBuilder {
public:
    NumericColumnBuilder() = default;

    void AddValue(T value) {
        data_.push_back(value);
    }

    void AddBatch(const VectorOfStrings2D& batch, size_t j) override {
        size_t h = batch.Height();
        data_.reserve(data_.size() + h);
        for (size_t i = 0; i < h; ++i) {
            data_.push_back(ColumnTypeTraits<T>::FromString(batch.GetString2D(i, j)));
        }
    }

    std::shared_ptr<Column> Finish() override {
        auto result = std::make_shared<NumericColumn<T>>(std::move(data_));
        data_.clear();
        return result;
    }

private:
    std::vector<T> data_;
};

using Int16Column = NumericColumn<int16_t>;
using Int32Column = NumericColumn<int32_t>;
using Int64Column = NumericColumn<int64_t>;
using Int128Column = NumericColumn<Int128>;
using FloatColumn = NumericColumn<float>;
using DoubleColumn = NumericColumn<double>;
using LongDoubleColumn = NumericColumn<long double>;

using Int16ColumnBuilder = NumericColumnBuilder<int16_t>;
using Int32ColumnBuilder = NumericColumnBuilder<int32_t>;
using Int64ColumnBuilder = NumericColumnBuilder<int64_t>;
using Int128ColumnBuilder = NumericColumnBuilder<Int128>;
using FloatColumnBuilder = NumericColumnBuilder<float>;
using DoubleColumnBuilder = NumericColumnBuilder<double>;
using LongDoubleColumnBuilder = NumericColumnBuilder<long double>;

template <typename T>
struct BuilderTypeTrait<NumericColumn<T>> {
    using Type = NumericColumnBuilder<T>;
};
