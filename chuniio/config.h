#pragma once

#include <stddef.h>
#include <stdint.h>

struct chuni_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;
    uint8_t vk_ir_emu;
    uint8_t vk_ir[6];
    uint8_t vk_cell[32];
    uint8_t led_port;
    uint32_t led_rate;
};

void chuni_io_config_load(
        struct chuni_io_config *cfg,
        const wchar_t *filename);
