#pragma once
#include "../display_stubs/Arduino.h"
constexpr int HEX = 16;

struct TestEsp {
    uint64_t getEfuseMac() {
        return 123;
    }
} inline ESP;

struct TestSerial {
    template <typename... Args> void printf(const char *, Args...) {
    }

    void println(const String &) {
    }
} inline Serial;
