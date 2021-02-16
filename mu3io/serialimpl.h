#pragma once

#include <windows.h>

#include "mu3io/leddata.h"

HRESULT mu3_led_serial_init(wchar_t led_com[MAX_PATH], DWORD baud);
void mu3_led_serial_update(struct _ongeki_led_data_buf_t* data);
