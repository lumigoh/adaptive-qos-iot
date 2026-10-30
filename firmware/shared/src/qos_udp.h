/*
 * Minimal IPv4 UDP transport shared by the host sender and the ESP32 app.
 *
 * Uses the BSD sockets API: Winsock on Windows, lwIP sockets on the ESP32 and
 * POSIX sockets on Linux/macOS/WSL. The destination is fixed at open time and
 * incoming datagrams from any other endpoint are reported as -2 so the caller
 * can reject spoofed/mismatched ACKs.
 */

#ifndef QOS_UDP_H
#define QOS_UDP_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    intptr_t handle;     /* socket handle; -1 when closed */
    uint32_t address_be; /* destination IPv4 address, network byte order */
    uint16_t port;       /* destination UDP port, host byte order */
} qos_udp_t;

/* Process-wide start-up/shutdown (Winsock only; no-ops elsewhere). */
int qos_net_init(void);
void qos_net_shutdown(void);

/* Opens a UDP socket and remembers the destination. Returns 0 on success. */
int qos_udp_open(qos_udp_t *udp, const char *ip, uint16_t port);

/* Requests the IPv4 TOS/DSCP byte. Verify the mark on the wire; the return
 * value only says whether the socket accepted the option. */
int qos_udp_set_tos(qos_udp_t *udp, uint8_t tos);

/* Sends one datagram. Returns 0 on success, -1 on error. */
int qos_udp_send(qos_udp_t *udp, const char *data, size_t len);

/*
 * Waits up to `timeout_ms` for one datagram from the configured endpoint.
 * Returns 1 when a payload was read, 0 on timeout, -1 on socket error and
 * -2 when a datagram arrived from a different source (it is discarded).
 */
int qos_udp_receive(qos_udp_t *udp, char *buf, size_t capacity,
                    size_t *out_len, int timeout_ms);

void qos_udp_close(qos_udp_t *udp);

/* Last socket error code for diagnostics (WSA error / errno). */
int qos_udp_last_error(void);

#endif /* QOS_UDP_H */
