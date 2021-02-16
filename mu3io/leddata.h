#pragma once

#include <windows.h>

#include <stdint.h>

#define LED_PACKET_FRAMING 0xE0
#define LED_PACKET_ESCAPE 0xD0
#define LED_NUM_MAX 66
#define LED_BOARDS_TOTAL 2
#define LED_OUTPUT_HEADER_SIZE 2
#define LED_OUTPUT_DATA_SIZE_MAX LED_NUM_MAX * 3 * 2  // max if every byte's escaped
#define LED_OUTPUT_TOTAL_SIZE_MAX LED_OUTPUT_HEADER_SIZE + LED_OUTPUT_DATA_SIZE_MAX

// This struct is used to send data related to the button and cab LEDs
struct _ongeki_led_data_buf_t {
    byte framing; // Sync byte
    uint8_t board; // LED output the data is for (0: cab, 1: control deck)
    byte data[LED_OUTPUT_DATA_SIZE_MAX]; // Buffer for LEDs
    byte data_len; // How many bytes to output from the buffer
};

static byte ongeki_led_board_data_lens[LED_BOARDS_TOTAL] = {9*3, 6*3};