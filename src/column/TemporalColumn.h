#pragma once

#include "column/Column.h"
#include "column/ColumnBuilder.h"
#include "types/Date.h"
#include "types/Timestamp.h"

#include <cstring>
#include <memory>
#include <vector>

template <typename Derived, typename T, ColumnType CType>
class TemporalColumn : public Column {
public:
    using ValueType = T;
    using ContainerType = std::vector<ValueType>;

    TemporalColumn() = default;
    TemporalColumn(ContainerType&& data) noexcept : data_(std::move(data)) {}

    ColumnType GetType() const override {
        return CType;
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

class DateColumn : public TemporalColumn<DateColumn, int32_t, ColumnType::DATE> {
public:
    using TemporalColumn::TemporalColumn;
};

class TimestampColumn : public TemporalColumn<TimestampColumn, int64_t, ColumnType::TIMESTAMP> {
public:
    using TemporalColumn::TemporalColumn;
};

template <typename ColumnTypeT, typename ParseStruct>
class TemporalColumnBuilder : public ColumnBuilder {
    using ValueType = typename ColumnTypeT::ValueType;

public:
    TemporalColumnBuilder() = default;

    void AddValue(ValueType value) {
        data_.push_back(value);
    }

    void AddBatch(const VectorOfStrings2D& batch, size_t j) override {
        size_t h = batch.Height();
        data_.reserve(data_.size() + h);
        for (size_t i = 0; i < h; ++i) {
            std::string_view cur = batch.GetString2D(i, j);
            data_.push_back(ParseStruct::Parse(cur));
        }
    }

    std::shared_ptr<Column> Finish() override {
        auto result = std::make_shared<ColumnTypeT>(std::move(data_));
        data_.clear();
        return result;
    }

private:
    std::vector<ValueType> data_;
};

using DateColumnBuilder = TemporalColumnBuilder<DateColumn, Date>;
using TimestampColumnBuilder = TemporalColumnBuilder<TimestampColumn, Timestamp>;

template <>
struct BuilderTypeTrait<DateColumn> {
    using Type = DateColumnBuilder;
};

template <>
struct BuilderTypeTrait<TimestampColumn> {
    using Type = TimestampColumnBuilder;
};
