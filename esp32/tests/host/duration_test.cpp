#include "duration_input.h"

#define CHECK(condition)                                                                           \
    if (!(condition)) {                                                                            \
        return __LINE__;                                                                           \
    }

int main() {
    uint32_t ms = 0;
    CHECK(parseDurationSeconds("2", ms) && ms == 2000);
    CHECK(parseDurationSeconds("0.1", ms) && ms == 100);
    CHECK(parseDurationSeconds("0.5", ms) && ms == 500);
    CHECK(parseDurationSeconds("1.234", ms) && ms == 1234);
    CHECK(parseDurationSeconds("30", ms) && ms == 30000);
    CHECK(parseDurationSeconds("30.000", ms) && ms == 30000);
    CHECK(!parseDurationSeconds("0.099", ms));
    CHECK(!parseDurationSeconds("30.001", ms));
    CHECK(!parseDurationSeconds("30000", ms));
    CHECK(!parseDurationSeconds("0.1001", ms));
    CHECK(!parseDurationSeconds("", ms));
    CHECK(!parseDurationSeconds("-1", ms));
    CHECK(!parseDurationSeconds("NaN", ms));
    CHECK(!parseDurationSeconds("1e1", ms));
    CHECK(!parseDurationSeconds("1.2.3", ms));
    CHECK(!parseDurationSeconds("1.", ms));
    return 0;
}
