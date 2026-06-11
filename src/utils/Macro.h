#pragma once

#include "column/Column.h"
#include "column/CharColumn.h"
#include "column/NumericColumn.h"
#include "column/StringColumn.h"
#include "column/TemporalColumn.h"

// USE THIS EVERYWHERE FOR SWITCHING
#define FOR_NUMERIC_COLUMN_TYPE(M)                \
    M(INT16, "INT16", Int16Column)                \
    M(INT32, "INT32", Int32Column)                \
    M(INT64, "INT64", Int64Column)                \
    M(INT128, "INT128", Int128Column)             \
    M(FLOAT, "FLOAT", FloatColumn)                \
    M(DOUBLE, "DOUBLE", DoubleColumn)             \
    M(LONGDOUBLE, "LONGDOUBLE", LongDoubleColumn) \
    M(CHAR, "CHAR", CharColumn)                   \
    M(DATE, "DATE", DateColumn)                   \
    M(TIMESTAMP, "TIMESTAMP", TimestampColumn)

// USE THIS EVERYWHERE FOR SWITCHING
#define FOR_EACH_COLUMN_TYPE(M) \
    FOR_NUMERIC_COLUMN_TYPE(M)  \
    M(STRING, "STRING", StringColumn)
