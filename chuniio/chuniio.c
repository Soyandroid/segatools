#include <windows.h>

#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "chuniio/chuniio.h"
#include "chuniio/config.h"

static unsigned int __stdcall chuni_io_slider_thread_proc(void *ctx);

static bool chuni_io_coin;
static uint16_t chuni_io_coins;
static uint8_t chuni_io_hand_pos;
static HANDLE chuni_io_slider_thread;
static bool chuni_io_slider_stop_flag;
static struct chuni_io_config chuni_io_cfg;

/**
 *  COM-port LED protocol
 *
 *  I made up this simple protocol as I implemented on my custom hardware. I
 *  used an Arduino Leonardo to send keys over HID and receive LED over COM.
 *  The host sends 48 bytes over serial to the slider, and expects one byte
 *  of ACK. To avoid/mitigate loss in serial communication, the host must not
 *  send without receiving ACK because the device may mask interrupts when
 *  writing timing-sensitive 3-pin LED strip (e.g., WS2812B). The device may
 *  send duplicate ACKs to mitigate potentially lost ACKs. ACK indicates that
 *  the device is receiving interrupts and has empty receiving buffer, expecting
 *  a full 48-byte request. Encoding is explained below.
 *  Request (H2D): 48 bytes. 16 colors from right to left. Each color is in RGB.
 *  Response (D2H): 1 byte. Character 'A' (0x41).
 */
static FILE *logfd = NULL;
static HANDLE led_fd = INVALID_HANDLE_VALUE;
static volatile enum {
    INVALID = 0,
    AVAILABLE,
    BUSY
} led_status;
static uint8_t led_colors[48];
static volatile uint32_t led_colors_hash, led_colors_prevhash;

HRESULT chuni_io_jvs_init(void)
{
    chuni_io_config_load(&chuni_io_cfg, L".\\segatools.ini");

    return S_OK;
}

void chuni_io_jvs_read_coin_counter(uint16_t *out)
{
    if (out == NULL) {
        return;
    }

    if (GetAsyncKeyState(chuni_io_cfg.vk_coin)) {
        if (!chuni_io_coin) {
            chuni_io_coin = true;
            chuni_io_coins++;
        }
    } else {
        chuni_io_coin = false;
    }

    *out = chuni_io_coins;
}

void chuni_io_jvs_poll(uint8_t *opbtn, uint8_t *beams)
{
    size_t i;

    if (GetAsyncKeyState(chuni_io_cfg.vk_test)) {
        *opbtn |= 0x01; /* Test */
    }

    if (GetAsyncKeyState(chuni_io_cfg.vk_service)) {
        *opbtn |= 0x02; /* Service */
    }

    if (chuni_io_cfg.vk_ir_emu) {
        // Use emulated AIR
        if (GetAsyncKeyState(chuni_io_cfg.vk_ir_emu)) {
            if (chuni_io_hand_pos < 6) {
                chuni_io_hand_pos++;
            }
        } else {
            if (chuni_io_hand_pos > 0) {
                chuni_io_hand_pos--;
            }
        }

        for (i = 0 ; i < 6 ; i++) {
            if (chuni_io_hand_pos > i) {
                *beams |= (1 << i);
            }
        }
    } else {
        // Use actual AIR
        // IR format is beams[5:0] = {b5,b6,b3,b4,b1,b2};
        for (i = 0 ; i < 3 ; i++) {
            if (GetAsyncKeyState(chuni_io_cfg.vk_ir[i*2]) & 0x8000)
                *beams |= (1 << (i*2+1));
            if (GetAsyncKeyState(chuni_io_cfg.vk_ir[i*2+1]) & 0x8000)
                *beams |= (1 << (i*2));
        }
    }
}

void chuni_io_jvs_set_coin_blocker(bool open)
{}

static void chuni_io_led_init(struct chuni_io_config *cfg)
{
    led_status = INVALID;
    if (!logfd) {
        logfd = fopen("ledlog.txt", "a");
    }
    fprintf(logfd, "LED Init called\n");
    if (!cfg->led_port) {
        return;
    }
    char comname[8];
    snprintf(comname, sizeof(comname), "COM%d", cfg->led_port);
    if (led_fd != INVALID_HANDLE_VALUE) {
        CloseHandle(led_fd);
    }
    led_fd = CreateFile(comname,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        0,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        0);
    if (led_fd == INVALID_HANDLE_VALUE) {
        fprintf(logfd, "Cannot open LED COM port: %ld\n", GetLastError());
        return;
    }
    // Set COM parameters
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(led_fd, &dcbSerialParams)) {
        fprintf(logfd, "Cannot get LED COM port parameters: %ld\n", GetLastError());
        goto fail_cleanup;
    }
    dcbSerialParams.BaudRate = cfg->led_rate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (!SetCommState(led_fd, &dcbSerialParams)) {
        fprintf(logfd, "Cannot set LED COM port parameters: %ld\n", GetLastError());
        goto fail_cleanup;
    }
    // Set COM timeout to nonblocking reads
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    if (!SetCommTimeouts(led_fd, &timeouts)) {
        fprintf(logfd, "Cannot set LED COM timeouts: %ld\n", GetLastError());
        goto fail_cleanup;
    }

    if (!EscapeCommFunction(led_fd, SETDTR)) {
        fprintf(logfd, "Cannot set LED COM DTR%ld\n", GetLastError());
        goto fail_cleanup;
    }

    if (!EscapeCommFunction(led_fd, SETRTS)) {
        fprintf(logfd, "Cannot set LED COM RTS%ld\n", GetLastError());
        goto fail_cleanup;
    }

    led_status = BUSY; // Wait for ACK
    fprintf(logfd, "LED COM connected.\n");
    return;

