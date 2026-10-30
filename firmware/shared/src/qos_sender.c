#include "qos_sender.h"

#include <stdio.h>
#include <string.h>

/* Wrap-safe "now is at or after start + ms" for uint32_t millisecond clocks. */
static int elapsed_at_least(uint32_t now, uint32_t since, uint32_t ms)
{
    return (uint32_t)(now - since) >= ms;
}

/* Wrap-safe "deadline reached"; valid while deadlines are < 2^31 ms away. */
static int deadline_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static uint32_t records_needed(const qos_profile_t *profile,
                               uint32_t warmup_ms, uint32_t measure_ms)
{
    uint64_t total = (uint64_t)warmup_ms + (uint64_t)measure_ms;
    uint64_t count = (total + profile->interval_ms - 1u) / profile->interval_ms;

    return (uint32_t)count + 2u;
}

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
                             void *io_ctx)
{
    char probe[QOS_MAX_PAYLOAD_BYTES];
    size_t probe_len = 0;
    qos_status_t status;

    if (sender == NULL || profile == NULL || run_id == NULL || log == NULL ||
        log->records == NULL || now_ms == NULL || timeout_ms == 0u) {
        return QOS_ERR_ARGUMENT;
    }

    /* Reuse the formatter's own profile/run-id validation. */
    status = qos_format_q1(profile, run_id, 1u, probe, sizeof probe, &probe_len);
    if (status != QOS_OK) {
        return status;
    }

    if (log->capacity < records_needed(profile, warmup_ms, measure_ms)) {
        return QOS_ERR_ARGUMENT;
    }

    memset(sender, 0, sizeof *sender);
    sender->profile = *profile;
    strncpy(sender->run_id, run_id, QOS_RUN_ID_MAX_LEN);
    sender->run_id[QOS_RUN_ID_MAX_LEN] = '\0';

    sender->interval_ms = profile->interval_ms;
    sender->warmup_ms = warmup_ms;
    sender->measure_ms = measure_ms;
    sender->drain_ms = drain_ms;
    sender->timeout_ms = timeout_ms;

    sender->now_ms = now_ms;
    sender->send = send;
    sender->io_ctx = io_ctx;
    sender->log = log;
    sender->phase = QOS_PHASE_IDLE;

    return QOS_OK;
}

void qos_sender_start(qos_sender_t *sender, uint32_t start_ms)
{
    if (sender == NULL) {
        return;
    }
    sender->phase = QOS_PHASE_WARMUP;
    sender->phase_start_ms = start_ms;
    sender->next_send_ms = start_ms;
    sender->next_sequence = 1u;
    sender->log->count = 0u;
    memset(&sender->counters, 0, sizeof sender->counters);
}

static void send_one(qos_sender_t *sender, uint32_t now)
{
    char payload[QOS_MAX_PAYLOAD_BYTES];
    size_t len = 0;
    qos_record_t *record;
    uint32_t sequence = sender->next_sequence;

    if (qos_format_q1(&sender->profile, sender->run_id, sequence, payload,
                      sizeof payload, &len) != QOS_OK) {
        sender->counters.send_errors++;
        sender->next_sequence++;
        return;
    }

    record = &sender->log->records[sender->log->count];
    sender->log->count++;

    record->sequence = sequence;
    record->sent_ms = now;
    record->ack_ms = 0u;
    record->rtt_ms = 0u;
    record->ack_status = QOS_ACK_PENDING;
    record->phase = (sender->phase == QOS_PHASE_WARMUP) ? 0u : 1u;

    sender->counters.sent++;
    if (sender->send != NULL &&
        sender->send(sender->io_ctx, payload, len) != 0) {
        sender->counters.send_errors++;
    }
    sender->next_sequence++;
}

static void mark_timeouts(qos_sender_t *sender, uint32_t now)
{
    size_t i;

    for (i = 0u; i < sender->log->count; i++) {
        qos_record_t *record = &sender->log->records[i];

        if (record->ack_status != QOS_ACK_PENDING) {
            continue;
        }
        if (elapsed_at_least(now, record->sent_ms, sender->timeout_ms)) {
            record->ack_status = QOS_ACK_NONE;
            sender->counters.timed_out++;
        }
    }
}

