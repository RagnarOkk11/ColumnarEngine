#pragma once

#include "types/ColumnType.h"

#include <memory>

class Column {
public:
    virtual ~Column() = default;
    virtual ColumnType GetType() const = 0;
    virtual size_t Size() const = 0;
    virtual const void* GetRawData() const = 0;
    virtual void Clear() = 0;
};
