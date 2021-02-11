#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "chuniio/config.h"

static const int chuni_io_default_cells[] = {
    'L', 'L', 'L', 'L',
    'K', 'K', 'K', 'K',
    'J', 'J', 'J', 'J',
    'H', 'H', 'H', 'H',
    'G', 'G', 'G', 'G',
    'F', 'F', 'F', 'F',
    'D', 'D', 'D', 'D',
    'S', 'S', 'S', 'S',
};

static const int chuni_io_default_ir[] = {
    VK_OEM_PERIOD, VK_OEM_2, VK_OEM_1, VK_OEM_7, VK_OEM_4, VK_OEM_6,
};


void chuni_io_config_load(
        struct chuni_io_config *cfg,
        const wchar_t *filename)
{
    wchar_t key[16];
    wchar_t output_path_input[MAX_PATH];
    int i;

    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io3", L"test", '1', filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io3", L"service", '2', filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io3", L"coin", '3', filename);
    cfg->ir_key_enable = GetPrivateProfileIntW(L"io3", L"enableirkeys", 0, filename);

    if(cfg->ir_key_enable) {
        for (i = 0 ; i < 6 ; i++) {
            swprintf_s(key, _countof(key), L"ir%i", i + 1);
            cfg->vk_ir[i] = GetPrivateProfileIntW(
                    L"io3",
                    key,
                    chuni_io_default_ir[i],
                    filename);
        }
    }
    else {
        cfg->vk_ir[0] = GetPrivateProfileIntW(L"io3", L"ir", VK_SPACE, filename);
    }   


    for (i = 0 ; i < 32 ; i++) {
        swprintf_s(key, _countof(key), L"cell%i", i + 1);
        cfg->vk_cell[i] = GetPrivateProfileIntW(
                L"slider",
                key,
                chuni_io_default_cells[i],
                filename);
    }

    cfg->ledstrip_output_pipe = GetPrivateProfileIntW(L"ledstrip", L"billboard_output_pipe", 1, filename);
    cfg->ledstrip_output_serial = GetPrivateProfileIntW(L"ledstrip", L"billboard_output_serial", 0, filename);
    
    cfg->slider_led_output_pipe = GetPrivateProfileIntW(L"ledstrip", L"slider_output_pipe", 1, filename);
    cfg->slider_led_output_serial = GetPrivateProfileIntW(L"ledstrip", L"slider_output_serial", 0, filename);

    GetPrivateProfileStringW(
            L"ledstrip",
            L"serial_port",
            L"COM10",
            output_path_input,
            _countof(output_path_input),
            filename);

    // Sanitize the output path. If it's a serial COM port, it needs to be prefixed
    // with `\\.\`.
    wcsncpy(cfg->led_serial_port, L"\\\\.\\", 4);
    wcsncat_s(cfg->led_serial_port, MAX_PATH, output_path_input, MAX_PATH);
    
    cfg->led_serial_baud = GetPrivateProfileIntW(L"ledstrip", L"serial_baud", 921600, filename);
}
