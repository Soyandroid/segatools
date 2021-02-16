#pragma once

#include <windows.h>

#include "mu3io/leddata.h"

HRESULT mu3_led_pipe_init();
void mu3_led_pipe_update(struct _ongeki_led_data_buf_t* data);
