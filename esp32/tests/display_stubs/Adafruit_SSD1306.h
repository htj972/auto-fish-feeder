#pragma once
#include "Arduino.h"
#include "Wire.h"
#include <cstring>

constexpr int SSD1306_WHITE = 1;
constexpr int SSD1306_BLACK = 0;
constexpr int SSD1306_SWITCHCAPVCC = 2;
constexpr int SSD1306_SETCONTRAST = 0x81;

class Adafruit_SSD1306 {
  public:
    unsigned displayCalls = 0;
    unsigned commandCalls = 0;
    uint8_t lastCommand = 0;

    void ssd1306_command(uint8_t command) {
        ++commandCalls;
        lastCommand = command;
    }

    Adafruit_SSD1306(int, int, TestWire *, int) {
    }

    bool begin(int, int, bool, bool) {
        return true;
    }

    void setTextSize(int) {
    }

    void setTextColor(int) {
    }

    void setTextWrap(bool) {
    }

    void clearDisplay() {
        std::memset(pixels, 0, sizeof(pixels));
    }

    uint8_t *getBuffer() {
        return pixels;
    }

    void display() {
        ++displayCalls;
    }

    void drawPixel(int x, int y, int color) {
        if (x < 0 || x >= 128 || y < 0 || y >= 32) {
            return;
        }
        const uint8_t bit = uint8_t(1 << (y % 8));
        uint8_t &pixel = pixels[x + (y / 8) * 128];
        if (color) {
            pixel |= bit;
        } else {
            pixel &= ~bit;
        }
    }

    void fillRect(int x, int y, int w, int h, int color) {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                drawPixel(x + dx, y + dy, color);
            }
        }
    }

    void drawRect(int x, int y, int w, int h, int color) {
        fillRect(x, y, w, 1, color);
        fillRect(x, y + h - 1, w, 1, color);
        fillRect(x, y, 1, h, color);
        fillRect(x + w - 1, y, 1, h, color);
    }

    void drawChar(int x, int y, unsigned char ch, int color, int background, int scale) {
        // 字符代理只用于判断帧变化，不模拟实际字库的视觉效果。
        fillRect(x, y, 6 * scale, 8 * scale, background);
        for (int bit = 0; bit < 7; ++bit) {
            if (ch & (1 << bit)) {
                fillRect(x, y + bit * scale, scale, scale, color);
            }
        }
    }

  private:
    uint8_t pixels[512]{};
};
