#pragma once

#include "Date.h"
#include "TimeUnit.h"

#include <string>
#include <string_view>

struct Timestamp {
    static int64_t Parse(std::string_view unit) {
        if (unit.size() != 19 || unit[4] != '-' || unit[7] != '-' || unit[10] != ' ' ||
            unit[13] != ':' || unit[16] != ':') [[unlikely]] {
            THROW_RUNTIME_ERROR("Invalid date format: " + std::string(unit));
        }

        auto to_int2 = [](char a, char b) { return (a - '0') * 10 + (b - '0'); };

        int year =
            (unit[0] - '0') * 1000 + (unit[1] - '0') * 100 + (unit[2] - '0') * 10 + (unit[3] - '0');
        int month = to_int2(unit[5], unit[6]);
        int day = to_int2(unit[8], unit[9]);
        int hour = to_int2(unit[11], unit[12]);
        int minute = to_int2(unit[14], unit[15]);
        int second = to_int2(unit[17], unit[18]);

        year -= (month <= 2);
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(year - era * 400);
        const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

        int32_t days = era * 146097 + static_cast<int>(doe) - 719468;
        int64_t seconds = hour * 3600 + minute * 60 + second;
        return static_cast<int64_t>(days) * 86400 + seconds;
    }

    static std::string Format(int64_t total_seconds) {
        int64_t days = total_seconds / 86400;
        int64_t seconds_in_day = total_seconds % 86400;
        if (seconds_in_day < 0) {
            seconds_in_day += 86400;
            days -= 1;
        }

        std::string res = Date::Format(static_cast<int32_t>(days));
        res += " 00:00:00";
        int hour = seconds_in_day / 3600;
        int minute = (seconds_in_day % 3600) / 60;
        int second = seconds_in_day % 60;
        auto write_digit = [&](int val, int pos) {
            res[pos] = (val / 10) + '0';
            res[pos + 1] = (val % 10) + '0';
        };

        write_digit(hour, 11);
        write_digit(minute, 14);
        write_digit(second, 17);

        return res;
    }

    // TODO: make uniform interface called Extract
    static int32_t ExtractMinute(int64_t total_seconds) {
        int64_t seconds_in_day = total_seconds % 86400;
        if (seconds_in_day < 0) {
            seconds_in_day += 86400;
        }
        return (seconds_in_day % 3600) / 60;
    }

    static int64_t Truncate(int64_t total_seconds, TimeUnitType part) {
        int64_t seconds_in_day = total_seconds % 86400;
        if (seconds_in_day < 0) {
            seconds_in_day += 86400;
        }

        switch (part) {
            case TimeUnitType::MINUTE: {
                int64_t extra_seconds = seconds_in_day % 60;
                return total_seconds - extra_seconds;
            }
            case TimeUnitType::HOUR: {
                int64_t extra_seconds = seconds_in_day % 3600;
                return total_seconds - extra_seconds;
            }
            case TimeUnitType::DAY: {
                return total_seconds - seconds_in_day;
            }
            case TimeUnitType::MONTH: {
                int32_t days = total_seconds / 86400;
                int32_t trunc = Date::Truncate(days, TimeUnitType::MONTH);
                return static_cast<int64_t>(trunc) * 86400;
            }
            case TimeUnitType::YEAR: {
                int32_t days = total_seconds / 86400;
                int32_t trunc = Date::Truncate(days, TimeUnitType::YEAR);
                return static_cast<int64_t>(trunc) * 86400;
            }
            default: break;
        }
        return total_seconds;
    }
};
