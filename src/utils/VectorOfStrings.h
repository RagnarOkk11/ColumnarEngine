#pragma once

#include "utils/Assert.h"

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <string>
#include <vector>

class VectorOfStrings {
public:
    VectorOfStrings() {
        offsets_.push_back(0);
    }

    void PushBack(std::string_view str) {
        data_.insert(data_.end(), str.begin(), str.end());
        offsets_.push_back(data_.size());
    }

    std::string_view GetString(size_t index) const {
        size_t start = offsets_[index];
        size_t end = offsets_[index + 1];
        return std::string_view(data_.data() + start, end - start);
    }
    std::string_view operator[](size_t index) const {
        return GetString(index);
    }
    bool EmptyLastString() const {
        return offsets_.back() == data_.size();
    }
    void ReserveData(size_t size) {
        data_.reserve(size);
    }
    void ReserveOffsets(size_t size) {
        offsets_.reserve(size + 1);
    }
    size_t Size() const {
        return offsets_.size() - 1;
    }
    size_t DataSize() const {
        return data_.size();
    }
    size_t DataCapacity() const {
        return data_.capacity();
    }
    size_t OffsetsCapacity() const {
        return offsets_.capacity();
    }
    void Clear() {
        data_.clear();
        offsets_.clear();
        offsets_.push_back(0);
    }
    const std::vector<char>& GetData() const {
        return data_;
    }
    const std::vector<size_t>& GetOffsets() const {
        return offsets_;
    }

    class Iterator {
    public:
        Iterator(const VectorOfStrings& vec, size_t index) : vec_(vec), index_(index) {
        }

        std::string_view operator*() const {
            return vec_.GetString(index_);
        }
        Iterator& operator++() {
            ++index_;
            return *this;
        }
        bool operator!=(const Iterator& other) const {
            return index_ != other.index_;
        }

    private:
        const VectorOfStrings& vec_;
        size_t index_;
    };
    Iterator begin() const {  // NOLINT
        return Iterator(*this, 0);
    }
    Iterator end() const {  // NOLINT
        return Iterator(*this, Size());
    }

private:
    std::vector<char> data_;
    std::vector<size_t> offsets_;
};

class VectorOfStrings2D {
public:
    VectorOfStrings2D() {
        offsets_.push_back(0);
        lines_ = 1;
        columns_ = 0;
        columns_in_line_ = 0;
        adding_string_ = false;
    }

    void AddString(std::string_view str) {
        const size_t next_columns_in_line = columns_in_line_ + 1;
        data_.insert(data_.end(), str.begin(), str.end());
        offsets_.push_back(data_.size());
        columns_in_line_ = next_columns_in_line;
        if (lines_ == 1) {
            column_sizes_.push_back(str.size());
        } else {
            column_sizes_[columns_in_line_ - 1] += str.size();
        }
    }

    void StartAddString() {
        adding_string_ = true;
    }

    void ContinueAddString(char c) {
        data_.push_back(c);
    }

    void ContinueAddString(std::string_view sv) {
        data_.insert(data_.end(), sv.begin(), sv.end());
    }

    void PopLastChar() {
        data_.pop_back();
    }

    void EndAddString() {
        offsets_.push_back(data_.size());
        size_t len = offsets_.back() - offsets_[offsets_.size() - 2];
        if (lines_ == 1) {
            column_sizes_.push_back(len);
        } else {
            column_sizes_[columns_in_line_] += len;
        }
        ++columns_in_line_;
    }

    void StartNewLine() {
        if (lines_ == 1) {
            columns_ = columns_in_line_;
        }
        ++lines_;
        columns_in_line_ = 0;
    }

    void Clear() {
        data_.clear();
        offsets_.clear();
        offsets_.push_back(0);
        column_sizes_.clear();
        columns_ = 0;
        columns_in_line_ = 0;
        lines_ = 1;
        adding_string_ = false;
    }

    std::string_view GetString(size_t index) const {
        size_t start = offsets_[index];
        size_t end = offsets_[index + 1];
        return std::string_view(data_.data() + start, end - start);
    }
    std::string_view GetString2D(size_t row_index, size_t column_index) const {
        const size_t row_width = (lines_ == 1 ? columns_in_line_ : columns_);
        size_t line_start = (row_index * row_width) + column_index;
        size_t start = offsets_[line_start];
        size_t end = offsets_[line_start + 1];
        return std::string_view(data_.data() + start, end - start);
    }
    bool EmptyLastString() const {
        return offsets_.back() == data_.size();
    }
    bool EmptyLastRow() const {
        return columns_in_line_ == 0;
    }
    char BackLastString() const {
        return data_.back();
    }
    size_t Size() const {
        if (lines_ != 1) [[unlikely]] {
            THROW_RUNTIME_ERROR("It is 2D vector of strings");
        }
        return offsets_.size() - 1;
    }
    size_t Width() const {
        if (lines_ == 1) {
            return columns_in_line_;
        }
        return columns_;
    }
    size_t Height() const {
        if (columns_in_line_ == 0) {
            return lines_ - 1;
        }
        return lines_;
    }
    size_t ColumnsInCurrentLine() const {
        return columns_in_line_;
    }

    void ReserveData(size_t size) {
        data_.reserve(size);
    }
    void ReserveOffsets(size_t size) {
        offsets_.reserve(size);
    }
    size_t DataSize() const {
        return data_.size();
    }
    uint64_t ApproxByteSize() const {
        return data_.size() + (offsets_.size() * sizeof(size_t));
    }
    uint64_t ColumnSize(size_t j) const {
        return column_sizes_[j];
    }

private:
    std::vector<char> data_;
    std::vector<size_t> offsets_;
    std::vector<size_t> column_sizes_;
    size_t lines_;
    size_t columns_;
    size_t columns_in_line_;
    bool adding_string_;
};
