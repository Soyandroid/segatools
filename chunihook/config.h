#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "amex/amex.h"

#include "board/sg-reader.h"

#include "chunihook/slider.h"

#include "chunihook/led1509306.h"

#include "hooklib/gfx.h"

#include "platform/platform.h"

struct chuni_hook_config {
    struct platform_config platform;
    struct amex_config amex;
    struct aime_config aime;
    struct gfx_config gfx;
    struct slider_config slider;
    struct led1509306_config led1509306;
};

void slider_config_load(struct slider_config *cfg, const wchar_t *filename);
void chuni_hook_config_load(
        struct chuni_hook_config *cfg,
        const wchar_t *filename);
