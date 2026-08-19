#ifndef _PROTOCOL_CODEC_
#define _PROTOCOL_CODEC_

#include <stdbool.h>
#include <stdio.h>
#include "protocol.h"

int protocol_frame_encode(const FRAME *frame, uint8_t *buffer, size_t buf_size);
int protocol_frame_decode(uint8_t *buffer, size_t buf_size, FRAME *frame);

#endif