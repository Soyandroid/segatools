#include <windows.h>

#include <assert.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "aimeio/hid.h"
#include "aimeio/window.h"
#include "aimeio/aimeio.h"

#include "util/crc.h"
#include "util/dprintf.h"

static unsigned int __stdcall cardio_thread_proc(void *ctx);

struct aime_io_config {
    wchar_t aime_path[MAX_PATH];
    wchar_t felica_path[MAX_PATH];
    bool felica_gen;
    bool use_cardio;
    uint8_t vk_scan;
};

static struct aime_io_config aime_io_cfg;
static uint8_t aime_io_aime_id[10];
static uint8_t aime_io_felica_id[8];
static bool aime_io_aime_id_present;
static bool aime_io_felica_id_present;
static bool aime_io_cardio_card_present;

static void aime_io_config_read(
        struct aime_io_config *cfg,
        const wchar_t *filename);

static HRESULT aime_io_read_id_file(
        const wchar_t *path,
        uint8_t *bytes,
        size_t nbytes);

static HRESULT aime_io_generate_felica(
        const wchar_t *path,
        uint8_t *bytes,
        size_t nbytes);

static HANDLE message_pump_thread;
BOOL message_pump_ready = FALSE;
HWND hWnd;

struct card_timer_holder {
  size_t index;
  uint8_t ticks;
  uint8_t last_card_type;
};

uint8_t MAX_NUM_OF_READERS = 1;

static unsigned int __stdcall cardio_thread_proc(void *ctx) {
    HINSTANCE hInstance;

    if (!InitWindowClass()) {
        dprintf("AimeIO DLL: Failed to initialize window class\n");
        return -1;
    }

    hInstance = GetModuleHandle(NULL);
    if ((hWnd = CreateTheWindow(hInstance)) == NULL) {
        dprintf("AimeIO DLL: Failed to initialize the background window\n");
        return -1;
    }

    dprintf("AimeIO DLL: Device notification listener ready, thread id = %lu", GetCurrentThreadId());
    message_pump_ready = TRUE;

    if (!MessagePump(hWnd)) {
        dprintf("AimeIO DLL: Message pump error\n");
        return -1;
    }

    return 0;
}


static void aime_io_config_read(
        struct aime_io_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    GetPrivateProfileStringW(
            L"aime",
            L"aimePath",
            L"DEVICE\\aime.txt",
            cfg->aime_path,
            _countof(cfg->aime_path),
            filename);

    GetPrivateProfileStringW(
            L"aime",
            L"felicaPath",
            L"DEVICE\\felica.txt",
            cfg->felica_path,
            _countof(cfg->felica_path),
            filename);

    cfg->felica_gen = GetPrivateProfileIntW(
            L"aime",
            L"felicaGen",
            1,
            filename);

    cfg->vk_scan = GetPrivateProfileIntW(
            L"aime",
            L"scan",
            VK_RETURN,
            filename);

    cfg->use_cardio = GetPrivateProfileIntW(
            L"aime",
            L"useCardio",
            0,
            filename);
}

static HRESULT aime_io_read_id_file(
        const wchar_t *path,
        uint8_t *bytes,
        size_t nbytes)
{
    HRESULT hr;
    FILE *f;
    size_t i;
    int byte;
    int r;

    f = _wfopen(path, L"r");

    if (f == NULL) {
        return S_FALSE;
    }

    memset(bytes, 0, nbytes);

    for (i = 0 ; i < nbytes ; i++) {
        r = fscanf(f, "%02x ", &byte);

        if (r != 1) {
            hr = E_FAIL;
            dprintf("AimeIO DLL: %S: fscanf[%i] failed: %i\n",
                    path,
                    (int) i,
                    r);

            goto end;
        }

        bytes[i] = byte;
    }

    hr = S_OK;

end:
    if (f != NULL) {
        fclose(f);
    }

    return hr;
}