void qos_sender_tick(qos_sender_t *sender)
{
    uint32_t now;

    if (sender == NULL || sender->now_ms == NULL) {
        return;
    }
    if (sender->phase == QOS_PHASE_IDLE || sender->phase == QOS_PHASE_DONE) {
        return;
    }

    now = sender->now_ms(sender->io_ctx);

    for (;;) {
        if (sender->phase == QOS_PHASE_WARMUP &&
            elapsed_at_least(now, sender->phase_start_ms, sender->warmup_ms)) {
            sender->phase = QOS_PHASE_MEASURE;
            sender->phase_start_ms = now;
            continue;
        }
        if (sender->phase == QOS_PHASE_MEASURE &&
            elapsed_at_least(now, sender->phase_start_ms,
                             sender->measure_ms)) {
            sender->phase = QOS_PHASE_DRAIN;
            sender->phase_start_ms = now;
            continue;
        }
        if (sender->phase == QOS_PHASE_DRAIN &&
            elapsed_at_least(now, sender->phase_start_ms, sender->drain_ms)) {
            sender->phase = QOS_PHASE_DONE;
            continue;
        }
        break;
    }

    if (sender->phase == QOS_PHASE_WARMUP ||
        sender->phase == QOS_PHASE_MEASURE) {
        /* Do not burst after a stall; resynchronise the schedule instead. */
        if ((int32_t)(now - sender->next_send_ms) >
            (int32_t)(2u * sender->interval_ms)) {
            sender->next_send_ms = now;
        }
        while ((sender->phase == QOS_PHASE_WARMUP ||
                sender->phase == QOS_PHASE_MEASURE) &&
               deadline_reached(now, sender->next_send_ms) &&
               sender->log->count < sender->log->capacity) {
            send_one(sender, now);
            sender->next_send_ms += sender->interval_ms;
        }
    }

    mark_timeouts(sender, now);

    if (sender->phase == QOS_PHASE_DONE) {
        size_t i;

        /* Anything still pending when the run ends never got an ACK. */
        for (i = 0u; i < sender->log->count; i++) {
            qos_record_t *record = &sender->log->records[i];

            if (record->ack_status == QOS_ACK_PENDING) {
                record->ack_status = QOS_ACK_NONE;
                sender->counters.timed_out++;
            }
        }
    }
}

void qos_sender_on_ack(qos_sender_t *sender, const char *payload, size_t len)
{
    qos_ids_t ids;
    qos_record_t *match = NULL;
    uint32_t now;
    size_t i;

    if (sender == NULL || payload == NULL) {
        return;
    }
    if (qos_parse_a1(payload, len, &ids) != QOS_OK) {
        sender->counters.invalid_acks++;
        return;
    }
    if (strcmp(ids.run_id, sender->run_id) != 0 ||
        ids.traffic_class != sender->profile.traffic_class) {
        sender->counters.invalid_acks++;
        return;
    }

    for (i = sender->log->count; i > 0u; i--) {
        if (sender->log->records[i - 1u].sequence == ids.sequence) {
            match = &sender->log->records[i - 1u];
            break;
        }
    }
    if (match == NULL) {
        sender->counters.unmatched_acks++;
        return;
    }

    switch (match->ack_status) {
    case QOS_ACK_PENDING:
        match->ack_status = QOS_ACK_ON_TIME;
        sender->counters.on_time++;
        break;
    case QOS_ACK_NONE:
        match->ack_status = QOS_ACK_LATE;
        sender->counters.late++;
        break;
    default:
        sender->counters.duplicate_acks++;
        return;
    }

    now = sender->now_ms(sender->io_ctx);
    match->ack_ms = now;
    match->rtt_ms = now - match->sent_ms;
}

qos_phase_t qos_sender_phase(const qos_sender_t *sender)
{
    return sender == NULL ? QOS_PHASE_IDLE : sender->phase;
}

const char *qos_phase_name(qos_phase_t phase)
{
    switch (phase) {
    case QOS_PHASE_WARMUP:
        return "warmup";
    case QOS_PHASE_MEASURE:
        return "measure";
    case QOS_PHASE_DRAIN:
        return "drain";
    case QOS_PHASE_DONE:
        return "done";
    case QOS_PHASE_IDLE:
    default:
        return "idle";
    }
}

const char *qos_ack_status_name(qos_ack_status_t status)
{
    switch (status) {
    case QOS_ACK_ON_TIME:
        return "on_time";
    case QOS_ACK_LATE:
        return "late";
    case QOS_ACK_NONE:
        return "none";
    case QOS_ACK_PENDING:
    default:
        return "pending";
    }
}

int qos_sender_csv_header(char *buf, size_t size)
{
    static const char header[] =
        "run_id,class,seq,phase,send_ms,ack_ms,rtt_ms,ack_status";
    int written;

    if (buf == NULL || size == 0u) {
        return 0;
    }
    written = snprintf(buf, size, "%s", header);
    return written > 0 && (size_t)written < size;
}

int qos_sender_csv_row(const qos_sender_t *sender, const qos_record_t *record,
                       char *buf, size_t size)
{
    const char *phase;
    const char *status;
    int written;

    if (sender == NULL || record == NULL || buf == NULL || size == 0u) {
        return 0;
    }

    phase = record->phase == 0u ? "warmup" : "measure";
    status = qos_ack_status_name((qos_ack_status_t)record->ack_status);

    if (record->ack_status == QOS_ACK_ON_TIME ||
        record->ack_status == QOS_ACK_LATE) {
        written = snprintf(buf, size, "%s,%s,%lu,%s,%lu,%lu,%lu,%s",
                           sender->run_id, sender->profile.class_name,
                           (unsigned long)record->sequence, phase,
                           (unsigned long)record->sent_ms,
                           (unsigned long)record->ack_ms,
                           (unsigned long)record->rtt_ms, status);
    } else {
        written = snprintf(buf, size, "%s,%s,%lu,%s,%lu,,,%s",
                           sender->run_id, sender->profile.class_name,
                           (unsigned long)record->sequence, phase,
                           (unsigned long)record->sent_ms, status);
    }

    return written > 0 && (size_t)written < size;
}
