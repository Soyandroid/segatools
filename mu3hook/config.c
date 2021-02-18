#include <assert.h>
#include <stddef.h>

#include "board/config.h"

#include "hooklib/config.h"
#include "hooklib/gfx.h"

#include "mu3hook/config.h"

#include "platform/config.h"

void led1509306_config_load(struct led1509306_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    wchar_t tmpstr[16];
    
    memset(cfg->board_number, ' ', sizeof(cfg->board_number));
    memset(cfg->chip_number, ' ', sizeof(cfg->chip_number));
    
    cfg->enable = GetPrivateProfileIntW(L"led_output", L"enable_cab_led_output", 1, filename);
    cfg->fw_ver = GetPrivateProfileIntW(L"led_output", L"fw_ver", 0xA0, filename);
    cfg->fw_sum = GetPrivateProfileIntW(L"led_output", L"fw_sum", 0xAA53, filename);
    
    GetPrivateProfileStringW(L"led_output", L"board_number", L"15093-06", tmpstr, _countof(tmpstr), filename);
    size_t n = wcstombs(cfg->board_number, tmpstr, sizeof(cfg->board_number));
    for (int i = n; i < sizeof(cfg->board_number); i++)
    {
        cfg->board_number[i] = ' ';
    }
    
    GetPrivateProfileStringW(L"led_output", L"chip_number", L"6710A", tmpstr, _countof(tmpstr), filename);
    n = wcstombs(cfg->chip_number, tmpstr, sizeof(cfg->chip_number));
    for (int i = n; i < sizeof(cfg->chip_number); i++)
    {
        cfg->chip_number[i] = ' ';
    } 
}

void mu3_hook_config_load(
        struct mu3_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    aime_config_load(&cfg->aime, filename);
    gfx_config_load(&cfg->gfx, filename);
    led1509306_config_load(&cfg->led1509306, filename);
}
