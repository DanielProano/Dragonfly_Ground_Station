#include "protocol_codec.h"
#include "protocol.h"
#include "crc.h"
#include <stdint.h>
#include <string.h>

#define TOTAL_HEADER_BYTE_SIZE 5

/*
 *  Encode a provided FRAME struct into bytes in the buffer
 */
int protocol_frame_encode(const FRAME *frame, uint8_t *buffer, size_t buf_size) {
    size_t total_len = TOTAL_HEADER_BYTE_SIZE + frame->payload_len + sizeof(uint16_t);

    if (total_len > buf_size)
        return -1;

    if (buf_size < total_len)
        return -1;

    buffer[0] = frame->start_byte;
    buffer[1] = frame->version;
    buffer[2] = frame->message_id;
    buffer[3] = frame->sequence;
    buffer[4] = frame->payload_len;

    memcpy(&buffer[5], frame->payload, frame->payload_len);

    uint16_t crc = compute_crc16(&buffer[1], 4 + frame->payload_len);

    buffer[5 + frame->payload_len] = (uint8_t) (crc & 0xFF);
    buffer[6 + frame->payload_len] = (uint8_t) (crc >> 8);

    return (int) total_len;
}

int protocol_frame_decode(uint8_t *buffer, size_t buf_size, FRAME *frame) {

    if (buf_size < (TOTAL_HEADER_BYTE_SIZE + sizeof(uint16_t)))
        return -1;

    if (buffer[0] != PROTOCOL_START_BYTE)
        return -1;

    uint8_t payload_len = buffer[4];

    if (payload_len > PAYLOAD_MAX_SIZE)
        return -1;

    int total_size = TOTAL_HEADER_BYTE_SIZE + payload_len + sizeof(uint16_t);

    if (buf_size < total_size)
        return -1;

    uint16_t received_crc = (uint16_t) buffer[5 + payload_len] | (uint16_t) (buffer[6 + payload_len] << 8);

    uint16_t calc_crc = compute_crc16(&buffer[1], 4 + payload_len);

    if (received_crc != calc_crc)
        return -1;

    frame->start_byte   = buffer[0];
    frame->version      = buffer[1];
    frame->message_id   = buffer[2];
    frame->sequence     = buffer[3];
    frame->payload_len  = payload_len;

    memcpy(frame->payload, &buffer[5], payload_len);

    frame->crc = received_crc;

    return (int) total_size;
}
