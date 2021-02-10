#include <windows.h>

#include <process.h>
#include <stdbool.h>
#include <stdint.h>

#include "chuniio/leddata.h"
#include "chuniio/pipeimpl.h"

static bool pipe_update[LED_BOARDS_TOTAL];

// incoming data is copied into these to ensure it isn't written during output
static struct _chuni_led_data_buf_t pipe_write_buf[LED_BOARDS_TOTAL];
static HANDLE pipe_write_mutex;

static HRESULT pipe_create(LPHANDLE hPipe, LPCWSTR lpszPipename, DWORD dwBufSize)
{
    *hPipe = INVALID_HANDLE_VALUE;
    
    *hPipe = CreateNamedPipeW( 
        lpszPipename,                   // pipe name
        PIPE_ACCESS_OUTBOUND,           // read/write access
        PIPE_TYPE_BYTE |                // byte type pipe
        PIPE_WAIT,                      // blocking mode
        PIPE_UNLIMITED_INSTANCES,       // max. instances 
        dwBufSize,                      // output buffer size
        0,                              // input buffer size
        0,                              // client time-out
        NULL);                          // default security attribute
    
    if (*hPipe == INVALID_HANDLE_VALUE)
    {
        return E_FAIL;
    }
    
    return S_OK;
}

static HRESULT pipe_write(HANDLE hPipe, LPCVOID lpBuffer, DWORD dwSize)
{
    DWORD cbWritten = 0;
    
    bool fSuccess = WriteFile( 
        hPipe,
        lpBuffer,
        dwSize,
        &cbWritten,
        NULL);
    
    if (!fSuccess || cbWritten != dwSize) 
    {
        DWORD last_err = GetLastError();
        return (last_err == ERROR_BROKEN_PIPE) ? E_ABORT : E_FAIL;
    }

    return S_OK;
}

static unsigned int __stdcall chuni_io_led_pipe_thread_proc(void *ctx)
{
    HANDLE hPipe;
    LPCWSTR lpszPipename = L"\\\\.\\pipe\\chuni_ledstrip";
    
    while (true)
    {
        hPipe = INVALID_HANDLE_VALUE;
        
        if (pipe_create(&hPipe, lpszPipename, LED_OUTPUT_TOTAL_SIZE_MAX) != S_OK)
        {
            continue;
        }
        
        // wait for a connection to the pipe
        bool fConnected = ConnectNamedPipe(hPipe, NULL) ?
            true : (GetLastError() == ERROR_PIPE_CONNECTED);
        
        while (fConnected)
        {
            if (WaitForSingleObject(pipe_write_mutex, INFINITE) != WAIT_OBJECT_0)
            {
                continue;
            }
            
            for (int i = 0; i < LED_BOARDS_TOTAL; i++) {
                if (pipe_update[i])
                {
                    HRESULT result = pipe_write(
                        hPipe,
                        &pipe_write_buf[i],
                        LED_OUTPUT_HEADER_SIZE + pipe_write_buf[i].data_len);

                    if (result != S_OK)
                    {
                        //if (result == E_ABORT) 
                        //{
                            fConnected = false;
                        //}
                        break;
                    }
                    
                    pipe_update[i] = false;
                }
            }
            
            ReleaseMutex(pipe_write_mutex);
        }
        
        FlushFileBuffers(hPipe); 
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
    
    return 0;
}

HRESULT led_pipe_init()
{
    pipe_write_mutex = CreateMutex(
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex
    
    if (pipe_write_mutex == NULL)
    {
        return E_FAIL;
    }
    
    // clear out update bools
    for (int i = 0; i < LED_BOARDS_TOTAL; i++) {
        pipe_update[i] = false;
    }
    
    _beginthreadex(
            NULL,
            0,
            chuni_io_led_pipe_thread_proc,
            0,
            0,
            NULL);
    
    return S_OK;
}

void led_pipe_update(struct _chuni_led_data_buf_t* data)
{
    if (data->board > 2)
    {
        return;
    }
    
    if (WaitForSingleObject(pipe_write_mutex, INFINITE) != WAIT_OBJECT_0)
    {
        return;
    }
    
    memcpy(&pipe_write_buf[data->board], data, sizeof(struct _chuni_led_data_buf_t));
    pipe_update[data->board] = true;
    
    ReleaseMutex(pipe_write_mutex);
}