fail_cleanup:
    CloseHandle(led_fd);
    led_fd = INVALID_HANDLE_VALUE;
}

HRESULT chuni_io_slider_init(void)
{
    return S_OK;
}

void chuni_io_slider_start(chuni_io_slider_callback_t callback)
{
    if (chuni_io_slider_thread != NULL) {
        return;
    }

    chuni_io_slider_thread = (HANDLE) _beginthreadex(
            NULL,
            0,
            chuni_io_slider_thread_proc,
            callback,
            0,
            NULL);
}

void chuni_io_slider_stop(void)
{
    if (chuni_io_slider_thread == NULL) {
        return;
    }

    chuni_io_slider_stop_flag = true;

    WaitForSingleObject(chuni_io_slider_thread, INFINITE);
    CloseHandle(chuni_io_slider_thread);
    chuni_io_slider_thread = NULL;
    chuni_io_slider_stop_flag = false;
}

void chuni_io_slider_set_leds(const uint8_t *rgb)
{
    int i;
    if (led_status == INVALID) {
        return;
    }
    // Remap GBR to RGB
    for (int i = 0; i < 16; i++) {
        led_colors[i*3+0] = rgb[i*6+1];
        led_colors[i*3+1] = rgb[i*6+2];
        led_colors[i*3+2] = rgb[i*6+0];
    }
    // Detect change in current LED colors by simple FNV hash
    // Only send colors to COM port if changed to reduce bus load (useful?)
    uint32_t hash = 0x811c9dc5;
    for (i = 0; i < 96; i++) {
        hash ^= rgb[i];
        hash *= 0x01000193;
    }
    if (led_colors_hash != hash) {
        led_colors_hash = hash;
    }
    // Actual COM write will happen in slider thread below
}

static unsigned int __stdcall chuni_io_slider_thread_proc(void *ctx)
{
    chuni_io_slider_callback_t callback;
    uint8_t pressure[32];
    size_t i;
    DWORD n;
    ssize_t ret;
    char buf;

    callback = ctx;

    chuni_io_led_init(&chuni_io_cfg);

    while (!chuni_io_slider_stop_flag) {
        for (i = 0 ; i < _countof(pressure) ; i++) {
            if (GetAsyncKeyState(chuni_io_cfg.vk_cell[i]) & 0x8000) {
                pressure[i] = 128;
            } else {
                pressure[i] = 0;
            }
        }

        callback(pressure);

        // Write LED colors to COM if there are pending requests
        if (led_status != INVALID) {
            ret = ReadFile(led_fd, &buf, 1, &n, NULL);
            if (!ret) {
                fprintf(logfd, "LED COM read failed: %ld\n", GetLastError());
                led_status = INVALID;
            } else if (n == 1) {
                if (buf == 'A') { // ACK. Safe to send
                    led_status = AVAILABLE;
                } else if (buf == 'N') { // NAK. Resend immediately
                    led_status = AVAILABLE;
                    led_colors_prevhash = ~led_colors_hash;
                    // fprintf(logfd, "LED Device NAK\n");
                }
            }
            if (led_colors_hash != led_colors_prevhash) {
                if (led_status == AVAILABLE) {
                    ret = WriteFile(led_fd, led_colors, sizeof(led_colors), &n, NULL);
                    if (!ret || n != sizeof(led_colors)) {
                        fprintf(logfd, "LED COM Write failed: %ld\n", GetLastError());
                        led_status = INVALID;
                    } else {
                        // OK. Set expecting ACK. Reset update flag
                        led_status = BUSY;
                        led_colors_prevhash = led_colors_hash;
                    }
                }
            }
        }

        Sleep(1);
    }

    if (led_fd != INVALID_HANDLE_VALUE) {
        CloseHandle(led_fd);
        led_fd = INVALID_HANDLE_VALUE;
    }

    return 0;
}
