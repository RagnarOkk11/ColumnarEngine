#pragma once

#include <cstdint>
#include <vector>
#include <string>

enum class ColumnType : uint8_t {
    INT16,
    INT32,
    INT64,
    INT128,
    FLOAT,
    DOUBLE,
    LONGDOUBLE,
    CHAR,
    STRING,
    DATE,
    TIMESTAMP,
};

struct ColumnMetadata {
    std::string name;
    ColumnType type;
    std::vector<uint64_t> offsets;
    std::vector<uint64_t> sizes;
};

// TODO: for macro if German is gonna tell i coded that xyevo
// template <ColumnType type>
// struct ColumnTraits;
//
// template <>
// struct ColumnTraits<ColumnType::INT16> {
//     using ValueType = int16_t;
//     static constexpr std::string Name = "INT16";
// };
//
// template <>
// struct ColumnTraits<ColumnType::INT32> {
//     using ValueType = int32_t;
//     static constexpr std::string Name = "INT32";
// };
//
// template <>
// struct ColumnTraits<ColumnType::INT64> {
//     using ValueType = int64_t;
//     static constexpr std::string Name = "INT64";
// };
