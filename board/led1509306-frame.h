#pragma once

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include "hook/iobuf.h"

enum {
    LED_15093_06_FRAME_SYNC = 0xE0,
};

struct led1509306_hdr {
    uint8_t sync;
    uint8_t dest_adr;
    uint8_t src_adr;
    uint8_t nbytes;
};

HRESULT led1509306_frame_decode(struct iobuf *dest, struct iobuf *src);

HRESULT led1509306_frame_encode(
        struct iobuf *dest,
        const void *ptr,
        size_t nbytes);
