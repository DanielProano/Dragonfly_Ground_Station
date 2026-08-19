#include "crc.h"
#include <stddef.h>
#include <stdint.h>

#define POSSIBLE_BYTE_VALUES 256
#define GENERATOR 0x1021

static uint16_t crc16_table[POSSIBLE_BYTE_VALUES];

void compute_crc16_table(void) {
    for (int dividend = 0; dividend < 256; dividend++) {
        uint16_t cur_byte = (uint16_t) (dividend << 8);

        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((cur_byte & 0x8000) != 0) {
                cur_byte <<= 1;
                cur_byte ^= GENERATOR;
            } else {
                cur_byte <<= 1;
            }
        }

        crc16_table[dividend] = cur_byte;
    }
}

uint16_t compute_crc16(uint8_t *buffer, size_t buffer_len) {
    uint16_t crc = 0;

    for (size_t byte = 0; byte < buffer_len; byte++) {
        uint8_t pos = (uint8_t) ((crc >> 8) ^ buffer[byte]);
        crc = (uint16_t) ((crc << 8) ^ (uint16_t) (crc16_table[pos]));
    }

    return crc;
}
