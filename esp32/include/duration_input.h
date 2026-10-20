#pragma once
#include <stdint.h>

// 用整数换算十进制秒，避免浮点舍入；最多三位小数以保留已有毫秒设置。
inline bool parseDurationSeconds(const char *text, uint32_t &milliseconds) {
    if (!text || !*text) {
        return false;
    }
    uint32_t seconds = 0, fraction = 0, digits = 0;
    bool decimal = false, wholeDigit = false;
    for (const char *p = text; *p; ++p) {
        if (*p == '.' && !decimal && wholeDigit) {
            decimal = true;
        } else if (*p >= '0' && *p <= '9') {
            if (decimal) {
                if (++digits > 3) {
                    return false;
                }
                fraction = fraction * 10 + (*p - '0');
            } else {
                seconds = seconds * 10 + (*p - '0');
                wholeDigit = true;
                if (seconds > 30) {
                    return false;
                }
            }
        } else {
            return false;
        }
    }
    if (decimal && digits == 0) {
        return false;
    }
    while (digits++ < 3) {
        fraction *= 10;
    }
    const uint32_t value = seconds * 1000 + fraction;
    if (value < 100 || value > 30000) {
        return false;
    }
    milliseconds = value;
    return true;
}