static HRESULT aime_io_generate_felica(
        const wchar_t *path,
        uint8_t *bytes,
        size_t nbytes)
{
    size_t i;
    FILE *f;

    assert(path != NULL);
    assert(bytes != NULL);
    assert(nbytes > 0);

    srand(time(NULL));

    for (i = 0 ; i < nbytes ; i++) {
        bytes[i] = rand();
    }

    /* FeliCa IDm values should have a 0 in their high nibble. I think. */
    bytes[0] &= 0x0F;

    f = _wfopen(path, L"w");

    if (f == NULL) {
        dprintf("AimeIO DLL: %S: fopen failed: %i\n", path, (int) errno);

        return E_FAIL;
    }

    for (i = 0 ; i < nbytes ; i++) {
        fprintf(f, "%02X", bytes[i]);
    }

    fprintf(f, "\n");
    fclose(f);

    dprintf("AimeIO DLL: Generated random FeliCa ID\n");

    return S_OK;
}

HRESULT aime_io_init(void)
{
    aime_io_config_read(&aime_io_cfg, L".\\segatools.ini");

    /* Initialize the cardio reader if configured */
    if (aime_io_cfg.use_cardio != 0) {
        dprintf("AimeIO DLL: Initializing HID card reader\n");

        hid_init();

        if (!hid_scan()) {
            dprintf("AimeIO DLL: Failed to initialize HID card reader\n");
            return S_FALSE;
        }

        if (message_pump_thread != NULL) {
            return S_OK;
        }

        message_pump_thread = (HANDLE) _beginthreadex(
                NULL,
                0,
                cardio_thread_proc,
                NULL,
                0,
                NULL);

        while (message_pump_ready == FALSE) {
            Sleep(25);
        }
    }

    return S_OK;
}

void aime_io_fini(void)
{
    EndTheWindow(hWnd);

    dprintf("AimeIO DLL: Device notification thread shutting down\n");
    WaitForSingleObject(message_pump_thread, INFINITE);
    CloseHandle(message_pump_thread);
    message_pump_thread = NULL;

    hid_close();

}

HRESULT aime_io_nfc_poll(uint8_t unit_no)
{
    bool sense;
    HRESULT hr;

    if (unit_no != 0) {
        return S_OK;
    }

    /* Reset presence flags */
    aime_io_aime_id_present = false;
    aime_io_felica_id_present = false;
    aime_io_cardio_card_present = false;

    /* Check the card files only if we're not set to use a cardIO */
    if (aime_io_cfg.use_cardio == 0) {
        /* Don't do anything more if the scan key is not held */
        sense = GetAsyncKeyState(aime_io_cfg.vk_scan) & 0x8000;

        if (!sense) {
            return S_OK;
        }

        /* Try AiMe IC */

        hr = aime_io_read_id_file(
                aime_io_cfg.aime_path,
                aime_io_aime_id,
                sizeof(aime_io_aime_id));

        if (SUCCEEDED(hr) && hr != S_FALSE) {
            aime_io_aime_id_present = true;

            return S_OK;
        }

        /* Try FeliCa IC */

        hr = aime_io_read_id_file(
                aime_io_cfg.felica_path,
                aime_io_felica_id,
                sizeof(aime_io_felica_id));

        if (SUCCEEDED(hr) && hr != S_FALSE) {
            aime_io_felica_id_present = true;

            return S_OK;
        }

        /* Try generating FeliCa IC (if enabled) */

        if (aime_io_cfg.felica_gen) {
            hr = aime_io_generate_felica(
                    aime_io_cfg.felica_path,
                    aime_io_felica_id,
                    sizeof(aime_io_felica_id));

            if (FAILED(hr)) {
                return hr;
            }

            aime_io_felica_id_present = true;
        }

        return S_OK;
    } else {
        /* Check the cardIO */
        uint8_t result = 0;
        size_t i;
        struct aimeio_hid_device *ctx;

        if (unit_no + 1 > MAX_NUM_OF_READERS) {
            MAX_NUM_OF_READERS = unit_no + 1;

            dprintf("AimeIO DLL: Max number of game readers found is now %u", MAX_NUM_OF_READERS);
        }

        EnterCriticalSection(&HID_LOCK);

        for (i = unit_no; i < CONTEXTS_LENGTH; i += MAX_NUM_OF_READERS) {
            ctx = &CONTEXTS[i];

            if (ctx->initialized) {
                switch (hid_device_poll(ctx)) {
                    case HID_POLL_ERROR:
                        dprintf("AimeIO DLL: Error polling cardio device\n");
                        result = 0;
                        break;

                    case HID_POLL_CARD_NOT_READY:
                        result = 0;
                        break;

                    case HID_POLL_CARD_READY:
                        result = 3;
                        aime_io_cardio_card_present = true;
                        break;
                }
            }

            if (result != 0) {
                break;
            }
        }

        LeaveCriticalSection(&HID_LOCK);

        return S_OK;
    }
}

