#ifndef _TRANSPORT_
#define _TRANSPORT_

#include <stddef.h>
#include <stdint.h>

typedef enum {
    TRANSPORT_OK, 
    TRANSPORT_ERR_SOCKET, 
    TRANSPORT_ERR_CONNECT, 
    TRANSPORT_ERR_TIMEOUT, 
    TRANSPORT_ERR_CLOSED, 
    TRANSPORT_ERR_INVALID_ARGS
} TRANSPORT_ERR;
typedef struct {
    int socket_fd;
} TRANSPORT;

int transport_connect(TRANSPORT *transport, const char *host, uint16_t port);
int transport_send(TRANSPORT *transport, const uint8_t *data, size_t data_size);
int transport_recv_exact(TRANSPORT *transport, uint8_t *buffer, size_t buffer_size);
int transport_close(TRANSPORT *transport);

#endif