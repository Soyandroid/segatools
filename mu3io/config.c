#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "mu3io/config.h"

void mu3_io_config_load(
        struct mu3_io_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    wchar_t output_path_input[MAX_PATH];

    cfg->cab_led_output_pipe = GetPrivateProfileIntW(L"led_output", L"cab_led_output_pipe", 1, filename);
    cfg->cab_led_output_serial = GetPrivateProfileIntW(L"led_output", L"cab_led_output_serial", 0, filename);
    
    cfg->controller_led_output_pipe = GetPrivateProfileIntW(L"led_output", L"controller_led_output_pipe", 1, filename);
    cfg->controller_led_output_serial = GetPrivateProfileIntW(L"led_output", L"controller_led_output_serial", 0, filename);

    GetPrivateProfileStringW(
            L"led_output",
            L"serial_port",
            L"COM10",
            output_path_input,
            _countof(output_path_input),
            filename);

    // Sanitize the output path. If it's a serial COM port, it needs to be prefixed
    // with `\\.\`.
    wcsncpy(cfg->led_serial_port, L"\\\\.\\", 4);
    wcsncat_s(cfg->led_serial_port, MAX_PATH, output_path_input, MAX_PATH);
    
    cfg->led_serial_baud = GetPrivateProfileIntW(L"led_output", L"serial_baud", 921600, filename);
}
