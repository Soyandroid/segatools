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
#include <stdio.h>
#include <strsafe.h>
#include <dbt.h>
#include <hidsdi.h>

#include "aimeio/hid.h"
#include "aimeio/window.h"

BOOL RUN_MESSAGE_PUMP = TRUE;

BOOL RegisterGuid(HWND hWnd, HDEVNOTIFY *hDeviceNotify) {
  DEV_BROADCAST_DEVICEINTERFACE notification_filter;

  memset(&notification_filter, 0, sizeof(DEV_BROADCAST_DEVICEINTERFACE));
  notification_filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
  notification_filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

  HidD_GetHidGuid(&notification_filter.dbcc_classguid);

  *hDeviceNotify = RegisterDeviceNotificationW(hWnd, &notification_filter, DEVICE_NOTIFY_WINDOW_HANDLE);
  if (*hDeviceNotify == NULL) {
    //log_f("Background hotplug window RegisterDeviceNotification failed: %08lx", GetLastError());
    return FALSE;
  }
  //log_f("RegisterDeviceNotification successful");

  return TRUE;
}

INT_PTR WINAPI WinProcCallback(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  static HDEVNOTIFY hDeviceNotify;

  LRESULT lRet = 1;

  switch (message) {
    case WM_CREATE:
      if (!RegisterGuid(hWnd, &hDeviceNotify)) {
        //log_f("Background hotplug RegisterGuid failed: %08lx", GetLastError());
        lRet = 0;
        return lRet;
      }
      //log_f("RegisterGuid successful");
      break;

    case WM_CLOSE:
      if (!UnregisterDeviceNotification(hDeviceNotify)) {
        DWORD error = GetLastError();

        // The handle may be invalid by this point
        if (error != ERROR_INVALID_HANDLE) {
          ////log_f("Background hotplug window UnregisterDeviceNotification failed: %08lx", error);
        }
      //} else {
        //log_f("UnregisterDeviceNotification successful");
      }
      DestroyWindow(hWnd);
      break;

    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    case WM_DEVICECHANGE:
    {
#ifdef DBT_DEBUG
      switch (wParam) {
        case DBT_DEVICEARRIVAL:
          //log_f("Message: DBT_DEVICEARRIVAL");
          break;
        case DBT_DEVICEREMOVECOMPLETE:
          //log_f("Message: DBT_DEVICEREMOVECOMPLETE");
          break;
        case DBT_DEVNODES_CHANGED:
          // Don't care
          break;
        default:
          //log_f("Message: WM_DEVICECHANGE message received, value %Iu unhandled", wParam);
          break;
      }
#endif

      if (lParam && (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE)) {
        PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR) lParam;

        switch (pHdr->dbch_devicetype) {
          case DBT_DEVTYP_DEVICEINTERFACE:
          {
            PDEV_BROADCAST_DEVICEINTERFACE pDevInf = (PDEV_BROADCAST_DEVICEINTERFACE) pHdr;
            //log_f(" -> DBT_DEVTYP_DEVICEINTERFACE => name: %ls", pDevInf->dbcc_name);

            if (wParam == DBT_DEVICEARRIVAL && hid_add_device(pDevInf->dbcc_name)) {
              //log_f("HID reader found: %ls", pDevInf->dbcc_name);
            } else {
              hid_remove_device(pDevInf->dbcc_name);
            }

#ifdef HID_DEBUG
            hid_print_contexts();
#endif

            break;
          }
#ifdef DBT_DEBUG
          case DBT_DEVTYP_HANDLE:
            //log_f(" -> DBT_DEVTYP_HANDLE");
            break;
          case DBT_DEVTYP_OEM:
           // log_f(" -> DBT_DEVTYP_OEM");
            break;
          case DBT_DEVTYP_PORT:
            //log_f(" -> DBT_DEVTYP_PORT");
            break;
          case DBT_DEVTYP_VOLUME:
            //log_f(" -> DBT_DEVTYP_VOLUME");
            break;
#endif
        }
      }
      break;
    }

    default:
      lRet = DefWindowProc(hWnd, message, wParam, lParam);
      break;
  }

  return lRet;
}

BOOL InitWindowClass() {
  WNDCLASSEX wnd_class;

  wnd_class.cbSize = sizeof(WNDCLASSEX);
  wnd_class.style = CS_OWNDC;
  wnd_class.hInstance = GetModuleHandle(0);
  wnd_class.lpfnWndProc = (WNDPROC) WinProcCallback;
  wnd_class.cbClsExtra = 0;
  wnd_class.cbWndExtra = 0;
  wnd_class.hIcon = NULL;
  wnd_class.hbrBackground = NULL;
  wnd_class.hCursor = NULL;
  wnd_class.lpszClassName = WND_CLASS_NAME;
  wnd_class.lpszMenuName = NULL;
  wnd_class.hIconSm = NULL;

  if (!RegisterClassEx(&wnd_class)) {
    //log_f("Background hotplug window RegisterClassEx failed: %08lx", GetLastError());
    return FALSE;
  }
  //log_f("RegisterClassEx successful");

  return TRUE;
}

HWND CreateTheWindow(HINSTANCE hInstance) {
  HWND hWnd = CreateWindowEx(
      0,
      WND_CLASS_NAME,
      TEXT("cardio"),
      WS_DISABLED,
      0, 0,
      CW_USEDEFAULT, CW_USEDEFAULT,
      NULL,
      NULL,
      hInstance,
      NULL);

  if (hWnd == NULL) {
    //log_f("Background hotplug window CreateWindowEx failed: %08lx", GetLastError());
    return NULL;
  }
  //log_f("CreateWindowEx successful");

  return hWnd;
}

BOOL MessagePump(HWND hWnd) {
  MSG msg;
  int ret_val;

  while (RUN_MESSAGE_PUMP && ((ret_val = GetMessage(&msg, hWnd, 0, 0)) != 0)) {
    if (ret_val == -1) {
     // log_f("Background hotplug window GetMessage failed: %08lx", GetLastError());
      return FALSE;
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return TRUE;
}

BOOL EndTheWindow(HWND hWnd) {
  RUN_MESSAGE_PUMP = FALSE;

  return PostMessage(hWnd, WM_CLOSE, 0, 0);
}
