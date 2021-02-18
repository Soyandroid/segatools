#include <windows.h>

#include <assert.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "board/led1509306.h"
#include "board/led1509306-cmd.h"
#include "board/led1509306-frame.h"

#include "hook/iobuf.h"
#include "hook/iohook.h"

#include "hooklib/uart.h"

#include "util/dprintf.h"
#include "util/dump.h"

static HRESULT led1509306_handle_irp(struct irp *irp);
static HRESULT led1509306_handle_irp_locked(int board, struct irp *irp);

static HRESULT led1509306_req_dispatch(int board, const struct led1509306_req_any *req);
static HRESULT led1509306_req_reset(int board, const struct led1509306_req_any *req);
static HRESULT led1509306_req_get_board_info(int board);
static HRESULT led1509306_req_get_fw_sum(int board);
static HRESULT led1509306_req_get_protocol_ver(int board);
static HRESULT led1509306_req_get_board_status(int board);
static HRESULT led1509306_req_set_led(int board, const struct led1509306_req_any *req);
static HRESULT led1509306_req_set_disable_response(int board, const struct led1509306_req_any *req);
static HRESULT led1509306_req_set_timeout(int board, const struct led1509306_req_any *req);

static char led1509306_board_num[8];
static char led1509306_chip_num[5];
static uint8_t led1509306_fw_ver;
static uint16_t led1509306_fw_sum;
static uint8_t led1509306_board_adr = 2;
static uint8_t led1509306_host_adr = 1;

#define led1509306_nboards 2

typedef struct {
    CRITICAL_SECTION lock;
    struct uart boarduart;
    uint8_t written_bytes[520];
    uint8_t readable_bytes[520];
    bool enable_response;
} _led1509306_per_board_vars;

_led1509306_per_board_vars led1509306_per_board_vars[led1509306_nboards];

HRESULT led1509306_hook_init(const struct led1509306_config *cfg, io_ledstrip_init _led_init, 
    io_ledstrip_set_leds _set_leds, int first_port, int num_boards, uint8_t board_adr, uint8_t host_adr)
{
    assert(cfg != NULL);
    assert(_led_init != NULL);
    assert(_set_leds != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    led_init = _led_init;
    set_leds = _set_leds;
    led1509306_board_adr = board_adr;
    led1509306_host_adr = host_adr;
    
    memcpy(led1509306_board_num, cfg->board_number, sizeof(led1509306_board_num));
    memcpy(led1509306_chip_num, cfg->chip_number, sizeof(led1509306_chip_num));
    led1509306_fw_ver = cfg->fw_ver;
    led1509306_fw_sum = cfg->fw_sum;
    
    for (int i = 0; i < num_boards; i++)
    {
        _led1509306_per_board_vars *v = &led1509306_per_board_vars[i];
        
        InitializeCriticalSection(&v->lock);

        uart_init(&v->boarduart, first_port + i);
        v->boarduart.written.bytes = v->written_bytes;
        v->boarduart.written.nbytes = sizeof(v->written_bytes);
        v->boarduart.readable.bytes = v->readable_bytes;
        v->boarduart.readable.nbytes = sizeof(v->readable_bytes);
        
        v->enable_response = true;
    }

    return iohook_push_handler(led1509306_handle_irp);
}

static HRESULT led1509306_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);
    
    for (int i = 0; i < led1509306_nboards; i++)
    {
        _led1509306_per_board_vars *v = &led1509306_per_board_vars[i];
        struct uart *boarduart = &v->boarduart;
        
        if (uart_match_irp(boarduart, irp))
        {
            CRITICAL_SECTION lock = v->lock;
            
            EnterCriticalSection(&lock);
            hr = led1509306_handle_irp_locked(i, irp);
            LeaveCriticalSection(&lock);

            return hr;
        }
    }
    
    return iohook_invoke_next(irp);
}

