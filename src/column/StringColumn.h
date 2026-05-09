#pragma once

#include "column/Column.h"
#include "column/ColumnBuilder.h"
#include "utils/VectorOfStrings.h"
#include "utils/Macro.h"

#include <cstring>
#include <memory>

class StringColumn : public Column {
public:
    using ValueType = std::string;
    using ContainerType = VectorOfStrings;

    StringColumn() = default;
    StringColumn(ContainerType&& data) noexcept : data_(std::move(data)) {}

    ColumnType GetType() const override {
        return ColumnType::STRING;
    }

    size_t Size() const override {
        return data_.Size();
    }

    const void* GetRawData() const override {
        return &data_;
    }

    void Clear() override {
        data_.Clear();
    }

    const ContainerType& GetData() const {
        return data_;
    }

    void ReadFromBuffer(const std::vector<char>& buffer) {
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

private:
    ContainerType data_;
};

class StringColumnBuilder : public ColumnBuilder {
public:
    StringColumnBuilder() = default;

    void AddValue(std::string_view value) {
        data_.PushBack(value);
    }

    void AddBatch(const VectorOfStrings2D& batch, size_t j) override {
        size_t h = batch.Height();
        for (size_t i = 0; i < h; ++i) {
            data_.PushBack(batch.GetString2D(i, j));
        }
    }

    std::shared_ptr<Column> Finish() override {
        auto result = std::make_shared<StringColumn>(std::move(data_));
        data_.Clear();
        return result;
    }

private:
    VectorOfStrings data_;
};

template <>
struct BuilderTypeTrait<StringColumn> {
    using Type = StringColumnBuilder;
};
