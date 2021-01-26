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

#include <windows.h>
#include <devguid.h>
#include <hidclass.h>
#include <hidusage.h>
#include <hidpi.h>
#include "hidsdi.h"
#include <setupapi.h>

#include "aimeio/hid.h"

#ifdef HID_DEBUG
#define DEBUG_LOG(MSG, ...) //log_f("[DEBUG] " MSG, ##__VA_ARGS__)
#else
#define DEBUG_LOG(MSG, ...)
#endif

#define DEFAULT_ALLOCATED_CONTEXTS 2
#define CARD_READER_USAGE_PAGE 0xffca

// GUID_DEVINTERFACE_HID
//GUID class_interface_guid = { 0x4d1e55b2, 0xf16f, 0x11cf, { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };

// GUID_DEVCLASS_HIDCLASS
GUID hidclass_guid = { 0x745a17a0, 0x74d3, 0x11d0, { 0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda } };

CRITICAL_SECTION HID_LOCK;

struct aimeio_hid_device *CONTEXTS = NULL;
size_t CONTEXTS_LENGTH = 0;

void hid_ctx_init(struct aimeio_hid_device *ctx) {
  ctx->dev_path = NULL;
  ctx->dev_handle = INVALID_HANDLE_VALUE;
  ctx->initialized = FALSE;
  ctx->io_pending = FALSE;
  ctx->read_size = 0;
  ctx->pp_data = NULL;
  ctx->collection = NULL;
  ctx->collection_length = 0;

  memset(&ctx->read_state, 0, sizeof(OVERLAPPED));
  memset(&ctx->report_buffer, 0, sizeof(ctx->report_buffer));
  memset(&ctx->usage_value, 0, sizeof(ctx->usage_value));
  memset(&ctx->caps, 0, sizeof(HIDP_CAPS));
}

void hid_ctx_free(struct aimeio_hid_device *ctx) {
  if (ctx->dev_path != NULL) {
    HeapFree(GetProcessHeap(), 0, ctx->dev_path);
    ctx->dev_path = NULL;
  }

  if (ctx->dev_handle != INVALID_HANDLE_VALUE) {
    CancelIo(ctx->dev_handle);
    CloseHandle(ctx->dev_handle);
    ctx->dev_handle = INVALID_HANDLE_VALUE;
  }

  if (ctx->pp_data != NULL) {
    HidD_FreePreparsedData(ctx->pp_data);
    ctx->pp_data = NULL;
  }

  if (ctx->collection != NULL) {
    HeapFree(GetProcessHeap(), 0, ctx->collection);
    ctx->collection = NULL;
  }
}

void hid_ctx_reset(struct aimeio_hid_device *ctx) {
  ctx->initialized = FALSE;
  ctx->io_pending = FALSE;
  ctx->read_size = 0;
  ctx->collection_length = 0;

  hid_ctx_free(ctx);

  memset(&ctx->read_state, 0, sizeof(OVERLAPPED));
  memset(&ctx->report_buffer, 0, sizeof(ctx->report_buffer));
  memset(&ctx->usage_value, 0, sizeof(ctx->usage_value));
  memset(&ctx->caps, 0, sizeof(HIDP_CAPS));
}

BOOL hid_init() {
  size_t i;

  InitializeCriticalSectionAndSpinCount(&HID_LOCK, 0x00000400);

  CONTEXTS_LENGTH = DEFAULT_ALLOCATED_CONTEXTS;
  CONTEXTS = (struct aimeio_hid_device *) HeapAlloc(
    GetProcessHeap(),
    HEAP_ZERO_MEMORY,
    CONTEXTS_LENGTH * sizeof(struct aimeio_hid_device));

  if (CONTEXTS == NULL) {
    //log_f("Failed to allocate memory to hold HID device information: %08lx", GetLastError());
    return FALSE;
  }

  for (i = 0; i < CONTEXTS_LENGTH; i++) {
    hid_ctx_init(&CONTEXTS[i]);
  }

  return TRUE;
}

