#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct chuni_io_config {
    // Key configurations
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;
    uint8_t vk_ir[6];
    uint8_t ir_key_enable;
    uint8_t vk_cell[32];
    
    // Which ways to output LED information are enabled
    bool ledstrip_output_pipe;
    bool ledstrip_output_serial;
    
    bool slider_led_output_pipe;
    bool slider_led_output_serial;

    // The name of a COM port to output LED data on, in serial mode
    wchar_t led_serial_port[MAX_PATH];
    int32_t led_serial_baud;
};

void chuni_io_config_load(
        struct chuni_io_config *cfg,
        const wchar_t *filename);
