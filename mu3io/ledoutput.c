#include <windows.h>

#include <process.h>
#include <stdbool.h>
#include <stdint.h>

#include "mu3io/config.h"
#include "mu3io/leddata.h"
#include "mu3io/ledoutput.h"
#include "mu3io/pipeimpl.h"
#include "mu3io/serialimpl.h"

static struct _ongeki_led_data_buf_t mu3_led_unescaped_buf[LED_BOARDS_TOTAL];
static struct _ongeki_led_data_buf_t mu3_led_escaped_buf[LED_BOARDS_TOTAL];

static bool mu3_led_output_is_init = false;
static struct mu3_io_config* mu3_io_config;
static bool mu3_led_any_outputs_enabled;

HANDLE mu3_led_init_mutex;

HRESULT mu3_led_output_init(struct mu3_io_config* const cfg)
{
    DWORD dwWaitResult = WaitForSingleObject(mu3_led_init_mutex, INFINITE);
    if (dwWaitResult == WAIT_FAILED)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    else if (dwWaitResult != WAIT_OBJECT_0)
    {
        return E_FAIL;
    }
    
    if (!mu3_led_output_is_init)
    {
        mu3_io_config = cfg;
        
        // Setup the framing bytes for the packets
        for (int i = 0; i < LED_BOARDS_TOTAL; i++) {
            mu3_led_unescaped_buf[i].framing = LED_PACKET_FRAMING;
            mu3_led_unescaped_buf[i].board = i;
            mu3_led_unescaped_buf[i].data_len = ongeki_led_board_data_lens[i];
            
            mu3_led_escaped_buf[i].framing = LED_PACKET_FRAMING;
            mu3_led_escaped_buf[i].board = i;
            mu3_led_escaped_buf[i].data_len = ongeki_led_board_data_lens[i];
        }
        
        mu3_led_any_outputs_enabled = mu3_io_config->cab_led_output_pipe || mu3_io_config->controller_led_output_pipe
            || mu3_io_config->cab_led_output_serial || mu3_io_config->controller_led_output_serial;
        
        if (mu3_io_config->cab_led_output_pipe || mu3_io_config->controller_led_output_pipe)
        {
            mu3_led_pipe_init();  // don't really care about errors here tbh
        }
        
        if (mu3_io_config->cab_led_output_serial || mu3_io_config->controller_led_output_serial)
        {
            mu3_led_serial_init(mu3_io_config->led_serial_port, mu3_io_config->led_serial_baud);
        }
    }
    
    mu3_led_output_is_init = true;
    
    ReleaseMutex(mu3_led_init_mutex);
    return S_OK;
}

struct _ongeki_led_data_buf_t* escape_led_data(struct _ongeki_led_data_buf_t* unescaped)
{
    struct _ongeki_led_data_buf_t* out_struct = &mu3_led_escaped_buf[unescaped->board];
    
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

void mu3_led_output_update(int board, const byte* rgb)
{
    if (board < 0 || board > 1 || !mu3_led_any_outputs_enabled)
    {
        return;
    }
    
    memcpy(mu3_led_unescaped_buf[board].data, rgb, mu3_led_unescaped_buf[board].data_len);
    struct _ongeki_led_data_buf_t* escaped_data = escape_led_data(&mu3_led_unescaped_buf[board]);
    
    if (board == 0)
    {
        // billboard
        if (mu3_io_config->cab_led_output_pipe)
        {
            mu3_led_pipe_update(escaped_data);
        }
        
        if (mu3_io_config->cab_led_output_serial)
        {
            mu3_led_serial_update(escaped_data);
        }
    }
    else
    {
        // slider
        if (mu3_io_config->controller_led_output_pipe)
        {
            mu3_led_pipe_update(escaped_data);
        }
        
        if (mu3_io_config->controller_led_output_serial)
        {
            mu3_led_serial_update(escaped_data);
        }
    }
}
