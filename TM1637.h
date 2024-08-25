#ifndef TM1637_H
#define TM1637_H

#include <stdbool.h>
#include <stdint.h>

#define TM1637_MAX_DIGITS 6

enum TM1637_Brightness {
    TM1637_BRIGHTNESS_1_16 = 00,
    TM1637_BRIGHTNESS_2_16 = 01,
    TM1637_BRIGHTNESS_4_16 = 02,
    TM1637_BRIGHTNESS_10_16 = 03,
    TM1637_BRIGHTNESS_11_16 = 04,
    TM1637_BRIGHTNESS_12_16 = 05,
    TM1637_BRIGHTNESS_13_16 = 06,
    TM1637_BRIGHTNESS_14_16 = 07
};

enum TM1637_DisplayOn {
    TM1637_DISPLAY_OFF = 000,
    TM1637_DISPLAY_ON = 010
};

enum TM1637_GPIOConfig {
    TM1637_GPIO_PULLUP_INPUT,
    TM1637_GPIO_PULLUP_OUTPUT,
};

struct TM1637_Platform {
    int (*gpioGet)(int pin);
    void (*gpioSet)(int pin, int value);
    void (*gpioSwitch)(int pin, enum TM1637_GPIOConfig config);

    void (*delayUs)(int us);
    void (*debugPrint)(const char *fmt, ...);

    int digitNum;
    int pinDIO;
    int pinCLK;
};

void TM1637_Init(struct TM1637_Platform *platform);

bool TM1637_DisplayRawData(const uint8_t *data, int count, enum TM1637_Brightness brightness);
bool TM1637_DisplayBCD(const uint8_t *bcd, int count, enum TM1637_Brightness brightness);
bool TM1637_DisplayASCII(const char *text, enum TM1637_Brightness brightness);
bool TM1637_DisplayInteger(int value, enum TM1637_Brightness brightness);
bool TM1637_DisplayFloat(float value, int precision, enum TM1637_Brightness brightness);

bool TM1637_DisplayOff(void);

const char *TM1637_UnitTest(void);

#endif // TM1637_H
