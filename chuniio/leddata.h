#pragma once

#include <windows.h>

#define LED_PACKET_FRAMING 0xE0
#define LED_PACKET_ESCAPE 0xD0
#define LED_NUM_MAX 66
#define LED_BOARDS_TOTAL 3
#define LED_OUTPUT_HEADER_SIZE 2
#define LED_OUTPUT_DATA_SIZE_MAX LED_NUM_MAX * 3 * 2  // max if every byte's escaped
#define LED_OUTPUT_TOTAL_SIZE_MAX LED_OUTPUT_HEADER_SIZE + LED_OUTPUT_DATA_SIZE_MAX

// This struct is used to send data related to the slider and billboard LEDs
struct _chuni_led_data_buf_t {
    byte framing; // Sync byte
    uint8_t board; // LED output the data is for (0-1: billboard, 2: slider)
    byte data[LED_OUTPUT_DATA_SIZE_MAX]; // Buffer for LEDs
    byte data_len; // How many bytes to output from the buffer
};

static byte chuni_led_board_data_lens[LED_BOARDS_TOTAL] = {53*3, 63*3, 31*3};