#pragma once

#include "board/led1509306-frame.h"

enum {
    LED_15093_06_CMD_RESET = 0x10,
    LED_15093_06_CMD_SET_TIMEOUT = 0x11,
    LED_15093_06_CMD_SET_DISABLE_RESPONSE = 0x14,
    LED_15093_06_CMD_SET_LED = 0x82,
    LED_15093_06_CMD_SET_LED_COUNT = 0x86,
    LED_15093_06_CMD_BOARD_INFO = 0xF0,
    LED_15093_06_CMD_BOARD_STATUS = 0xF1,
    LED_15093_06_CMD_FW_SUM = 0xF2,
    LED_15093_06_CMD_PROTOCOL_VER = 0xF3,
    LED_15093_06_CMD_BOOTLOADER = 0xFD,
};

struct led1509306_req_any {
    struct led1509306_hdr hdr;
    uint8_t cmd;
    uint8_t payload[256];
};

struct led1509306_resp_any {
    struct led1509306_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    uint8_t data[32];
};

struct led1509306_resp_board_info {
    struct led1509306_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    struct {
        char board_num[8];
        uint8_t _0a;
        char chip_num[5];
        uint8_t _ff;
        uint8_t fw_ver;
        // may be some more data after this that isn't checked
    } data;
};