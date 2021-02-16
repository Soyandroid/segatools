#pragma once

#include <windows.h>

#include <stdint.h>

enum {
    MU3_IO_OPBTN_TEST = 0x01,
    MU3_IO_OPBTN_SERVICE = 0x02,
};

enum {
    MU3_IO_GAMEBTN_1 = 0x01,
    MU3_IO_GAMEBTN_2 = 0x02,
    MU3_IO_GAMEBTN_3 = 0x04,
    MU3_IO_GAMEBTN_SIDE = 0x08,
    MU3_IO_GAMEBTN_MENU = 0x10,
};

enum {
    /* These are the bitmasks to use when checking which
       lights are triggered on incoming IO4 GPIO writes. */
    MU3_IO_LED_L1_R = 1 << 31,
    MU3_IO_LED_L1_G = 1 << 28,
    MU3_IO_LED_L1_B = 1 << 30,
    MU3_IO_LED_L2_R = 1 << 27,
    MU3_IO_LED_L2_G = 1 << 29,
    MU3_IO_LED_L2_B = 1 << 26,
    MU3_IO_LED_L3_R = 1 << 25,
    MU3_IO_LED_L3_G = 1 << 24,
    MU3_IO_LED_L3_B = 1 << 23,
    MU3_IO_LED_R1_R = 1 << 22,
    MU3_IO_LED_R1_G = 1 << 21,
    MU3_IO_LED_R1_B = 1 << 20,
    MU3_IO_LED_R2_R = 1 << 19,
    MU3_IO_LED_R2_G = 1 << 18,
    MU3_IO_LED_R2_B = 1 << 17,
    MU3_IO_LED_R3_R = 1 << 16,
    MU3_IO_LED_R3_G = 1 << 15,
    MU3_IO_LED_R3_B = 1 << 14,
};

HRESULT mu3_io_init(void);

HRESULT mu3_io_poll(void);

void mu3_io_get_opbtns(uint8_t *opbtn);

void mu3_io_get_gamebtns(uint8_t *left, uint8_t *right);

void mu3_io_get_lever(int16_t *pos);

void mu3_io_set_leds(int board, const uint8_t *rgb);
