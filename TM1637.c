#include <string.h>
#include "TM1637.h"

#define CLOCK_PERIOD_US 3 // min 1 us (clock frequency should be less than 250K)

#define TM1637_WRITE_DISPLAY 00
#define TM1637_READ_KEY 02

#define TM1637_ADDR_ADD 00
#define TM1637_ADDR_FIX 04

#define TM1637_NORMAL_MODE 000
#define TM1637_TEST_MODE 010

// Based on https://github.com/petrows/esp-32-tm1637/blob/master/tm1637.c
// NOTE: bits are XGFEDCBA, X is DP
#define SEG_0 0x3F // 0b00111111 : 0
#define SEG_1 0x06 // 0b00000110 : 1
#define SEG_2 0x5B // 0b01011011 : 2
#define SEG_3 0x4F // 0b01001111 : 3
#define SEG_4 0x66 // 0b01100110 : 4
#define SEG_5 0x6D // 0b01101101 : 5
#define SEG_6 0x7D // 0b01111101 : 6
#define SEG_7 0x07 // 0b00000111 : 7
#define SEG_8 0x7F // 0b01111111 : 8
#define SEG_9 0x6F // 0b01101111 : 9
#define SEG_A 0x77 // 0b01110111 : A
#define SEG_B 0x7C // 0b01111100 : b
#define SEG_C 0x39 // 0b00111001 : C
#define SEG_D 0x5E // 0b01011110 : d
#define SEG_E 0x79 // 0b01111001 : E
#define SEG_F 0x71 // 0b01110001 : F

#define SEG_DP 0x80 // 0b10000000 : DP
#define SEG_MN 0x40 // 0b01000000 : -
#define SEG_BL 0x00 // 0b00000000 : Blank

static struct TM1637_Platform platform;

static const uint8_t segmentBCD[16] = {
    SEG_0,
    SEG_1,
    SEG_2,
    SEG_3,
    SEG_4,
    SEG_5,
    SEG_6,
    SEG_7,
    SEG_8,
    SEG_9,
    SEG_A,
    SEG_B,
    SEG_C,
    SEG_D,
    SEG_E,
    SEG_F
};

static const uint8_t segmentASCII[256] = {
    // 0x00 - 0x0F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x10 - 0x1F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x20 - 0x2F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, SEG_DP, 0,
    // 0x30 - 0x3F
    SEG_0, SEG_1, SEG_2, SEG_3, SEG_4, SEG_5, SEG_6, SEG_7, SEG_8, SEG_9, 0, 0, 0, 0, 0, 0,
    // 0x40 - 0x4F
    0, SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x50 - 0x5F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x60 - 0x6F
    0, SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x70 - 0x7F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void sendStart(void) {
    // Drop DIO, CLK high
    platform.gpioSet(platform.pinDIO, 0);

    // Wait clock period (NOTE: maybe period can be reduced)
    platform.delayUs(CLOCK_PERIOD_US);
}

static bool sendByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        // Clock low, load data bit
        platform.gpioSet(platform.pinCLK, 0);
        platform.gpioSet(platform.pinDIO, data & 0x01);
        platform.delayUs(CLOCK_PERIOD_US);

        // Clock high, keep data bit
        platform.gpioSet(platform.pinCLK, 1);
        platform.delayUs(CLOCK_PERIOD_US);

        // Next bit
        data >>= 1;
    }

    // Release DIO before the falling edge to read chip ACK
    platform.gpioSwitch(platform.pinDIO, TM1637_GPIO_FLOATING_INPUT);
    platform.gpioSet(platform.pinCLK, 0);
    platform.delayUs(CLOCK_PERIOD_US);

    // Read ACK, should be low
    int ack = platform.gpioGet(platform.pinDIO);
    platform.gpioSet(platform.pinCLK, 1);
    platform.delayUs(CLOCK_PERIOD_US);

    // Take DIO back
    // TODO: should this be done after 9-th clock pulse end (falling edge)?
    platform.gpioSwitch(platform.pinDIO, TM1637_GPIO_PULLUP_OUTPUT);

    return !ack;
}

static void sendStop(void) {
    // CLK low, DIO low
    platform.gpioSet(platform.pinCLK, 0);
    platform.gpioSet(platform.pinDIO, 0);
    platform.delayUs(CLOCK_PERIOD_US);

    // CLK high, DIO low
    platform.gpioSet(platform.pinCLK, 1);
    platform.gpioSet(platform.pinDIO, 0);
    platform.delayUs(CLOCK_PERIOD_US);

    // CLK high, DIO low to high - end of transmission
    platform.gpioSet(platform.pinDIO, 1);
    platform.delayUs(CLOCK_PERIOD_US);
}

static bool dataCommandSet(int dataMode, int addrMode, int testMode) {
    bool ok;

    uint8_t data = 0100 | dataMode | addrMode | testMode;
    ok = sendByte(data);

    return ok;
}

