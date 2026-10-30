/*
 * Shared Q1/A1 protocol definitions for the Adaptive QoS ESP32 senders.
 *
 * This file is deliberately free of Arduino/ESP-IDF dependencies so the same
 * code can be compiled and unit-tested on a development host.
 *
 * Source of truth: docs/architecture.md ("UDP message and acknowledgement
 * protocol (v1)") and docs/testing.md ("Fixed traffic settings").
 */

#ifndef QOS_PROTOCOL_H
#define QOS_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define QOS_Q1_PREFIX "Q1"
#define QOS_A1_PREFIX "A1"

#define QOS_RUN_ID_MAX_LEN 24u

/* UDP payload sizes, including ASCII space padding. */
#define QOS_CRITICAL_PAYLOAD_BYTES 128u
#define QOS_ROUTINE_PAYLOAD_BYTES 256u
#define QOS_MAX_PAYLOAD_BYTES QOS_ROUTINE_PAYLOAD_BYTES

typedef enum {
    QOS_CLASS_CRITICAL = 0,
    QOS_CLASS_ROUTINE = 1
} qos_class_t;

/*
 * Per-device traffic configuration. Critical and Routine run the same
 * firmware and protocol code; only the profile values differ.
 */
typedef struct {
    qos_class_t traffic_class;
    const char *class_name;   /* protocol field, e.g. "critical"            */
    size_t payload_bytes;     /* total padded UDP payload length             */
    uint32_t interval_ms;     /* generation period: 200 ms / 50 ms           */
    uint8_t dscp;             /* CS5 = 40 / CS0 = 0 (IPv4 DSCP field)        */
    uint8_t tos;              /* IPv4 TOS byte: 0xA0 / 0x00                  */
} qos_profile_t;

extern const qos_profile_t QOS_PROFILE_CRITICAL;
extern const qos_profile_t QOS_PROFILE_ROUTINE;

typedef enum {
    QOS_OK = 0,
    QOS_ERR_ARGUMENT,          /* NULL pointer or invalid profile            */
    QOS_ERR_RUN_ID,            /* run_id missing or violates the format      */
    QOS_ERR_SEQUENCE,          /* sequence must be a positive integer        */
    QOS_ERR_BUFFER_TOO_SMALL   /* caller buffer cannot hold the payload      */
} qos_status_t;

/* Validates 1-24 characters from [A-Za-z0-9_-] (no other bytes allowed). */
qos_status_t qos_validate_run_id(const char *run_id);

/*
 * Formats a padded Q1 data message:
 *
 *     Q1|<run_id>|<traffic_class>|<sequence>
 *
 * The payload is written without a NUL terminator and padded with ASCII
 * spaces (0x20) to exactly profile->payload_bytes. `buffer` must hold at
 * least `profile->payload_bytes` bytes. On success `*out_len` receives the
 * number of payload bytes to send (always profile->payload_bytes).
 *
 * On failure the buffer contents are unspecified but never exceed
 * `buffer_size` bytes.
 */
qos_status_t qos_format_q1(const qos_profile_t *profile,
                           const char *run_id,
                           uint32_t sequence,
                           char *buffer,
                           size_t buffer_size,
                           size_t *out_len);

#endif /* QOS_PROTOCOL_H */
