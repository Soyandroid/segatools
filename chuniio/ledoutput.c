#include <windows.h>

#include <process.h>
#include <stdbool.h>
#include <stdint.h>

#include "chuniio/config.h"
#include "chuniio/leddata.h"
#include "chuniio/ledoutput.h"
#include "chuniio/pipeimpl.h"
#include "chuniio/serialimpl.h"

static struct _chuni_led_data_buf_t led_unescaped_buf[LED_BOARDS_TOTAL];
static struct _chuni_led_data_buf_t led_escaped_buf[LED_BOARDS_TOTAL];

static bool led_output_is_init = false;
static struct chuni_io_config* config;
static bool any_outputs_enabled;

HANDLE led_init_mutex;

HRESULT led_output_init(struct chuni_io_config* const cfg)
{
    DWORD dwWaitResult = WaitForSingleObject(led_init_mutex, INFINITE);
    if (dwWaitResult == WAIT_FAILED)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    else if (dwWaitResult != WAIT_OBJECT_0)
    {
        return E_FAIL;
    }
    
    if (!led_output_is_init)
    {
        config = cfg;
        
        // Setup the framing bytes for the packets
        for (int i = 0; i < LED_BOARDS_TOTAL; i++) {
            led_unescaped_buf[i].framing = LED_PACKET_FRAMING;
            led_unescaped_buf[i].board = i;
            led_unescaped_buf[i].data_len = chuni_led_board_data_lens[i];
            
            led_escaped_buf[i].framing = LED_PACKET_FRAMING;
            led_escaped_buf[i].board = i;
            led_escaped_buf[i].data_len = chuni_led_board_data_lens[i];
        }
        
        any_outputs_enabled = config->ledstrip_output_pipe || config->slider_led_output_pipe
            || config->ledstrip_output_serial || config->slider_led_output_serial;
        
        if (config->ledstrip_output_pipe || config->slider_led_output_pipe)
        {
            led_pipe_init();  // don't really care about errors here tbh
        }
        
        if (config->ledstrip_output_serial || config->slider_led_output_serial)
        {
            led_serial_init(config->led_serial_port, config->led_serial_baud);
        }
    }
    
    led_output_is_init = true;
    
    ReleaseMutex(led_init_mutex);
    return S_OK;
}

struct _chuni_led_data_buf_t* escape_led_data(struct _chuni_led_data_buf_t* unescaped)
{
    struct _chuni_led_data_buf_t* out_struct = &led_escaped_buf[unescaped->board];
    
    byte* in_buf = unescaped->data;
    byte* out_buf = out_struct->data;
    int i = 0;
    int o = 0;
    
    while (i < unescaped->data_len)
    {
        byte b = in_buf[i++];
        if (b == LED_PACKET_FRAMING || b == LED_PACKET_ESCAPE)
        {
            out_buf[o++] = LED_PACKET_ESCAPE;
            b--;
        }
        out_buf[o++] = b;
    }
    
    out_struct->data_len = o;
    
    return out_struct;
}

void led_output_update(int board, const byte* rgb)
{
    if (board < 0 || board > 2 || !any_outputs_enabled)
    {
        return;
    }
    
    memcpy(led_unescaped_buf[board].data, rgb, led_unescaped_buf[board].data_len);
    struct _chuni_led_data_buf_t* escaped_data = escape_led_data(&led_unescaped_buf[board]);
    
    if (board < 2)
    {
        // billboard
        if (config->ledstrip_output_pipe)
        {
            led_pipe_update(escaped_data);
        }
        
        if (config->ledstrip_output_serial)
        {
            led_serial_update(escaped_data);
        }
    }
    else
    {
        // slider
        if (config->slider_led_output_pipe)
        {
            led_pipe_update(escaped_data);
        }
        
        if (config->slider_led_output_serial)
        {
            led_serial_update(escaped_data);
        }
    }
}