void hid_close() {
  size_t i;

  if (CONTEXTS_LENGTH > 0) {
    for (i = 0; i < CONTEXTS_LENGTH; i++) {
      hid_ctx_free(&CONTEXTS[i]);
    }

    HeapFree(GetProcessHeap(), 0, CONTEXTS);
    CONTEXTS = NULL;
    CONTEXTS_LENGTH = 0;

    DeleteCriticalSection(&HID_LOCK);
  }
}

void hid_print_caps(struct aimeio_hid_device *hid_ctx) {
#define VPRINT(KEY, VALUE) //log_f("... " #KEY ": %u", VALUE)
  VPRINT(InputReportByteLength,     hid_ctx->caps.InputReportByteLength);
  //VPRINT(OutputReportByteLength,    hid_ctx->caps.OutputReportByteLength);
  //VPRINT(FeatureReportByteLength,   hid_ctx->caps.FeatureReportByteLength);
  //VPRINT(NumberLinkCollectionNodes, hid_ctx->caps.NumberLinkCollectionNodes);
  VPRINT(NumberInputValueCaps,      hid_ctx->caps.NumberInputValueCaps);
  VPRINT(NumberInputDataIndices,    hid_ctx->caps.NumberInputDataIndices);
  //VPRINT(NumberOutputValueCaps,     hid_ctx->caps.NumberOutputValueCaps);
  //VPRINT(NumberOutputDataIndices,   hid_ctx->caps.NumberOutputDataIndices);
  //VPRINT(NumberFeatureValueCaps,    hid_ctx->caps.NumberFeatureValueCaps);
  //VPRINT(NumberFeatureDataIndices,  hid_ctx->caps.NumberFeatureDataIndices);
#undef VPRINT
}

#ifdef HID_DEBUG
void hid_print_contexts() {
  size_t i;
  struct aimeio_hid_device *ctx;

  EnterCriticalSection(&HID_LOCK);

  for (i = 0; i < CONTEXTS_LENGTH; i++) {
    ctx = &CONTEXTS[i];

    DEBUG_LOG("CONTEXTS[%Iu] = 0x%p", i, &CONTEXTS[i]);
    DEBUG_LOG("... initialized = %d", ctx->initialized);

    if (ctx->initialized) {
      DEBUG_LOG("... dev_path = %ls", ctx->dev_path);
      DEBUG_LOG("... dev_handle = 0x%p", ctx->dev_handle);
    }
  }

  LeaveCriticalSection(&HID_LOCK);
}
#endif

BOOL hid_add_device(LPCWSTR device_path) {
  BOOL res = TRUE;
  size_t free_spot, i;

  EnterCriticalSection(&HID_LOCK);

  free_spot = CONTEXTS_LENGTH;

  for (i = 0; i < CONTEXTS_LENGTH; i++) {
    DEBUG_LOG("hid_add_device(\"%ls\") => i: %Iu", device_path, i);

    if (!CONTEXTS[i].initialized) {
      free_spot = i;
      break;
    }
  }

  if (free_spot >= CONTEXTS_LENGTH) {
    CONTEXTS_LENGTH++;
    CONTEXTS = (struct aimeio_hid_device *) HeapReAlloc(
      GetProcessHeap(),
      HEAP_ZERO_MEMORY,
      CONTEXTS,
      CONTEXTS_LENGTH * sizeof(struct aimeio_hid_device));

    if (CONTEXTS == NULL) {
      //log_f("Failed to reallocate memory for HID device information: %08lx: %ls",
      //  GetLastError(),
      //  device_path);

      res = FALSE;
    } else {
      hid_ctx_init(&CONTEXTS[free_spot]);
    }
  }

  // If the memory reallocation fails, do not scan as the memory for the
  // context is not allocated.
  if (res == TRUE) {
    res = hid_scan_device(&CONTEXTS[free_spot], device_path);

    // Reset context if device scan failed
    if (res == FALSE) {
      hid_ctx_reset(&CONTEXTS[free_spot]);
    }
  }

  LeaveCriticalSection(&HID_LOCK);

  return res;
}

