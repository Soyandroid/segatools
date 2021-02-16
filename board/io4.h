#pragma once

#include <windows.h>

#include <stdint.h>

#define IO4_REPORT_OUT_PAYLOAD_LEN 62

enum {
    /* System buttons in button[0] */

    IO4_BUTTON_TEST     = 1 << 9,
    IO4_BUTTON_SERVICE  = 1 << 6,
};

struct io4_state {
    uint16_t adcs[8];
    uint16_t spinners[4];
    uint16_t chutes[2];
    uint16_t buttons[2];
};

struct io4_ops {
    HRESULT (*poll)(void *ctx, struct io4_state *state);
    HRESULT (*write_gpio)(uint8_t* payload, size_t len);
};

HRESULT io4_hook_init(const struct io4_ops *ops, void *ctx);
