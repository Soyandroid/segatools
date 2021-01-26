/*
 * Copyright 2018 Matt Bilker <me@mbilker.us>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HID_H
#define HID_H

#include <windows.h>
#include <stdint.h>
#include <hidsdi.h>

extern CRITICAL_SECTION HID_LOCK;
extern struct aimeio_hid_device *CONTEXTS;
extern size_t CONTEXTS_LENGTH;

struct aimeio_hid_device {
  LPWSTR dev_path;
  HANDLE dev_handle;
  OVERLAPPED read_state;
  BOOL initialized;
  BOOL io_pending;

  BYTE report_buffer[128];
  unsigned char usage_value[128];
  DWORD read_size;

  PHIDP_PREPARSED_DATA pp_data;
  HIDP_CAPS caps;
  PHIDP_VALUE_CAPS collection;
  USHORT collection_length;
};

typedef enum hid_poll_value {
  HID_POLL_ERROR = 0,
  HID_POLL_CARD_NOT_READY = 1,
  HID_POLL_CARD_READY = 2,
} hid_poll_value_t;

typedef enum hid_card_type {
  HID_CARD_NONE = 0,
  HID_CARD_ISO_15693 = 0x41,
  HID_CARD_ISO_18092 = 0x42,
} hid_card_type_t;

BOOL hid_init();
void hid_close();
BOOL hid_add_device(LPCWSTR device_path);
BOOL hid_remove_device(LPCWSTR device_path);
BOOL hid_scan_device(struct aimeio_hid_device *ctx, LPCWSTR device_path);
BOOL hid_scan();
hid_poll_value_t hid_device_poll(struct aimeio_hid_device *ctx);
uint8_t hid_device_read(struct aimeio_hid_device *ctx);

#ifdef HID_DEBUG
void hid_print_contexts();
#endif

#endif
