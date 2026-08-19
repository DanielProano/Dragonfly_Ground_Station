#ifndef _CRC_
#define _CRC_

#include <stddef.h>
#include <stdint.h>

void compute_crc16_table(void);
uint16_t compute_crc16(uint8_t *buffer, size_t buffer_len);

#endif