#include "qos_udp.h"

#include <string.h>

#if defined(ARDUINO) || defined(ESP_PLATFORM)

#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <errno.h>

typedef int qos_fd_t;
#define QOS_BAD_FD (-1)
#define qos_close_fd close
#define QOS_SOCKET_ERROR (-1)

static int qos_socket_error(void)
{
    return errno;
}

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

typedef SOCKET qos_fd_t;
#define QOS_BAD_FD INVALID_SOCKET
#define qos_close_fd closesocket
#define QOS_SOCKET_ERROR SOCKET_ERROR

static int qos_socket_error(void)
{
    return WSAGetLastError();
}

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

typedef int qos_fd_t;
#define QOS_BAD_FD (-1)
#define qos_close_fd close
#define QOS_SOCKET_ERROR (-1)

static int qos_socket_error(void)
{
    return errno;
}

#endif

static int last_error;

int qos_net_init(void)
{
#if defined(_WIN32)
    static int started;
    WSADATA data;

    if (started) {
        return 0;
    }
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        last_error = qos_socket_error();
        return -1;
    }
    started = 1;
#endif
    return 0;
}

void qos_net_shutdown(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

int qos_udp_last_error(void)
{
    return last_error;
}

/* Parses "a.b.c.d" without inet_pton so the same code works on lwIP. */
static int parse_ipv4(const char *text, uint32_t *address_be)
{
    unsigned int octet[4];
    int i;
    const char *p = text;

    for (i = 0; i < 4; i++) {
        unsigned int value = 0;
        int digits = 0;

        while (*p >= '0' && *p <= '9') {
            value = value * 10u + (unsigned int)(*p - '0');
            if (value > 255u || ++digits > 3) {
                return -1;
            }
            p++;
        }
        if (digits == 0) {
            return -1;
        }
        octet[i] = value;
        if (i < 3) {
            if (*p != '.') {
                return -1;
            }
            p++;
        }
    }
    if (*p != '\0') {
        return -1;
    }

    *address_be = htonl((uint32_t)octet[0] << 24 |
                        (uint32_t)octet[1] << 16 |
                        (uint32_t)octet[2] << 8 |
                        (uint32_t)octet[3]);
    return 0;
}

int qos_udp_open(qos_udp_t *udp, const char *ip, uint16_t port)
{
    if (udp == NULL || ip == NULL || port == 0u) {
        return -1;
    }

    udp->handle = (intptr_t)QOS_BAD_FD;
    udp->port = port;

    if (parse_ipv4(ip, &udp->address_be) != 0) {
        last_error = 0;
        return -1;
    }

    udp->handle = (intptr_t)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if ((qos_fd_t)udp->handle == QOS_BAD_FD) {
        last_error = qos_socket_error();
        return -1;
    }

#if defined(_WIN32)
    /*
     * Windows reports ICMP port-unreachable from an earlier sendto() as
     * WSAECONNRESET on the next recvfrom(). A receiver that is not up yet
     * would otherwise kill the run, so disable that behaviour.
     */
    {
        BOOL disable = FALSE;
        DWORD returned = 0;

        (void)WSAIoctl((SOCKET)udp->handle, SIO_UDP_CONNRESET, &disable,
                       (DWORD)sizeof disable, NULL, 0, &returned, NULL, NULL);
    }
#endif

    return 0;
}

int qos_udp_set_tos(qos_udp_t *udp, uint8_t tos)
{
    int value = (int)tos;
    qos_fd_t fd;

    if (udp == NULL) {
        return -1;
    }
    fd = (qos_fd_t)udp->handle;
#ifdef IP_TOS
    if (setsockopt(fd, IPPROTO_IP, IP_TOS, (const char *)&value,
                   (int)sizeof value) != 0) {
        last_error = qos_socket_error();
        return -1;
    }
    return 0;
#else
    last_error = 0;
    (void)fd;
    return -1;
#endif
}

int qos_udp_send(qos_udp_t *udp, const char *data, size_t len)
{
    struct sockaddr_in destination;
    qos_fd_t fd;
    int sent;

    if (udp == NULL || data == NULL || len == 0u) {
        return -1;
    }
    fd = (qos_fd_t)udp->handle;
    if (fd == QOS_BAD_FD) {
        return -1;
    }

    memset(&destination, 0, sizeof destination);
    destination.sin_family = AF_INET;
    destination.sin_port = htons(udp->port);
    destination.sin_addr.s_addr = udp->address_be;

    sent = (int)sendto(fd, data, (int)len, 0,
                       (struct sockaddr *)&destination,
                       (socklen_t)sizeof destination);
    if (sent != (int)len) {
        last_error = qos_socket_error();
        return -1;
    }
    return 0;
}

int qos_udp_receive(qos_udp_t *udp, char *buf, size_t capacity,
                    size_t *out_len, int timeout_ms)
{
    struct sockaddr_in source;
    socklen_t source_len = (socklen_t)sizeof source;
    struct timeval wait;
    fd_set readable;
    qos_fd_t fd;
    int selected;
    int received;

    if (udp == NULL || buf == NULL || out_len == NULL || capacity == 0u ||
        timeout_ms < 0) {
        return -1;
    }
    fd = (qos_fd_t)udp->handle;
    if (fd == QOS_BAD_FD) {
        return -1;
    }

    FD_ZERO(&readable);
    FD_SET(fd, &readable);
    wait.tv_sec = timeout_ms / 1000;
    wait.tv_usec = (timeout_ms % 1000) * 1000;

    selected = select((int)(fd + 1), &readable, NULL, NULL, &wait);
    if (selected == 0) {
        return 0;
    }
    if (selected == QOS_SOCKET_ERROR) {
        last_error = qos_socket_error();
        return -1;
    }

    received = (int)recvfrom(fd, buf, (int)capacity, 0,
                             (struct sockaddr *)&source, &source_len);
    if (received < 0) {
        last_error = qos_socket_error();
        return -1;
    }
    *out_len = (size_t)received;

    if (source.sin_addr.s_addr != udp->address_be ||
        source.sin_port != htons(udp->port)) {
        *out_len = 0u;
        return -2;
    }
    return 1;
}

void qos_udp_close(qos_udp_t *udp)
{
    if (udp == NULL) {
        return;
    }
    if ((qos_fd_t)udp->handle != QOS_BAD_FD) {
        qos_close_fd((qos_fd_t)udp->handle);
        udp->handle = (intptr_t)QOS_BAD_FD;
    }
}