static bool addrCommandSet(int addr) {
    bool ok;

    uint8_t data = 0300 | (addr & 07);
    ok = sendByte(data);

    return ok;
}

static bool displayControl(enum TM1637_Brightness brightness, enum TM1637_DisplayOn on) {
    bool ok;

    uint8_t data = 0200 | brightness | on;
    ok = sendByte(data);

    return ok;
}

void TM1637_Init(struct TM1637_Platform *platformPtr) {
    platform = *platformPtr;

    // Init GPIO
    platform.gpioSwitch(platform.pinCLK, TM1637_GPIO_PULLUP_OUTPUT);
    platform.gpioSet(platform.pinCLK, 1);

    platform.gpioSwitch(platform.pinDIO, TM1637_GPIO_PULLUP_OUTPUT);
    platform.gpioSet(platform.pinDIO, 1);
}

bool TM1637_DisplayRawData(const uint8_t *data, int count, enum TM1637_Brightness brightness) {
    bool ok = true;

    // Data command setting (auto address increment)
    sendStart();
    ok &= dataCommandSet(TM1637_WRITE_DISPLAY, TM1637_ADDR_ADD, TM1637_NORMAL_MODE);
    sendStop();

    // Set initial address and write data bytes
    sendStart();
    ok &= addrCommandSet(0);
    for (int i = 0; i < count; i++) {
        ok &= sendByte(data[i]);
    }
    sendStop();

    // Display control
    sendStart();
    ok &= displayControl(brightness, TM1637_DISPLAY_ON);
    sendStop();

    if (!ok) {
        platform.debugPrint("TM1637: Failed to display data (ACK error)\r\n");
    }

    return ok;
}

bool TM1637_DisplayBCD(const uint8_t *bcd, int count, enum TM1637_Brightness brightness) {
    // Check count
    if (count > TM1637_MAX_DIGITS || count > platform.digitNum) {
        count = platform.digitNum;
    }

    // Encode BCD
    uint8_t data[TM1637_MAX_DIGITS];
    for (int i = 0; i < count; i++) {
        data[i] = segmentBCD[bcd[i]];
    }

    // Display data
    return TM1637_DisplayRawData(data, count, brightness);
}

bool TM1637_DisplayASCII(const char *text, enum TM1637_Brightness brightness) {
    // Check text length
    int len = strlen(text);
    if (len > TM1637_MAX_DIGITS || len > platform.digitNum) {
        len = platform.digitNum;
    }

    // Encode ASCII
    uint8_t data[TM1637_MAX_DIGITS];
    for (int i = 0; i < len; i++) {
        data[i] = segmentASCII[text[i] & 0x7F];
    }

    // Display data
    return TM1637_DisplayRawData(data, len, brightness);
}

bool TM1637_DisplayDecimal(int value, int dpPos, enum TM1637_Brightness brightness) {
    // Check minus sign
    int minusSign = 0;
    if (value < 0) {
        minusSign = 1;
        value = -value;
    }

    // Encode decimal -> BCD -> segment
    uint8_t data[TM1637_MAX_DIGITS];
    for (int i = 0; i < TM1637_MAX_DIGITS; i++) {
        data[TM1637_MAX_DIGITS - i - 1] = segmentBCD[value % 10];
        value /= 10;
    }

    // Replace leading zeros with blanks
    int digit = 0;
    while (data[digit] == SEG_0 && digit < TM1637_MAX_DIGITS && digit < TM1637_MAX_DIGITS - dpPos - 1) {
        data[digit] = SEG_BL;
        digit++;
    }

    // Prepend with minus sign
    if (minusSign && digit > 0) {
        data[digit - 1] = SEG_MN;
    }

    // Write decimal point
    if (dpPos >= 0) {
        data[TM1637_MAX_DIGITS - dpPos - 1] |= SEG_DP;
    }

    // Shift extra digits (if actual digit count is less than max)
    if (platform.digitNum < TM1637_MAX_DIGITS) {
        for (int i = 0; i < platform.digitNum; i++) {
            data[i] = data[i + TM1637_MAX_DIGITS - platform.digitNum];
        }
    }

    // Display data
    return TM1637_DisplayRawData(data, platform.digitNum, brightness);
}

bool TM1637_DisplayFloat(float value, int precision, enum TM1637_Brightness brightness) {
    // Convert to decimal integer
    for (int i = 0; i < precision; i++) {
        value *= 10.0f;
    }

    int decValue = (int)value;

    // Display decimal integer
    return TM1637_DisplayDecimal(decValue, (precision > 0) ? precision : -1, brightness);
}

bool TM1637_DisplayOff(void) {
    bool ok;

    sendStart();
    ok = displayControl(TM1637_BRIGHTNESS_1_16, TM1637_DISPLAY_OFF);
    sendStop();

    if (!ok) {
        platform.debugPrint("TM1637: Failed to display off (ACK error)\r\n");
    }

    return ok;
}
