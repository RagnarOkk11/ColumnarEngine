#pragma once

#include "TimeUnit.h"
#include "utils/Assert.h"

#include <cstdint>
#include <string>
#include <string_view>

struct Date {
    static int32_t Parse(std::string_view unit) {
        if (unit.size() != 10 || unit[4] != '-' || unit[7] != '-') [[unlikely]] {
            THROW_RUNTIME_ERROR("Invalid date format: " + std::string(unit));
        }

        int32_t year =
            (unit[0] - '0') * 1000 + (unit[1] - '0') * 100 + (unit[2] - '0') * 10 + (unit[3] - '0');
        int32_t month = (unit[5] - '0') * 10 + (unit[6] - '0');
        int32_t day = (unit[8] - '0') * 10 + (unit[9] - '0');

        year -= (month <= 2);
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(year - era * 400);
        const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

        return era * 146097 + static_cast<int32_t>(doe) - 719468;
    }

    static std::string Format(int32_t days) {
        days += 719468;
        const int era = (days >= 0 ? days : days - 146096) / 146097;
        const unsigned doe = static_cast<unsigned>(days - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int y = static_cast<int>(yoe) + era * 400;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        const unsigned d = doy - (153 * mp + 2) / 5 + 1;
        const unsigned m = mp + (mp < 10 ? 3 : -9);
        const int year = y + (m <= 2);

        std::string res = "0000-00-00";
        auto write_digit = [&](int val, int pos, int len) {
            for (int i = 0; i < len; ++i) {
                res[pos + len - 1 - i] = (val % 10) + '0';
                val /= 10;
            }
        };
        write_digit(year, 0, 4);
        write_digit(m, 5, 2);
        write_digit(d, 8, 2);
        return res;
    }

    static int32_t Truncate(int32_t total_days, TimeUnitType unit) {
        if (unit == TimeUnitType::DAY || unit == TimeUnitType::HOUR || unit == TimeUnitType::MINUTE ||
            unit == TimeUnitType::SECOND) {
            return total_days;
        }

        int32_t days = total_days + 719468;
        const int era = (days >= 0 ? days : days - 146096) / 146097;
        const unsigned doe = static_cast<unsigned>(days - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int y = static_cast<int>(yoe) + era * 400;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        const unsigned d = doy - (153 * mp + 2) / 5 + 1;
        const unsigned m = mp + (mp < 10 ? 3 : -9);
        int year = y + (m <= 2);

        switch (unit) {
            case TimeUnitType::MONTH: {
                return total_days - static_cast<int32_t>(d) + 1;
            }
            case TimeUnitType::YEAR: {
                const int e = (year >= 0 ? year : year - 399) / 400;
                const unsigned ye = static_cast<unsigned>(year - e * 400);
                const unsigned de = ye * 365 + ye / 4 - ye / 100 + 306;
                return e * 146097 + static_cast<int32_t>(de) - 719468;
            }
            default:
                return total_days;
        }
    }
};
