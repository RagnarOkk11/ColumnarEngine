//
// Created by ragnarokk on 01.04.2026.
//

#include "VectorOfStrings.h"

#include "macro.h"

#include <cstdint>
#include <stdexcept>
#include <string>

VectorOfStrings2D::VectorOfStrings2D() {
    offsets_.push_back(0);
    lines_ = 1;
    columns_ = 0;
    columns_in_line_ = 0;
    adding_string_ = false;
}

void VectorOfStrings2D::Clear() {
    data_.clear();
    offsets_.clear();
    offsets_.push_back(0);
    column_sizes_.clear();
    columns_ = 0;
    columns_in_line_ = 0;
    lines_ = 1;
    adding_string_ = false;
}


size_t VectorOfStrings2D::Size() const {
    if (lines_ != 1) [[unlikely]] {
        THROW_RUNTIME_ERROR("It is 2D vector of strings");
    }
    return offsets_.size() - 1;
}

size_t VectorOfStrings2D::Width() const {
    if (lines_ == 1) {
        return columns_in_line_;
    }
    return columns_;
}

size_t VectorOfStrings2D::Height() const {
    if (columns_in_line_ == 0) {
        return lines_ - 1;
    }
    return lines_;
}

size_t VectorOfStrings2D::ColumnsInCurrentLine() const {
    return columns_in_line_;
}

uint64_t VectorOfStrings2D::ApproxByteSize() const {
    return data_.size() + (offsets_.size() * sizeof(size_t));
}

uint64_t VectorOfStrings2D::ColumnSize(size_t j) const {
    return column_sizes_[j];
}
