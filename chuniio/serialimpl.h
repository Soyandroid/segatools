#pragma once

#include <windows.h>

#include "chuniio/leddata.h"

HRESULT led_serial_init(wchar_t led_com[MAX_PATH], DWORD baud);
void led_serial_update(struct _chuni_led_data_buf_t* data);
