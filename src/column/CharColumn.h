#pragma once

#include "column/Column.h"
#include "column/ColumnBuilder.h"

#include <vector>
#include <memory>

class CharColumn : public Column {
public:
    using ValueType = char;
    using ContainerType = std::vector<ValueType>;

    ColumnType GetType() const override {
        return ColumnType::CHAR;
    }
    
    size_t Size() const override {
        return data_.size();
    }

    const void* GetRawData() const override {
        return data_.data();
    }

    void Clear() override {
        data_.clear();
    }

    const ContainerType& GetData() const {
        return data_;
    }

    void AddValue(char value) {
        data_.push_back(value);
    }
    
    void ReadFromBuffer(const std::vector<char>& buffer) {
        data_.clear();
        data_.insert(data_.end(), buffer.begin(), buffer.end());
    }

private:
    ContainerType data_;
};

class CharColumnBuilder : public ColumnBuilder {
public:
    CharColumnBuilder() : column_(std::make_shared<CharColumn>()) {}

    void AddBatch(const VectorOfStrings2D& batch, size_t j) override {
        size_t h = batch.Height();
        for (size_t i = 0; i < h; ++i) {
            std::string_view val = batch.GetString2D(i, j);
            ASSERT(val.size() == 1);
            column_->AddValue(val[0]);
        }
    }

    std::shared_ptr<Column> Finish() override {
        auto result = column_;
        column_ = std::make_shared<CharColumn>();
        return result;
    }

private:
    std::shared_ptr<CharColumn> column_;
};
