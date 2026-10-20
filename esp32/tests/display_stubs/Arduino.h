#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <type_traits>

// 桌面显示测试所需的最小Arduino字符串接口。
class String : public std::string {
  public:
    using std::string::string;
    String() = default;

    String(const std::string &value) : std::string(value) {
    }

    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
    String(T value) : std::string(std::to_string(value)) {
    }

    String(float value, int precision) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.*f", precision, double(value));
        assign(buffer);
    }

    bool isEmpty() const {
        return empty();
    }

    String substring(unsigned start, unsigned end) const {
        return substr(start, end - start);
    }
};

uint32_t millis();

template <typename T> T min(T a, T b) {
    return a < b ? a : b;
}
