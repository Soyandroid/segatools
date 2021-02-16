#pragma once

#include <windows.h>

#include <stdint.h>

HRESULT mu3_io4_hook_init(void);
HRESULT mu3_io4_write_gpio(uint8_t* payload, size_t len);
