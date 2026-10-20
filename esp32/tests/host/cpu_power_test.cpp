#define FEEDER_HOST_TEST
#include "../../src/cpu_power.cpp"

namespace {
uint32_t testNow = 0;
uint32_t mhz = 160;
unsigned changes = 0;
} // namespace

String errorText = "";

uint32_t millis() {
    return testNow;
}

uint32_t getCpuFrequencyMhz() {
    return mhz;
}

bool setCpuFrequencyMhz(uint32_t frequency) {
    mhz = frequency;
    ++changes;
    return true;
}

#define CHECK(condition)                                                                           \
    if (!(condition)) {                                                                            \
        return __LINE__;                                                                           \
    }

int main() {
    beginCpuPower();
    CHECK(mhz == 80 && changes == 1);
    // 已在80MHz时初始化不会重复改频；不存在Web升频入口。
    beginCpuPower();
    CHECK(mhz == 80 && changes == 1);
    testNow = 0xFFFFFF00u;
    beginCpuPower();
    CHECK(mhz == 80 && changes == 1);
    return 0;
}