BOOL hid_remove_device(LPCWSTR device_path) {
  BOOL res = FALSE;
  size_t i;

  EnterCriticalSection(&HID_LOCK);

  for (i = 0; i < CONTEXTS_LENGTH; i++) {
    // The device paths in `hid_scan` are partially lower-case, so perform a
    // case-insensitive comparison here
    if (CONTEXTS[i].initialized && (wcsicmp(device_path, CONTEXTS[i].dev_path) == 0)) {
      DEBUG_LOG("hid_remove_device(\"%ls\") => i: %Iu", device_path, i);

      hid_ctx_reset(&CONTEXTS[i]);

      res = TRUE;
      break;
    }
  }

  LeaveCriticalSection(&HID_LOCK);

  return res;
}

/*
 * Scan HID device to see if it is a HID reader
 */
BOOL hid_scan_device(struct aimeio_hid_device *ctx, LPCWSTR device_path) {
  size_t dev_path_size;
  NTSTATUS res;

#ifdef HID_DEBUG
  size_t i;
#endif

  dev_path_size = (wcslen(device_path) + 1) * sizeof(WCHAR);
  DEBUG_LOG("hid_scan_device(\"%ls\") => dev_path_size: %Iu", device_path, dev_path_size);

  ctx->dev_path = (LPWSTR) HeapAlloc(GetProcessHeap(), 0, dev_path_size);
  if (ctx->dev_path == NULL) {
    //log_f("Failed to allocate memory for dev node string copy: %08lx: %ls",
    //  GetLastError(),
    //  device_path);

    return FALSE;
  }

  DEBUG_LOG("hid_scan_device(\"%ls\") => ctx->dev_path = %p", device_path, ctx->dev_path);

  memcpy(ctx->dev_path, device_path, dev_path_size);
  ctx->dev_path[dev_path_size - 1] = '\0';

  ////log_f("... DevicePath = %ls", ctx->dev_path);
  ctx->dev_handle = CreateFileW(
    ctx->dev_path,
    GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,
    NULL);

  if (ctx->dev_handle == INVALID_HANDLE_VALUE) {
    //log_f("Initial CreateFileW failed: %08lx: %ls", GetLastError(), ctx->dev_path);

    HeapFree(GetProcessHeap(), 0, ctx->dev_path);
    ctx->dev_path = NULL;
    ctx->dev_handle = INVALID_HANDLE_VALUE;

    return FALSE;
  }

  //DEBUG_LOG("before HidD_GetPreparsedData");
  if (!HidD_GetPreparsedData(ctx->dev_handle, &ctx->pp_data)) {
    //log_f("HidD_GetPreparsedData failed: %08lx: %ls", GetLastError(), ctx->dev_path);
    return FALSE;
  }
  //DEBUG_LOG("after HidD_GetPreparsedData");

  res = HidP_GetCaps(ctx->pp_data, &ctx->caps);
  if (res != HIDP_STATUS_SUCCESS) {
    //log_f("HidP_GetCaps failed: %08lx: %ls", res, ctx->dev_path);
    return FALSE;
  }
  // 0xffca is the card reader usage page ID
  if (ctx->caps.UsagePage != CARD_READER_USAGE_PAGE) {
    //log_f("Skipping HID device with incorrect usage page: %ls", ctx->dev_path);
    return FALSE;
  } else if (ctx->caps.NumberInputValueCaps == 0) {
    //log_f("Skipping HID device with no value capabilities: %ls", ctx->dev_path);
    return FALSE;
  }

  DEBUG_LOG("hid_scan_device(\"%ls\") => collection size: %Iu", ctx->dev_path, ctx->caps.NumberInputValueCaps * sizeof(HIDP_VALUE_CAPS));
  ctx->collection_length = ctx->caps.NumberInputValueCaps;
  ctx->collection = (HIDP_VALUE_CAPS *) HeapAlloc(
    GetProcessHeap(),
    0,
    ctx->collection_length * sizeof(HIDP_VALUE_CAPS));

  if (ctx->collection == NULL) {
    //log_f("Failed to allocate memory for HID Value Capabilities: %08lx: %ls",
    //  GetLastError(),
    //  ctx->dev_path);

    return FALSE;
  }

  res = HidP_GetValueCaps(
    HidP_Input,
    ctx->collection,
    &ctx->collection_length,
    ctx->pp_data);

  if (res != HIDP_STATUS_SUCCESS) {
    //log_f("HidP_GetLinkCollectionNodes failed: %08lx: %ls",
    //  res,
    //  ctx->dev_path);

    return FALSE;
  }

  //log_f("Initializing HID reader on dev node %ls:", ctx->dev_path);
  //log_f("... Device top-level usage: %u", ctx->caps.Usage);
  //log_f("... Device usage page: 0x%04x", ctx->caps.UsagePage);

  hid_print_caps(ctx);


#ifdef HID_DEBUG
  for (i = 0; i < ctx->collection_length; i++) {
    HIDP_VALUE_CAPS *item = &ctx->collection[i];
    //log_f("... collection[%Iu]", i);
    //log_f("...   UsagePage = 0x%04x", item->UsagePage);
    //log_f("...   ReportID = %u", item->ReportID);
    ////log_f("...   IsAlias = %u", item->IsAlias);
    ////log_f("...   LinkUsage = %u", item->LinkUsage);
    ////log_f("...   IsRange = %u", item->IsRange);
    ////log_f("...   IsAbsolute = %u", item->IsAbsolute);
    ////log_f("...   BitSize = %u", item->BitSize);
    ////log_f("...   ReportCount = %u", item->ReportCount);

    if (!item->IsRange) {
      ////log_f("...   collection[%Iu].NotRange:", i);
      ////log_f("...     Usage = 0x%x", item->NotRange.Usage);
      ////log_f("...     DataIndex = %u", item->NotRange.DataIndex);
      //log_f("...   Usage = 0x%x", item->NotRange.Usage);
      //log_f("...   DataIndex = %u", item->NotRange.DataIndex);
    }
  }
#endif

  ctx->initialized = TRUE;

  return TRUE;
}

