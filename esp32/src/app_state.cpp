#include "app_state.h"
#include "board_config.h"
String errorText;

bool usablePin(int pin) {
    // Exclude flash (12..17), boot straps (2/8/9), USB (18/19), UART (20/21).
    return pin == 0 || pin == 1 || (pin >= 3 && pin <= 7) || pin == 10;
}

bool validateBoardPins() {
    bool buttonValid = BUTTON_PIN == -1 || (usablePin(BUTTON_PIN) && BUTTON_PIN != MOTOR_PIN);
    bool oledValid = (OLED_SDA == -1 && OLED_SCL == -1) ||
                     (usablePin(OLED_SDA) && usablePin(OLED_SCL) && OLED_SDA != OLED_SCL &&
                      OLED_SDA != MOTOR_PIN && OLED_SCL != MOTOR_PIN && OLED_SDA != BUTTON_PIN &&
                      OLED_SCL != BUTTON_PIN);
    const int pins[] = {MOTOR_PIN,
                        BUTTON_PIN,
                        OLED_SDA,
                        OLED_SCL,
                        CHARGE_PIN,
                        CHARGE_DONE_PIN,
                        BATTERY_ADC_ENABLED ? BATTERY_PIN : -1};
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        for (unsigned j = i + 1; j < sizeof(pins) / sizeof(pins[0]); ++j) {
            if (pins[i] >= 0 && pins[i] == pins[j]) {
                return false;
            }
        }
    }
    return buttonValid && oledValid;
}
