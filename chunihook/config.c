#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "amex/amex.h"
#include "amex/config.h"

#include "board/config.h"
#include "board/sg-reader.h"

#include "chunihook/config.h"

#include "hooklib/config.h"
#include "hooklib/gfx.h"

#include "platform/config.h"
#include "platform/platform.h"

void slider_config_load(struct slider_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"slider", L"enable", 1, filename);
}

void led1509306_config_load(struct led1509306_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    wchar_t tmpstr[16];
    
    memset(cfg->board_number, ' ', sizeof(cfg->board_number));
    memset(cfg->chip_number, ' ', sizeof(cfg->chip_number));
    
    cfg->enable = GetPrivateProfileIntW(L"led_output", L"enable_cab_led_output", 1, filename);
    cfg->fw_ver = GetPrivateProfileIntW(L"led_output", L"fw_ver", 0x90, filename);
    cfg->fw_sum = GetPrivateProfileIntW(L"led_output", L"fw_sum", 0xadf7, filename);
    
    GetPrivateProfileStringW(L"led_output", L"board_number", L"15093-06", tmpstr, _countof(tmpstr), filename);
    size_t n = wcstombs(cfg->board_number, tmpstr, sizeof(cfg->board_number));
    for (int i = n; i < sizeof(cfg->board_number); i++)
    {
        cfg->board_number[i] = ' ';
    }
    
    GetPrivateProfileStringW(L"led_output", L"chip_number", L"6710 ", tmpstr, _countof(tmpstr), filename);
    n = wcstombs(cfg->chip_number, tmpstr, sizeof(cfg->chip_number));
    for (int i = n; i < sizeof(cfg->chip_number); i++)
    {
        cfg->chip_number[i] = ' ';
    } 
}

void chuni_hook_config_load(
        struct chuni_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    memset(cfg, 0, sizeof(*cfg));

    platform_config_load(&cfg->platform, filename);
    amex_config_load(&cfg->amex, filename);
    aime_config_load(&cfg->aime, filename);
    gfx_config_load(&cfg->gfx, filename);
    slider_config_load(&cfg->slider, filename);
    led1509306_config_load(&cfg->led1509306, filename);
}
