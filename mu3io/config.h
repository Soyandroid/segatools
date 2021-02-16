#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mu3_io_config {
    // Which ways to output LED information are enabled
    bool cab_led_output_pipe;
    bool cab_led_output_serial;
    
    bool controller_led_output_pipe;
    bool controller_led_output_serial;

    // The name of a COM port to output LED data on, in serial mode
    wchar_t led_serial_port[MAX_PATH];
    int32_t led_serial_baud;
};

void mu3_io_config_load(
        struct mu3_io_config *cfg,
        const wchar_t *filename);
