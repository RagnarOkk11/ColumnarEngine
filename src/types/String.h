#pragma once

#include <string_view>

class String {
public:
    static size_t Length(std::string_view string) {
        return string.size();
    }
};
