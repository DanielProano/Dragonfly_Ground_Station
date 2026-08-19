#include "transport.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TRANSPORT_DEFAULT_TIMEOUT_SEC 3

static int set_timeouts(int fd, int seconds) {
    struct timeval tv = {0};
    tv.tv_sec = seconds;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
        return -1;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
        return -1;

    return 0;
}

int transport_connect(
    TRANSPORT *transport,
    const char *host,
    uint16_t port
) {
    if (transport == NULL || host == NULL)
        return TRANSPORT_ERR_INVALID_ARGS;

    transport->socket_fd = -1;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%u", port);

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result;

    if (getaddrinfo(host, port_str, &hints, &result) != 0)
        return TRANSPORT_ERR_CONNECT;

    int fd = -1;

    for (struct addrinfo *addr = result; addr != NULL; addr = addr->ai_next) {
        fd = socket(
            addr->ai_family,
            addr->ai_socktype,
            addr->ai_protocol
        );

        if (fd < 0)
            continue;

        if (connect(fd, addr->ai_addr, addr->ai_addrlen) == 0)
            break; /* success */

        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);

    if (fd < 0)
        return TRANSPORT_ERR_CONNECT;

    /* Small frames, low latency matters more than throughput. */
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    if (set_timeouts(fd, TRANSPORT_DEFAULT_TIMEOUT_SEC) != 0) {
        close(fd);
        return TRANSPORT_ERR_SOCKET;
    }

    transport->socket_fd = fd;
    return TRANSPORT_OK;
}

int transport_send(
    TRANSPORT *transport,
    const uint8_t *data,
    size_t data_size
) {
    if (transport == NULL || data == NULL)
        return TRANSPORT_ERR_INVALID_ARGS;
    if (transport->socket_fd < 0)
        return TRANSPORT_ERR_SOCKET;

    size_t sent = 0;

    while (sent < data_size) {
        ssize_t n = send(
            transport->socket_fd,
            data + sent,
            data_size - sent,
            0
        );

        if (n > 0) {
            sent += (size_t)n;
            continue;
        }

        if (n == 0)
            return TRANSPORT_ERR_CLOSED; /* not typical for send(), but be safe */

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return TRANSPORT_ERR_TIMEOUT;

        if (errno == EPIPE || errno == ECONNRESET)
            return TRANSPORT_ERR_CLOSED;

        return TRANSPORT_ERR_SOCKET;
    }

    return TRANSPORT_OK;
}

int transport_recv_exact(
    TRANSPORT *transport,
    uint8_t *buffer,
    size_t buffer_size
) {
    if (transport == NULL || buffer == NULL)
        return TRANSPORT_ERR_INVALID_ARGS;
    if (transport->socket_fd < 0)
        return TRANSPORT_ERR_SOCKET;

    size_t received = 0;

    while (received < buffer_size) {
        ssize_t n = recv(
            transport->socket_fd,
            buffer + received,
            buffer_size - received,
            0
        );

        if (n > 0) {
            received += (size_t)n;
            continue;
        }

        if (n == 0)
            return TRANSPORT_ERR_CLOSED; /* orderly shutdown by peer */

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return TRANSPORT_ERR_TIMEOUT;

        if (errno == ECONNRESET)
            return TRANSPORT_ERR_CLOSED;

        return TRANSPORT_ERR_SOCKET;
    }

    return TRANSPORT_OK;
}

int transport_close(TRANSPORT *transport) {
    if (transport == NULL)
        return TRANSPORT_ERR_INVALID_ARGS;

    if (transport->socket_fd >= 0) {
        close(transport->socket_fd);
        transport->socket_fd = -1;
    }

    return TRANSPORT_OK;
}