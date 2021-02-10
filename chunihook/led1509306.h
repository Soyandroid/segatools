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

HRESULT led1509306_hook_init(const struct led1509306_config *cfg);