/*
 * Checks all devices registered with the HIDClass GUID. If the usage page of
 * the device is 0xffca, then a compatible card reader was found.
 *
 * Usage 0x41 => ISO_15693
 * Usage 0x42 => ISO_18092 (FeliCa)
 */
BOOL hid_scan() {
  SP_DEVINFO_DATA devinfo_data;
  SP_DEVICE_INTERFACE_DATA device_interface_data;
  SP_DEVICE_INTERFACE_DETAIL_DATA_W *device_interface_detail_data = NULL;
  HDEVINFO device_info_set = (HDEVINFO) INVALID_HANDLE_VALUE;
  GUID hid_guid;
  //DWORD dwPropertyRegDataType;
  //wchar_t szBuffer[1024];
  wchar_t szGuid[64] = { 0 };
  DWORD device_index = 0;
  DWORD dwSize = 0;

  HidD_GetHidGuid(&hid_guid);
  StringFromGUID2(&hid_guid, szGuid, 64);
  ////log_f("HID guid: %ls", szGuid);

  StringFromGUID2(&hidclass_guid, szGuid, 64);
  ////log_f("HIDClass guid: %ls", szGuid);

  // HID collection opening needs `DIGCF_DEVICEINTERFACE` and ignore
  // disconnected devices
  device_info_set = SetupDiGetClassDevs(&hid_guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (device_info_set == INVALID_HANDLE_VALUE) {
    //log_f("SetupDiGetClassDevs failed: %08lx", GetLastError());

    return FALSE;
  }

  memset(&devinfo_data, 0, sizeof(SP_DEVINFO_DATA));
  devinfo_data.cbSize = sizeof(SP_DEVINFO_DATA);
  device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  // `SetupDiEnumDeviceInterfaces` must come before `SetupDiEnumDeviceInfo`
  // else `SetupDiEnumDeviceInterfaces` will fail with error 259
  while (SetupDiEnumDeviceInterfaces(device_info_set, NULL, &hid_guid, device_index, &device_interface_data)) {
    ////log_f("device_index: %lu", device_index);

    // Get the required size
    if (SetupDiGetDeviceInterfaceDetailW(device_info_set, &device_interface_data, NULL, 0, &dwSize, NULL)) {
      //log_f("Initial SetupDiGetDeviceInterfaceDetailW failed: %08lx for device index %lu",
      //  GetLastError(),
      //  device_index);

      continue;
    }

    device_interface_detail_data = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize);
    if (device_interface_detail_data == NULL) {
      //log_f("Failed to allocate memory of size %lu for device interface detail data: %08lx for device index %lu",
      //  dwSize,
      //  GetLastError(),
      //  device_index);

      continue;
    }

    device_interface_detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

#define LOOP_CONTINUE() \
  HeapFree(GetProcessHeap(), 0, device_interface_detail_data); \
  device_interface_detail_data = NULL; \
  device_index++; \
  continue

    if (!SetupDiGetDeviceInterfaceDetailW(device_info_set, &device_interface_data, device_interface_detail_data, dwSize, NULL, NULL)) {
      //log_f("Second SetupDiGetDeviceInterfaceDetailW failed: %08lx for device index %lu",
      //  GetLastError(),
      //  device_index);

      LOOP_CONTINUE();
    }

    if (!SetupDiEnumDeviceInfo(device_info_set, device_index, &devinfo_data)) {
      //log_f("SetupDiEnumDeviceInfo failed: %08lx: %ls",
       // GetLastError(),
      //  device_interface_detail_data->DevicePath);

      LOOP_CONTINUE();
    }

    if (!IsEqualGUID(&hidclass_guid, &devinfo_data.ClassGuid)) {
      //log_f("Skipping HID device with incorrect class GUID: %ls", device_interface_detail_data->DevicePath);
      LOOP_CONTINUE();
    }

    //StringFromGUID2(&devinfo_data.ClassGuid, szGuid, 64);
    ////log_f("... Class GUID: %ls", szGuid);

    /*
    if (SetupDiGetDeviceRegistryPropertyW(device_info_set, &devinfo_data, SPDRP_CLASS, NULL, (BYTE *) &szBuffer, sizeof(szBuffer), NULL)) {
      //log_f("... Driver Name: %ls", szBuffer);
    }

    if (SetupDiGetDeviceRegistryPropertyW(device_info_set, &devinfo_data, SPDRP_DEVICEDESC, &dwPropertyRegDataType, (BYTE *) &szBuffer, sizeof(szBuffer), &dwSize)) {
      //log_f("... Device Description: %ls", szBuffer);
    }
    */

    hid_add_device(device_interface_detail_data->DevicePath);

    HeapFree(GetProcessHeap(), 0, device_interface_detail_data);
    device_interface_detail_data = NULL;

    device_index++;
  }

#undef LOOP_CONTINUE

  if (device_info_set != INVALID_HANDLE_VALUE) {
    SetupDiDestroyDeviceInfoList(device_info_set);
  }

  return TRUE;
}

