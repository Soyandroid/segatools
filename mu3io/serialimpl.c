#include <windows.h>

#include <process.h>
#include <stdbool.h>
#include <stdint.h>

#include "mu3io/leddata.h"
#include "mu3io/serialimpl.h"

#include "util/dprintf.h"

static HANDLE mu3_serial_port;

HRESULT mu3_led_serial_init(wchar_t led_com[MAX_PATH], DWORD baud)
{
    // Setup the serial communications
    BOOL status;

    mu3_serial_port = CreateFileW(led_com,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
    
    if (mu3_serial_port == INVALID_HANDLE_VALUE)
    {
        dprintf("Ongeki Serial LEDs: Failed to open COM port (attempted on %S)\n", led_com);
        return E_FAIL;
    }
            
    DCB dcb_serial_params = { 0 };
    dcb_serial_params.DCBlength = sizeof(dcb_serial_params);
    status = GetCommState(mu3_serial_port, &dcb_serial_params);
    
    dcb_serial_params.BaudRate = baud;
    dcb_serial_params.ByteSize = 8;
    dcb_serial_params.StopBits = ONESTOPBIT;
    dcb_serial_params.Parity   = NOPARITY; 
    SetCommState(mu3_serial_port, &dcb_serial_params);
    
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 50; 
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    
    SetCommTimeouts(mu3_serial_port, &timeouts);

    if (!status)
    {
        return E_FAIL;
    }

    dprintf("Ongeki Serial LEDs: COM port opened successfully.\n");

    return S_OK;
}

void mu3_led_serial_update(struct _ongeki_led_data_buf_t* data)
{
    if (data->board > 1)
    {
        return;
    }

    if (mu3_serial_port != INVALID_HANDLE_VALUE) 
    {
        DWORD bytes_written = 0;

        BOOL status = WriteFile(
            mu3_serial_port, 
            data, 
            LED_OUTPUT_HEADER_SIZE + data->data_len, 
            &bytes_written, 
            NULL);

        if (!status) 
        {
            DWORD last_err = GetLastError();
            dprintf("Ongeki Serial LEDs: Serial port write failed -- %d\n", last_err); 
        }
    } 
}
