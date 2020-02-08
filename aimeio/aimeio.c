#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "aimeio/aimeio.h"

#include "util/crc.h"
#include "util/dprintf.h"

struct aime_io_config {
    wchar_t aime_path[MAX_PATH];
    wchar_t felica_path[MAX_PATH];
    wchar_t nfclist_path[MAX_PATH];
    bool felica_gen;
    uint8_t vk_scan;
};

static struct aime_io_config aime_io_cfg;
static uint8_t aime_io_aime_id[10];
static uint8_t aime_io_felica_id[8];
static bool aime_io_aime_id_present;
static bool aime_io_felica_id_present;

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

    GetPrivateProfileStringW(
            L"aime",
            L"nfclistPath",
            L"",
            cfg->nfclist_path,
            _countof(cfg->nfclist_path),
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

    return S_OK;
}

void aime_io_fini(void)
{
}

static int aime_io_read_nfclist(
        const wchar_t *path,
        uint8_t *bytes,
        size_t nbytes)
{
    SECURITY_ATTRIBUTES sattr;
    HANDLE cpout_w = NULL, cpout_r = NULL;
    PROCESS_INFORMATION procinfo;
    STARTUPINFOW startinfo;
    BOOL success = FALSE;
    DWORD n;
    char buf[65536], *ptr;
    wchar_t cmdline[32];
    int r, i, v;

    sattr.nLength = sizeof(SECURITY_ATTRIBUTES);
    sattr.bInheritHandle = TRUE;
    sattr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&cpout_r, &cpout_w, &sattr, 0)) {
        dprintf("AimeIO DLL: CreatePipe failed\n");
        return -1;
    }
    if (!SetHandleInformation(cpout_r, HANDLE_FLAG_INHERIT, 0)) {
        dprintf("AimeIO DLL: SetHandleInformation failed\n");
        return -1;
    }

    ZeroMemory(&procinfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&startinfo, sizeof(STARTUPINFOW));
    startinfo.cb = sizeof(STARTUPINFO);
    startinfo.hStdError = cpout_w;
    startinfo.hStdOutput = cpout_w;
    startinfo.hStdInput = cpout_r;
    startinfo.dwFlags |= STARTF_USESTDHANDLES;
    wcscpy(cmdline, L"nfc-list -t 2");

    success = CreateProcessW(
        path,          // binary path
        cmdline,       // command line
        NULL,          // process security attributes
        NULL,          // primary thread security attributes
        TRUE,          // handles are inherited
        0,             // creation flags
        NULL,          // use parent's environment
        NULL,          // use parent's current directory
        &startinfo,    // STARTUPINFO pointer
        &procinfo);    // receives PROCESS_INFORMATION

    // If an error occurs, exit the application.
    if (!success) {
        dprintf("AimeIO DLL: CreateProcess %S failed\n", path);
        return -1;
    }

    if (!ReadFile(cpout_r, buf, 65536, &n, NULL)) {
        dprintf("AimeIO DLL: Read failed\n");
        return -1;
    }
    buf[n] = '\0';
    ptr = strstr(buf, "ID (NFCID2): ");
    if (!ptr) {
        return 0;
    }

    // Felica found
    ptr += 13;

    memset(bytes, 0, nbytes);

    for (i = 0 ; i < nbytes ; i++) {
        r = sscanf(ptr, "%02X ", &v);
        if (r != 1) {
            dprintf("AimeIO DLL: %S: sscanf[%i] failed: %i\n",
                    path,
                    (int) i,
                    r);
            return -1;
        }
        bytes[i] = v;
        ptr += 4;
    }

    WaitForSingleObject(procinfo.hProcess, 3000);
    CloseHandle(procinfo.hProcess);
    CloseHandle(procinfo.hThread);
    CloseHandle(cpout_w);
    CloseHandle(cpout_r);
    return 1;
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

    /* Read nfc-list */

    if (aime_io_cfg.nfclist_path[0]) {
        int ret = aime_io_read_nfclist(
                aime_io_cfg.nfclist_path,
                aime_io_felica_id,
                sizeof(aime_io_felica_id));

        if (ret > 0) {
            aime_io_felica_id_present = true;
            return S_OK;
        }
    }

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
}

HRESULT aime_io_nfc_get_aime_id(
        uint8_t unit_no,
        uint8_t *luid,
        size_t luid_size)
{
    assert(luid != NULL);
    assert(luid_size == sizeof(aime_io_aime_id));

    if (unit_no != 0 || !aime_io_aime_id_present) {
        return S_FALSE;
    }

    memcpy(luid, aime_io_aime_id, luid_size);

    return S_OK;
}

HRESULT aime_io_nfc_get_felica_id(uint8_t unit_no, uint64_t *IDm)
{
    uint64_t val;
    size_t i;

    assert(IDm != NULL);

    if (unit_no != 0 || !aime_io_felica_id_present) {
        return S_FALSE;
    }

    val = 0;

    for (i = 0 ; i < 8 ; i++) {
        val = (val << 8) | aime_io_felica_id[i];
    }

    *IDm = val;

    return S_OK;
}

void aime_io_led_set_color(uint8_t unit_no, uint8_t r, uint8_t g, uint8_t b)
{}
