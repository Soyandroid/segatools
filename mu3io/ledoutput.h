#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

#include "mu3io/config.h"

extern HANDLE mu3_led_init_mutex;
HRESULT mu3_led_output_init(struct mu3_io_config* const cfg);
void mu3_led_output_update(int board, const byte* rgb);
