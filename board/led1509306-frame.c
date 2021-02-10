#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board/led1509306-frame.h"

#include "hook/iobuf.h"

static void led1509306_frame_sync(struct iobuf *src);
static HRESULT led1509306_frame_accept(const struct iobuf *dest);
static HRESULT led1509306_frame_encode_byte(struct iobuf *dest, uint8_t byte);

/* Frame structure:

   [0] Sync byte (0xE0)
   [1] Destination address
   [2] Source Address
   [3] Length of data/payload
   [4] Data/payload
        For requests (host to board):
            [0] Command
            ... Payload
        For responses (board to host):
            [0] Status
            [1] Command
            [2] Report
            ... Payload
   [n] Checksum: Sum of all prior bytes (excluding sync byte)

   Byte stuffing:

   0xD0 is an escape byte. Un-escape the subsequent byte by adding 1. */

static void led1509306_frame_sync(struct iobuf *src)
{
    size_t i;

    for (i = 0 ; i < src->pos && src->bytes[i] != 0xE0 ; i++);

    src->pos -= i;
    memmove(&src->bytes[0], &src->bytes[i], i);
}

static HRESULT led1509306_frame_accept(const struct iobuf *dest)
{
    uint8_t checksum;
    size_t i;

    if (dest->pos < 3 || dest->pos != dest->bytes[3] + 5) {
        return S_FALSE;
    }

    checksum = 0;

    for (i = 1 ; i < dest->pos - 1 ; i++) {
        checksum += dest->bytes[i];
    }
    
    //dprintf("LED checksum %02x, expected %02x\n", checksum, dest->bytes[dest->pos - 1]);
    
    if (checksum != dest->bytes[dest->pos - 1]) {
        return HRESULT_FROM_WIN32(ERROR_CRC);
    }

    return S_OK;
}

HRESULT led1509306_frame_decode(struct iobuf *dest, struct iobuf *src)
{
    uint8_t byte;
    bool escape;
    size_t i;
    HRESULT hr;

    assert(dest != NULL);
    assert(dest->bytes != NULL || dest->nbytes == 0);
    assert(dest->pos <= dest->nbytes);
    assert(src != NULL);
    assert(src->bytes != NULL || src->nbytes == 0);
    assert(src->pos <= src->nbytes);

    led1509306_frame_sync(src);

    dest->pos = 0;
    escape = false;

    for (i = 0, hr = S_FALSE ; i < src->pos && hr == S_FALSE ; i++) {
        /* Step the FSM to unstuff another byte */

        byte = src->bytes[i];

        if (dest->pos >= dest->nbytes) {
            hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        } else if (i == 0) {
            dest->bytes[dest->pos++] = byte;
        } else if (byte == 0xE0) {
            hr = E_FAIL;
        } else if (byte == 0xD0) {
            if (escape) {
                hr = E_FAIL;
            }

            escape = true;
        } else if (escape) {
            dest->bytes[dest->pos++] = byte + 1;
            escape = false;
        } else {
            dest->bytes[dest->pos++] = byte;
        }

        /* Try to accept the packet we've built up so far */

        if (SUCCEEDED(hr)) {
            hr = led1509306_frame_accept(dest);
        }
    }

    /* Handle FSM terminal state */

    if (hr != S_FALSE) {
        /* Frame was either accepted or rejected, remove it from src */
        memmove(&src->bytes[0], &src->bytes[i], src->pos - i);
        src->pos -= i;
    }

    return hr;
}

HRESULT led1509306_frame_encode(
        struct iobuf *dest,
        const void *ptr,
        size_t nbytes)
{
    const uint8_t *src;
    uint8_t checksum;
    uint8_t byte;
    size_t i;
    HRESULT hr;

    assert(dest != NULL);
    assert(dest->bytes != NULL || dest->nbytes == 0);
    assert(dest->pos <= dest->nbytes);
    assert(ptr != NULL);

    src = ptr;

    assert(nbytes >= 3 && src[0] == 0xE0 && src[3] + 4 == nbytes);

    if (dest->pos >= dest->nbytes) {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    dest->bytes[dest->pos++] = 0xE0;
    checksum = 0;
    // dprintf("%02x ", 0xe0);

    for (i = 1 ; i < nbytes ; i++) {
        byte = src[i];
        checksum += byte;
        // dprintf("%02x ", byte);

        hr = led1509306_frame_encode_byte(dest, byte);

        if (FAILED(hr)) {
            return hr;
        }
    }
    // dprintf("%02x \n", checksum);

    return led1509306_frame_encode_byte(dest, checksum);
}

static HRESULT led1509306_frame_encode_byte(struct iobuf *dest, uint8_t byte)
{
    if (byte == 0xE0 || byte == 0xD0) {
        if (dest->pos + 2 > dest->nbytes) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        dest->bytes[dest->pos++] = 0xD0;
        dest->bytes[dest->pos++] = byte - 1;
    } else {
        if (dest->pos + 1 > dest->nbytes) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        dest->bytes[dest->pos++] = byte;
    }

    return S_OK;
}
