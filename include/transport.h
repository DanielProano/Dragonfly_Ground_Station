#ifndef _TRANSPORT_
#define _TRANSPORT_

#include <stddef.h>
#include <stdint.h>
typedef struct {
    int socket_fd;
} TRANSPORT;

int connect(TRANSPORT *transport, const char *host, int port);
int send(TRANSPORT *transport, uint8_t *data, size_t data_size);
int recv_exact(TRANSPORT *transport, uint8_t *buffer, size_t buffer_size);
int close(TRANSPORT *transport);

#endif