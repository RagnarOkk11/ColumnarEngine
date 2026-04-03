//
// Created by ragnarokk on 01.04.2026.
//

#ifndef COLUMNAR_ENGINE_VECTOROFSTRINGS_H
#define COLUMNAR_ENGINE_VECTOROFSTRINGS_H

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>

class VectorOfStrings2D {
public:
    VectorOfStrings2D();
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
    void Clear();

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
    size_t Size() const;
    size_t Width() const;
    size_t Height() const;
    size_t ColumnsInCurrentLine() const;

    void ReserveData(size_t size) {
        data_.reserve(size);
    }
    void ReserveOffsets(size_t size) {
        offsets_.reserve(size);
    }
    size_t DataSize() const {
        return data_.size();
    }
    uint64_t ApproxByteSize() const;
    uint64_t ColumnSize(size_t j) const;

private:
    std::vector<char> data_;
    std::vector<size_t> offsets_;
    std::vector<size_t> column_sizes_;
    size_t lines_;
    size_t columns_;
    size_t columns_in_line_;
    bool adding_string_;
};

#endif  // COLUMNAR_ENGINE_VECTOROFSTRINGS_H
