#pragma once

class TestWire {
  public:
    void begin(int, int) {
    }

    void setTimeOut(int) {
    }

    void beginTransmission(int) {
    }

    int endTransmission() {
        return 0;
    }
};

inline TestWire Wire;