void cardio_read_card(uint8_t unit_no, uint8_t *card_id, uint8_t nbytes) {
    struct aimeio_hid_device *ctx;
    size_t i;

    EnterCriticalSection(&HID_LOCK);

    // The idea here is to map excess HID readers to the modulo of the maximum number of readers
    // the game uses, which in most cases is one.
    for (i = unit_no; i < CONTEXTS_LENGTH; i += MAX_NUM_OF_READERS) {
        ctx = &CONTEXTS[i];

        // Skip uninitialized contexts
        if (!ctx->initialized) {
            continue;
        }

        uint8_t card_type = hid_device_read(ctx);

        if (nbytes > sizeof(ctx->usage_value)) {
            dprintf("AimeIO DLL: nbytes > buffer_size, not inserting card\n");
            card_type = HID_CARD_NONE;
        }

        switch (card_type) {
            case HID_CARD_NONE:
                break;

            case HID_CARD_ISO_15693:
                dprintf("AimeIO DLL: Found: EAM_IO_CARD_ISO15696\n");
                aime_io_aime_id_present = true;
                break;

            case HID_CARD_ISO_18092:
                dprintf("AimeIO DLL: Found: EAM_IO_CARD_FELICA\n");
                aime_io_felica_id_present = true;
                break;

            default:
                dprintf("AimeIO DLL: Unknown card type found: %u", card_type);
        }

        if (aime_io_aime_id_present || aime_io_felica_id_present) {
            memcpy(card_id, ctx->usage_value, nbytes);
            break;
        }
    }

    LeaveCriticalSection(&HID_LOCK);
}


HRESULT aime_io_nfc_get_aime_id(
        uint8_t unit_no,
        uint8_t *luid,
        size_t luid_size)
{
    assert(luid != NULL);
    assert(luid_size == sizeof(aime_io_aime_id));

    if (unit_no != 0 || (!aime_io_aime_id_present && !aime_io_cardio_card_present)) {
        return S_FALSE;
    }

    /* Read the cardio values if it polled a card */
    if (aime_io_cardio_card_present) {
        cardio_read_card(unit_no, aime_io_aime_id, 10);
    }

    if (aime_io_aime_id_present) {
        memcpy(luid, aime_io_aime_id, luid_size);
    }

    return S_OK;
}

HRESULT aime_io_nfc_get_felica_id(uint8_t unit_no, uint64_t *IDm)
{
    uint64_t val;
    size_t i;

    assert(IDm != NULL);

    if (unit_no != 0 || (!aime_io_felica_id_present && !aime_io_cardio_card_present)) {
        return S_FALSE;
    }

    /* Read the cardio values if it polled a card */
    if (aime_io_cardio_card_present) {
        cardio_read_card(unit_no, aime_io_felica_id, 8);
    }

    if (aime_io_felica_id_present) {
        val = 0;

        for (i = 0 ; i < 8 ; i++) {
            val = (val << 8) | aime_io_felica_id[i];
        }

        *IDm = val;
    }

    return S_OK;
}

void aime_io_led_set_color(uint8_t unit_no, uint8_t r, uint8_t g, uint8_t b)
{}
