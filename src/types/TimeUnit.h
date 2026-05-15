#pragma once

#include "utils/Assert.h"

#include <string_view>
#include <string>

enum class TimeUnitType {
    YEAR,
    MONTH,
    DAY,
    HOUR,
    MINUTE,
    SECOND
};


class TimeUnit {
public:
    static TimeUnitType ParseTimeUnit(std::string_view unit_str) {
        if (unit_str == "year") {
            return TimeUnitType::YEAR;
        }
        if (unit_str == "month") {
            return TimeUnitType::MONTH;
        }
        if (unit_str == "day") {
            return TimeUnitType::DAY;
        }
        if (unit_str == "hour") {
            return TimeUnitType::HOUR;
        }
        if (unit_str == "minute") {
            return TimeUnitType::MINUTE;
        }
        if (unit_str == "second") {
            return TimeUnitType::SECOND;
        }

        THROW_RUNTIME_ERROR("Unknown time unit: " + std::string(unit_str));
    }
};