static HRESULT led1509306_handle_irp_locked(int board, struct irp *irp)
{
    struct led1509306_req_any req;
    struct iobuf req_iobuf;
    HRESULT hr;

    if (irp->op == IRP_OP_OPEN) {
        dprintf("LED 1509306: Starting backend DLL\n");
        hr = led_init(board);

        if (FAILED(hr)) {
            dprintf("LED 1509306: Backend DLL error: %x\n", (int) hr);

            return hr;
        }
    }
    
    struct uart *boarduart = &led1509306_per_board_vars[board].boarduart;

    hr = uart_handle_irp(boarduart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

    for (;;) {
#if 0
        dprintf("TX Buffer:\n");
        dump_iobuf(&boarduart->written);
#endif

        req_iobuf.bytes = (byte*)&req;
        req_iobuf.nbytes = sizeof(req.hdr) + sizeof(req.cmd) + sizeof(req.payload);
        req_iobuf.pos = 0;

        hr = led1509306_frame_decode(&req_iobuf, &boarduart->written);

        if (hr != S_OK) {
            if (FAILED(hr)) {
                dprintf("LED 1509306: Deframe error: %x\n", (int) hr);
            }

            return hr;
        }

#if 0
        dprintf("Deframe Buffer:\n");
        dump_iobuf(&req_iobuf);
#endif

        hr = led1509306_req_dispatch(board, &req);

        if (FAILED(hr)) {
            dprintf("LED 1509306: Processing error: %x\n", (int) hr);
        }
    }
}

static HRESULT led1509306_req_dispatch(int board, const struct led1509306_req_any *req)
{
    switch (req->cmd) {
    case LED_15093_06_CMD_RESET:
        return led1509306_req_reset(board, req);

    case LED_15093_06_CMD_BOARD_INFO:
        return led1509306_req_get_board_info(board);
    
    case LED_15093_06_CMD_FW_SUM:
        return led1509306_req_get_fw_sum(board);
    
    case LED_15093_06_CMD_PROTOCOL_VER:
        return led1509306_req_get_protocol_ver(board);

    case LED_15093_06_CMD_BOARD_STATUS:
        return led1509306_req_get_board_status(board);

    case LED_15093_06_CMD_SET_LED:
        return led1509306_req_set_led(board, req);

    case LED_15093_06_CMD_SET_DISABLE_RESPONSE:
        return led1509306_req_set_disable_response(board, req);

    case LED_15093_06_CMD_SET_TIMEOUT:
        return led1509306_req_set_timeout(board, req);

    default:
        dprintf("LED 1509306: Unhandled command %02x\n", req->cmd);

        return S_OK;
    }
}

static HRESULT led1509306_req_reset(int board, const struct led1509306_req_any *req)
{
    dprintf("LED 1509306: Reset (board %u, type %02x)\n", board, req->payload[0]);
    
    if (req->payload[0] != 0xd9)
        dprintf("LED 1509306: Warning -- Unknown reset type %02x\n", req->payload[0]);
    
    led1509306_per_board_vars[board].enable_response = true;

    struct led1509306_resp_any resp;
    
    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_06_FRAME_SYNC;
    resp.hdr.dest_adr = led1509306_host_adr;
    resp.hdr.src_adr = led1509306_board_adr;
    resp.hdr.nbytes = 3;
    
    resp.status = 1;
    resp.cmd = LED_15093_06_CMD_RESET;
    resp.report = 1;

    return led1509306_frame_encode(&led1509306_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led1509306_req_get_board_info(int board)
{
    dprintf("LED 1509306: Get board info (board %u)\n", board);
    
    struct led1509306_resp_board_info resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_06_FRAME_SYNC;
    resp.hdr.dest_adr = led1509306_host_adr;
    resp.hdr.src_adr = led1509306_board_adr;
    resp.hdr.nbytes = sizeof(resp.data) + 3;
    
    resp.status = 1;
    resp.cmd = LED_15093_06_CMD_BOARD_INFO;
    resp.report = 1;

    memcpy(resp.data.board_num, led1509306_board_num, sizeof(resp.data.board_num));
    resp.data._0a = 0x0a;
    memcpy(resp.data.chip_num, led1509306_chip_num, sizeof(resp.data.chip_num));
    resp.data._ff = 0xff;
    resp.data.fw_ver = led1509306_fw_ver;

    return led1509306_frame_encode(&led1509306_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led1509306_req_get_fw_sum(int board)
{
    dprintf("LED 1509306: Get firmware checksum (board %u)\n", board);
    
    struct led1509306_resp_any resp;
    
    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_06_FRAME_SYNC;
    resp.hdr.dest_adr = led1509306_host_adr;
    resp.hdr.src_adr = led1509306_board_adr;
    resp.hdr.nbytes = 2 + 3;
    
    resp.status = 1;
    resp.cmd = LED_15093_06_CMD_FW_SUM;
    resp.report = 1;
    
    resp.data[0] = (led1509306_fw_sum >> 8) & 0xff;
    resp.data[1] = led1509306_fw_sum & 0xff;
    
    return led1509306_frame_encode(&led1509306_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led1509306_req_get_protocol_ver(int board)
{
    dprintf("LED 1509306: Get protocol version (board %u)\n", board);
    
    struct led1509306_resp_any resp;
    
    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_06_FRAME_SYNC;
    resp.hdr.dest_adr = led1509306_host_adr;
    resp.hdr.src_adr = led1509306_board_adr;
    resp.hdr.nbytes = 3 + 3;
    
    resp.status = 1;
    resp.cmd = LED_15093_06_CMD_PROTOCOL_VER;
    resp.report = 1;
    
    resp.data[0] = 1;
    resp.data[1] = 1;
    resp.data[2] = 4;
    
    return led1509306_frame_encode(&led1509306_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led1509306_req_get_board_status(int board)
{
    dprintf("LED 1509306: Get board status (board %u)\n", board);
    
    struct led1509306_resp_any resp;
    
    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_06_FRAME_SYNC;
    resp.hdr.dest_adr = led1509306_host_adr;
    resp.hdr.src_adr = led1509306_board_adr;
    resp.hdr.nbytes = 4 + 3;
    
    resp.status = 1;
    resp.cmd = LED_15093_06_CMD_BOARD_STATUS;
    resp.report = 1;
    
    resp.data[0] = 0;
    resp.data[1] = 0;
    resp.data[2] = 0;
    resp.data[3] = 0;
    
    return led1509306_frame_encode(&led1509306_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led1509306_req_set_led(int board, const struct led1509306_req_any *req)
{
    // dprintf("LED 1509306: Set LED (board %u)\n", board);
    
    set_leds(board, req->payload);

    if (!led1509306_per_board_vars[board].enable_response)
        return S_OK;
    
    struct led1509306_resp_any resp;
    
    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_06_FRAME_SYNC;
    resp.hdr.dest_adr = led1509306_host_adr;
    resp.hdr.src_adr = led1509306_board_adr;
    resp.hdr.nbytes = 3;
    
    resp.status = 1;
    resp.cmd = LED_15093_06_CMD_SET_LED;
    resp.report = 1;
    
    return led1509306_frame_encode(&led1509306_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led1509306_req_set_disable_response(int board, const struct led1509306_req_any *req)
{
    dprintf("LED 1509306: Disable LED responses (board %u)\n", board);
    
    led1509306_per_board_vars[board].enable_response = !req->payload[0];

    struct led1509306_resp_any resp;
    
    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_06_FRAME_SYNC;
    resp.hdr.dest_adr = led1509306_host_adr;
    resp.hdr.src_adr = led1509306_board_adr;
    resp.hdr.nbytes = 1 + 3;
    
    resp.status = 1;
    resp.cmd = LED_15093_06_CMD_SET_DISABLE_RESPONSE;
    resp.report = 1;
    
    resp.data[0] = req->payload[0];
    
    return led1509306_frame_encode(&led1509306_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led1509306_req_set_timeout(int board, const struct led1509306_req_any *req)
{
    dprintf("LED 1509306: Set timeout (board %u)\n", board);
    
    // not actually implemented, but respond correctly anyway
    
    struct led1509306_resp_any resp;
    
    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_06_FRAME_SYNC;
    resp.hdr.dest_adr = led1509306_host_adr;
    resp.hdr.src_adr = led1509306_board_adr;
    resp.hdr.nbytes = 2 + 3;
    
    resp.status = 1;
    resp.cmd = LED_15093_06_CMD_SET_TIMEOUT;
    resp.report = 1;
    
    resp.data[0] = req->payload[0];
    resp.data[1] = req->payload[1];
    
    return led1509306_frame_encode(&led1509306_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}
