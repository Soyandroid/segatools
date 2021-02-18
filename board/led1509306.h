#pragma once

#include <windows.h>

#include <stdbool.h>

struct led1509306_config {
    bool enable;
    char board_number[8];
    char chip_number[5];
    uint8_t fw_ver;
    uint16_t fw_sum;
};

typedef HRESULT (*io_ledstrip_init)(int board);
typedef void (*io_ledstrip_set_leds)(int board, const uint8_t *rgb);

io_ledstrip_init led_init;
io_ledstrip_set_leds set_leds;

HRESULT led1509306_hook_init(const struct led1509306_config *cfg, io_ledstrip_init _led_init, 
    io_ledstrip_set_leds _set_leds, int first_port, int num_boards, uint8_t board_adr, uint8_t host_adr);
