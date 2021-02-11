#include <windows.h>

#include <process.h>
#include <stdbool.h>
#include <stdint.h>

#include "chuniio/leddata.h"
#include "chuniio/serialimpl.h"
#include "util/dprintf.h"

static HANDLE serial_port;
static HANDLE serial_write_mutex;

HRESULT led_serial_init(wchar_t led_com[MAX_PATH], DWORD baud)
{
    // Setup the serial communications
    BOOL status;

    serial_port = CreateFileW(led_com,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
    
    /*if (serial_port == INVALID_HANDLE_VALUE)
        dprintf("Chunithm Serial LEDs: Failed to open COM port (attempted on %S)\n", led_com);
    else
        dprintf("Chunithm Serial LEDs: COM port success!\n");*/
            
    DCB dcb_serial_params = { 0 };
    dcb_serial_params.DCBlength = sizeof(dcb_serial_params);
    status = GetCommState(serial_port, &dcb_serial_params);
    
    dcb_serial_params.BaudRate = baud;
    dcb_serial_params.ByteSize = 8;
    dcb_serial_params.StopBits = ONESTOPBIT;
    dcb_serial_params.Parity   = NOPARITY; 
    SetCommState(serial_port, &dcb_serial_params);
    
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 50; 
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    
    SetCommTimeouts(serial_port, &timeouts);

    if (!status)
    {
        return E_FAIL;
    }

    serial_write_mutex = CreateMutex(
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex

    if (serial_write_mutex == NULL)
    {
        return E_FAIL;
    }

    return S_OK;
}

void led_serial_update(struct _chuni_led_data_buf_t* data)
{
    if (data->board > 2)
    {
        return;
    }
    
    if (WaitForSingleObject(serial_write_mutex, INFINITE) != WAIT_OBJECT_0)
    {
        return;
    }

    BOOL status = true;
    DWORD bytes_written = 0;

    if (serial_port != INVALID_HANDLE_VALUE) {
        status = WriteFile(
            serial_port, 
            data, 
            LED_OUTPUT_HEADER_SIZE + data->data_len, 
            &bytes_written, 
            NULL);
    }

    if (!status) {
        DWORD last_err = GetLastError();
        // dprintf("Chunithm Serial LEDs: Serial port write failed -- %d\n", last_err); 
    }
    
    ReleaseMutex(serial_write_mutex);
}
