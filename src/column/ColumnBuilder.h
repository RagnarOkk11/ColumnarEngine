#pragma once

#include "column/Column.h"
#include "utils/VectorOfStrings.h"

#include <memory>

class ColumnBuilder {
public:
    virtual ~ColumnBuilder() = default;
    virtual void AddBatch(const VectorOfStrings2D& batch, size_t column_index) = 0;
    virtual std::shared_ptr<Column> Finish() = 0;
};