hid_poll_value_t hid_device_poll(struct aimeio_hid_device *ctx) {
  DWORD error = 0;

  if (!ctx->initialized) {
    return HID_POLL_ERROR;
  }

  if (ctx->io_pending) {
    // Do this if inside to not have more `ReadFile` overlapped I/O requests.
    // If there are more calls to `ReadFile` than `GetOverlappedResult` then
    // eventually the working set quota will run out triggering error 1426
    // (ERROR_WORKING_SET_QUOTA).
    if (HasOverlappedIoCompleted(&ctx->read_state)) {
      ctx->io_pending = FALSE;

      if (!GetOverlappedResult(ctx->dev_handle, &ctx->read_state, &ctx->read_size, FALSE)) {
        //log_f("Card ID reading with GetOverlappedResult error: %08lx", GetLastError());
        return HID_POLL_ERROR;
      }

      memset(&ctx->read_state, 0, sizeof(OVERLAPPED));

      return HID_POLL_CARD_READY;
    }
  } else {
    if (!ReadFile(
      ctx->dev_handle,
      &ctx->report_buffer,
      sizeof(ctx->report_buffer),
      &ctx->read_size,
      &ctx->read_state))
    {
      error = GetLastError();

      if (error == ERROR_IO_PENDING) {
        ctx->io_pending = TRUE;
      } else {
        //log_f("Card ID reading with ReadFile failed: %08lx", error);
        return HID_POLL_ERROR;
      }
    } else {
      // The read completed right away
      return HID_POLL_CARD_READY;
    }
  }

  return HID_POLL_CARD_NOT_READY;
}

