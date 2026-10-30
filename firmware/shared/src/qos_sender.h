/*
 * Sender lifecycle, sequence tracking, ACK matching, timeout/late handling
 * and sender-log formatting shared by both ESP32 profiles.
 *
 * Time is injected through a callback (`now_ms`) so the logic can be tested on
 * a host with a fake clock. All comparisons are wrap-safe for uint32_t
 * millisecond counters (monotonic `millis()` / `esp_timer` / CLOCK_MONOTONIC).
 *
 * This layer performs no I/O and never retransmits: the application sends one
 * datagram per sequence number and calls qos_sender_on_ack() for received A1
 * messages.
 */

#ifndef QOS_SENDER_H
#define QOS_SENDER_H

#include <stddef.h>
#include <stdint.h>

#include "qos_protocol.h"

/* Thresholds from docs/testing.md; pilots may adjust, then freeze. */
#define QOS_ACK_TIMEOUT_MS 2000u
#define QOS_WARMUP_MS 10000u
#define QOS_MEASURE_MS 60000u
#define QOS_DRAIN_MS 2000u

/*
 * Enough records for 20 msg/s over a 72 s run (Routine = 1440).
 * Critical needs ~360. The caller owns this storage.
 */
#define QOS_MAX_RECORDS 2048u

typedef enum {
    QOS_PHASE_IDLE = 0,
    QOS_PHASE_WARMUP,
    QOS_PHASE_MEASURE,
    QOS_PHASE_DRAIN,
    QOS_PHASE_DONE
} qos_phase_t;

typedef enum {
    QOS_ACK_PENDING = 0,  /* sent, no ACK seen and not yet timed out */
    QOS_ACK_ON_TIME,      /* matched within the ACK timeout          */
    QOS_ACK_LATE,         /* timed out earlier, ACK arrived later    */
    QOS_ACK_NONE          /* no ACK by the end of the run            */
} qos_ack_status_t;

typedef struct {
    uint32_t sequence;
    uint32_t sent_ms;
    uint32_t ack_ms;
    uint32_t rtt_ms;
    uint8_t ack_status;   /* qos_ack_status_t */
    uint8_t phase;        /* 0 = warmup, 1 = measure */
} qos_record_t;

typedef struct {
    size_t capacity;
    size_t count;
    qos_record_t *records;
} qos_record_log_t;

typedef struct {
    uint32_t sent;            /* generation attempts (denominator) */
    uint32_t on_time;
    uint32_t late;
    uint32_t timed_out;
    uint32_t duplicate_acks;
    uint32_t invalid_acks;
    uint32_t unmatched_acks;
    uint32_t send_errors;     /* the transport refused a send */
} qos_counters_t;

typedef struct {
    qos_profile_t profile;
    char run_id[QOS_RUN_ID_MAX_LEN + 1];

    uint32_t interval_ms;
    uint32_t warmup_ms;
    uint32_t measure_ms;
    uint32_t drain_ms;
    uint32_t timeout_ms;

    qos_phase_t phase;
    uint32_t phase_start_ms;
    uint32_t next_send_ms;
    uint32_t next_sequence;

    uint32_t (*now_ms)(void *ctx);
    int (*send)(void *ctx, const char *payload, size_t len);
    void *io_ctx;

    qos_record_log_t *log;
    qos_counters_t counters;
} qos_sender_t;

/*
 * Prepares a sender. Fails with QOS_ERR_ARGUMENT when the run_id/profile is
 * invalid or the record log cannot hold the whole run, so a too-small log
 * cannot silently truncate results.
 *
 * `log` and `log->records` remain owned by the caller. Typical sizes:
 *   Critical: 360 records, Routine: 1440 records.
 */
qos_status_t qos_sender_init(qos_sender_t *sender,
                             const qos_profile_t *profile,
                             const char *run_id,
                             uint32_t warmup_ms,
                             uint32_t measure_ms,
                             uint32_t drain_ms,
                             uint32_t timeout_ms,
                             qos_record_log_t *log,
                             uint32_t (*now_ms)(void *ctx),
                             int (*send)(void *ctx, const char *payload,
                                         size_t len),
                             void *io_ctx);

/* Starts the warm-up phase at `start_ms`. Sequences begin at 1. */
void qos_sender_start(qos_sender_t *sender, uint32_t start_ms);

/*
 * Drives the state machine: advances phases, emits any messages due now and
 * marks silent messages as timed out. Call it often (every few milliseconds).
 * Sends are scheduled at profile.interval_ms; if the caller falls more than
 * two intervals behind, the schedule resynchronises instead of bursting.
 */
void qos_sender_tick(qos_sender_t *sender);

/*
 * Handles one received A1 payload. The caller must already have accepted the
 * datagram only from the expected receiver endpoint.
 * Mismatching run/class/sequence numbers are counted, never applied.
 */
void qos_sender_on_ack(qos_sender_t *sender, const char *payload, size_t len);

qos_phase_t qos_sender_phase(const qos_sender_t *sender);
const char *qos_phase_name(qos_phase_t phase);
const char *qos_ack_status_name(qos_ack_status_t status);

/*
 * Sender CSV per docs/testing.md:
 *   run_id,class,seq,phase,send_ms,ack_ms,rtt_ms,ack_status
 * ack_ms and rtt_ms are empty when no ACK was matched. Returns 1 on success,
 * 0 if `buf` was too small (the output is then truncated and unusable).
 */
int qos_sender_csv_header(char *buf, size_t size);
int qos_sender_csv_row(const qos_sender_t *sender, const qos_record_t *record,
                       char *buf, size_t size);

#endif /* QOS_SENDER_H */