static const char *hid_card_type_name(hid_card_type_t card_type) {
  switch (card_type) {
    case HID_CARD_NONE: return "none";
    case HID_CARD_ISO_15693: return "ISO 15693";
    case HID_CARD_ISO_18092: return "ISO 18092 (FeliCa)";
    default: return "unknown";
  }
}

uint8_t hid_device_read(struct aimeio_hid_device *hid_ctx) {
  NTSTATUS res;

  int i = 0;

  if (!hid_ctx->initialized) {
    return HID_CARD_NONE;
  }

  if (hid_ctx->io_pending) {
    return HID_CARD_NONE;
  }
  if (hid_ctx->read_size == 0) {
    //log_f("Not enough bytes read: %lu", hid_ctx->read_size);
    return HID_CARD_NONE;
  }

  for (i = 0; i < hid_ctx->collection_length; i++) {
    HIDP_VALUE_CAPS *item = &hid_ctx->collection[i];

    res = HidP_GetUsageValueArray(
      HidP_Input,
      CARD_READER_USAGE_PAGE,
      0, // LinkCollection
      item->NotRange.Usage,
      (PCHAR) &hid_ctx->usage_value,
      sizeof(hid_ctx->usage_value),
      hid_ctx->pp_data,
      (PCHAR) &hid_ctx->report_buffer,
      hid_ctx->read_size);

    // Loop through the collection to find the entry that handles
    // this ReportID
    if (res == HIDP_STATUS_INCOMPATIBLE_REPORT_ID) {
      continue;
    }

    if (res != HIDP_STATUS_SUCCESS) {
      //log_f("HidP_GetData error: 0x%08lx", res);
      return HID_CARD_NONE;
    }

    /*log_f("Loaded card ID [%02X%02X%02X%02X%02X%02X%02X%02X] type %s (0x%02x)",
      hid_ctx->usage_value[0],
      hid_ctx->usage_value[1],
      hid_ctx->usage_value[2],
      hid_ctx->usage_value[3],
      hid_ctx->usage_value[4],
      hid_ctx->usage_value[5],
      hid_ctx->usage_value[6],
      hid_ctx->usage_value[7],
      hid_card_type_name(item->NotRange.Usage),
      item->NotRange.Usage); */

    return item->NotRange.Usage;
  }

  DEBUG_LOG("Got report for unknown usage: %02x %02x %02x %02x %02x %02x %02x %02x %02x",
    hid_ctx->report_buffer[0],
    hid_ctx->report_buffer[1],
    hid_ctx->report_buffer[2],
    hid_ctx->report_buffer[3],
    hid_ctx->report_buffer[4],
    hid_ctx->report_buffer[5],
    hid_ctx->report_buffer[6],
    hid_ctx->report_buffer[7],
    hid_ctx->report_buffer[8]);

  return HID_CARD_NONE;
}

