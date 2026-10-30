/*
 * Host tests for A1 parsing, sender lifecycle, ACK matching, timeout/late
 * handling and CSV output.
 *
 * The clock and the transmit function are fake, so these tests need no
 * network and no ESP32. They prove protocol/lifecycle logic only.
 */

#include <stdio.h>
#include <string.h>

#include "qos_protocol.h"
#include "qos_sender.h"

static int failures;
static int checks;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            failures++;                                                      \
        }                                                                    \
    } while (0)

#define CHECK_STATUS(expected, actual)                                       \
    do {                                                                     \
        qos_status_t s_ = (actual);                                          \
        checks++;                                                            \
        if (s_ != (expected)) {                                              \
            printf("FAIL %s:%d: expected status %d, got %d\n", __FILE__,     \
                   __LINE__, (int)(expected), (int)s_);                      \
            failures++;                                                      \
        }                                                                    \
    } while (0)

typedef struct {
    uint32_t now;
    unsigned sends;
    char last_payload[QOS_MAX_PAYLOAD_BYTES];
    size_t last_len;
} fake_io_t;

static uint32_t fake_now(void *ctx)
{
    return ((fake_io_t *)ctx)->now;
}

static int fake_send(void *ctx, const char *payload, size_t len)
{
    fake_io_t *io = (fake_io_t *)ctx;

    io->sends++;
    if (len <= sizeof io->last_payload) {
        memcpy(io->last_payload, payload, len);
        io->last_len = len;
    } else {
        io->last_len = 0u;
    }
    return 0;
}

static qos_record_t records[QOS_MAX_RECORDS];

static void setup(qos_sender_t *sender, fake_io_t *io, qos_record_log_t *log,
                  const qos_profile_t *profile, size_t capacity,
                  uint32_t warmup_ms, uint32_t measure_ms, uint32_t drain_ms,
                  uint32_t timeout_ms)
{
    qos_status_t status;

    memset(io, 0, sizeof *io);
    log->capacity = capacity;
    log->count = 0u;
    log->records = records;

    status = qos_sender_init(sender, profile, "b01_fifo", warmup_ms,
                             measure_ms, drain_ms, timeout_ms, log, fake_now,
                             fake_send, io);
    CHECK_STATUS(QOS_OK, status);
    qos_sender_start(sender, io->now);
}

static void tick_to(qos_sender_t *sender, fake_io_t *io, uint32_t now)
{
    io->now = now;
    qos_sender_tick(sender);
}

static void send_ack(qos_sender_t *sender, fake_io_t *io, uint32_t now,
                     const char *run_id, const char *class_name,
                     uint32_t sequence)
{
    char ack[64];
    int len;

    io->now = now;
    len = snprintf(ack, sizeof ack, "A1|%s|%s|%lu", run_id, class_name,
                   (unsigned long)sequence);
    qos_sender_on_ack(sender, ack, (size_t)len);
}

static void test_a1_parsing(void)
{
    qos_ids_t ids;

    CHECK_STATUS(QOS_OK, qos_parse_a1("A1|r001|critical|42", 19u, &ids));
    CHECK(strcmp(ids.run_id, "r001") == 0);
    CHECK(ids.traffic_class == QOS_CLASS_CRITICAL);
    CHECK(ids.sequence == 42u);

    CHECK_STATUS(QOS_OK,
                 qos_parse_a1("A1|b01_fifo|routine|1200", 24u, &ids));
    CHECK(strcmp(ids.run_id, "b01_fifo") == 0);
    CHECK(ids.traffic_class == QOS_CLASS_ROUTINE);
    CHECK(ids.sequence == 1200u);

    /* Maximum sequence value and a 24-character run id. */
    {
        static const char max_ack[] =
            "A1|abcdefghijklmnopqrstuvwx|critical|4294967295";

        CHECK_STATUS(QOS_OK,
                     qos_parse_a1(max_ack, sizeof max_ack - 1u, &ids));
        CHECK(ids.sequence == 4294967295u);
    }

    /* Unpadded only: no trailing spaces, CR, LF or NUL. */
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|critical|42 ", 20u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|critical|42\n", 20u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|critical|42\r", 20u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|critical|42\0", 20u, &ids));

    /* Wrong prefix, missing fields, empty/bad fields. */
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("Q1|r001|critical|42", 19u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|critical", 16u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT, qos_parse_a1("A1||critical|1", 14u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|bad id|critical|1", 20u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|abcdefghijklmnopqrstuvwxy|critical|1", 34u,
                              &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|urgent|1", 16u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|critical|0", 18u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|critical|-1", 19u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|critical|4294967296", 27u, &ids));
    CHECK_STATUS(QOS_ERR_FORMAT,
                 qos_parse_a1("A1|r001|critical|1x", 19u, &ids));
    CHECK_STATUS(QOS_ERR_ARGUMENT, qos_parse_a1(NULL, 19u, &ids));
}

static void test_init_validation(void)
{
    qos_sender_t sender;
    qos_record_log_t log;
    fake_io_t io;

    memset(&io, 0, sizeof io);
    log.capacity = 10u;
    log.count = 0u;
    log.records = records;

    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_sender_init(&sender, &QOS_PROFILE_CRITICAL, "b01_fifo",
                                 QOS_WARMUP_MS, QOS_MEASURE_MS, QOS_DRAIN_MS,
                                 QOS_ACK_TIMEOUT_MS, &log, fake_now,
                                 fake_send, &io));

    log.capacity = QOS_MAX_RECORDS;
    CHECK_STATUS(QOS_ERR_RUN_ID,
                 qos_sender_init(&sender, &QOS_PROFILE_CRITICAL, "bad id",
                                 0u, 1000u, 0u, 100u, &log, fake_now,
                                 fake_send, &io));
    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_sender_init(&sender, &QOS_PROFILE_CRITICAL, "b01_fifo",
                                 0u, 1000u, 0u, 0u, &log, fake_now, fake_send,
                                 &io));
    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_sender_init(&sender, &QOS_PROFILE_CRITICAL, "b01_fifo",
                                 0u, 1000u, 0u, 100u, &log, NULL, fake_send,
                                 &io));
}

static void test_schedule_and_phases(void)
{
    qos_sender_t sender;
    qos_record_log_t log;
    fake_io_t io;
    uint32_t t;
    size_t i;

    setup(&sender, &io, &log, &QOS_PROFILE_CRITICAL, 64u, 1000u, 1000u, 0u,
          2000u);

    for (t = 0u; t <= 1800u; t += 200u) {
        tick_to(&sender, &io, t);
    }

    CHECK(sender.counters.sent == 10u);
    CHECK(log.count == 10u);
    CHECK(io.sends == 10u);
    CHECK(qos_sender_phase(&sender) == QOS_PHASE_MEASURE);

    /* One send per sequence, strictly increasing from 1 at 200 ms spacing. */
    for (i = 0u; i < log.count; i++) {
        CHECK(log.records[i].sequence == (uint32_t)(i + 1u));
        CHECK(log.records[i].sent_ms == (uint32_t)(i * 200u));
        CHECK(log.records[i].ack_status == QOS_ACK_PENDING);
        CHECK(log.records[i].phase == (i < 5u ? 0u : 1u));
    }

    /* 199 ms later: nothing extra. */
    tick_to(&sender, &io, 1999u);
    CHECK(io.sends == 10u);
    CHECK(qos_sender_phase(&sender) == QOS_PHASE_MEASURE);

    tick_to(&sender, &io, 2000u);
    CHECK(qos_sender_phase(&sender) == QOS_PHASE_DONE);
    CHECK(io.sends == 10u);
}

static void test_full_spec_run_emits_350_messages(void)
{
    qos_sender_t sender;
    qos_record_log_t log;
    fake_io_t io;
    uint32_t t;
    size_t measure_count = 0u;
    size_t i;

    setup(&sender, &io, &log, &QOS_PROFILE_CRITICAL, 512u, QOS_WARMUP_MS,
          QOS_MEASURE_MS, QOS_DRAIN_MS, QOS_ACK_TIMEOUT_MS);

    for (t = 0u; t <= 72000u; t += 200u) {
        tick_to(&sender, &io, t);
    }

    CHECK(qos_sender_phase(&sender) == QOS_PHASE_DONE);
    CHECK(sender.counters.sent == 350u);
    CHECK(log.count == 350u);
    CHECK(io.sends == 350u);

    CHECK(log.records[0].sequence == 1u);
    CHECK(log.records[0].phase == 0u);              /* warm-up   */
    CHECK(log.records[49].sent_ms == 9800u);
    CHECK(log.records[50].sequence == 51u);
    CHECK(log.records[50].phase == 1u);             /* measure   */
    CHECK(log.records[50].sent_ms == 10000u);
    CHECK(log.records[349].sequence == 350u);
    CHECK(log.records[349].sent_ms == 69800u);
    CHECK(log.records[349].phase == 1u);

    for (i = 0u; i < log.count; i++) {
        if (log.records[i].phase == 1u) {
            measure_count++;
        }
    }
    CHECK(measure_count == 300u);

    /* Every message timed out because no ACKs were delivered. */
    CHECK(sender.counters.timed_out == 350u);
    CHECK(sender.counters.on_time == 0u);

    /* No sends after completion. */
    tick_to(&sender, &io, 80000u);
    CHECK(io.sends == 350u);
}

static void test_ack_matching_and_rtt(void)
{
    qos_sender_t sender;
    qos_record_log_t log;
    fake_io_t io;

    setup(&sender, &io, &log, &QOS_PROFILE_CRITICAL, 64u, 0u, 1000u, 0u,
          2000u);
    tick_to(&sender, &io, 0u);
    CHECK(log.count == 1u);
    CHECK(log.records[0].sequence == 1u);

    send_ack(&sender, &io, 42u, "b01_fifo", "critical", 1u);
    CHECK(log.records[0].ack_status == QOS_ACK_ON_TIME);
    CHECK(log.records[0].ack_ms == 42u);
    CHECK(log.records[0].rtt_ms == 42u);
    CHECK(sender.counters.on_time == 1u);
    CHECK(sender.counters.timed_out == 0u);

    /* Duplicate ACK: counted, record unchanged. */
    send_ack(&sender, &io, 60u, "b01_fifo", "critical", 1u);
    CHECK(sender.counters.duplicate_acks == 1u);
    CHECK(log.records[0].ack_ms == 42u);
    CHECK(log.records[0].rtt_ms == 42u);

    /* Unrelated/invalid ACKs never touch records. */
    send_ack(&sender, &io, 70u, "b02_fifo", "critical", 1u);
    send_ack(&sender, &io, 70u, "b01_fifo", "routine", 1u);
    send_ack(&sender, &io, 70u, "b01_fifo", "critical", 99u);
    qos_sender_on_ack(&sender, "not an ack", 10u);
    CHECK(sender.counters.invalid_acks == 3u);
    CHECK(sender.counters.unmatched_acks == 1u);
    CHECK(log.records[0].ack_status == QOS_ACK_ON_TIME);
}

static void test_timeout_then_late_ack(void)
{
    qos_sender_t sender;
    qos_record_log_t log;
    fake_io_t io;

    setup(&sender, &io, &log, &QOS_PROFILE_CRITICAL, 64u, 0u, 1000u, 500u,
          2000u);
    tick_to(&sender, &io, 0u);
    CHECK(log.count == 1u);

    tick_to(&sender, &io, 1999u);
    CHECK(log.records[0].ack_status == QOS_ACK_PENDING);
    CHECK(sender.counters.timed_out == 0u);

    tick_to(&sender, &io, 2000u);
    CHECK(log.records[0].ack_status == QOS_ACK_NONE);
    CHECK(sender.counters.timed_out == 1u);

    send_ack(&sender, &io, 2100u, "b01_fifo", "critical", 1u);
    CHECK(log.records[0].ack_status == QOS_ACK_LATE);
    CHECK(log.records[0].rtt_ms == 2100u);
    CHECK(sender.counters.late == 1u);
    CHECK(sender.counters.on_time == 0u);
    CHECK(sender.counters.timed_out == 1u); /* stays visible as a timeout */
}

static void test_late_ack_during_drain(void)
{
    qos_sender_t sender;
    qos_record_log_t log;
    fake_io_t io;

    setup(&sender, &io, &log, &QOS_PROFILE_CRITICAL, 64u, 0u, 200u, 500u,
          100u);
    tick_to(&sender, &io, 0u);
    CHECK(log.count == 1u);

    tick_to(&sender, &io, 200u);
    CHECK(qos_sender_phase(&sender) == QOS_PHASE_DRAIN);
    CHECK(log.records[0].ack_status == QOS_ACK_NONE);

    send_ack(&sender, &io, 250u, "b01_fifo", "critical", 1u);
    CHECK(log.records[0].ack_status == QOS_ACK_LATE);
    CHECK(sender.counters.late == 1u);

    tick_to(&sender, &io, 700u);
    CHECK(qos_sender_phase(&sender) == QOS_PHASE_DONE);
}

static void test_csv_rows(void)
{
    qos_sender_t sender;
    qos_record_log_t log;
    fake_io_t io;
    char line[256];

    setup(&sender, &io, &log, &QOS_PROFILE_CRITICAL, 64u, 0u, 1000u, 0u,
          2000u);

    CHECK(qos_sender_csv_header(line, sizeof line));
    CHECK(strcmp(line,
                 "run_id,class,seq,phase,send_ms,ack_ms,rtt_ms,ack_status") ==
          0);

    tick_to(&sender, &io, 0u);
    send_ack(&sender, &io, 42u, "b01_fifo", "critical", 1u);
    CHECK(qos_sender_csv_row(&sender, &log.records[0], line, sizeof line));
    CHECK(strcmp(line, "b01_fifo,critical,1,measure,0,42,42,on_time") == 0);

    /* Second message times out, then a late ACK arrives during the drain. */
    tick_to(&sender, &io, 200u);
    tick_to(&sender, &io, 2400u);
    send_ack(&sender, &io, 2500u, "b01_fifo", "critical", 2u);
    CHECK(log.records[1].ack_status == QOS_ACK_LATE);
    CHECK(qos_sender_csv_row(&sender, &log.records[1], line, sizeof line));
    CHECK(strcmp(line, "b01_fifo,critical,2,measure,200,2500,2300,late") == 0);

    /* Truncation is reported instead of silently returning partial rows. */
    CHECK(!qos_sender_csv_row(&sender, &log.records[0], line, 8u));

    /* A silent run ends with ack_status "none" and empty ACK/RTT fields. */
    setup(&sender, &io, &log, &QOS_PROFILE_CRITICAL, 64u, 0u, 200u, 0u, 100u);
    tick_to(&sender, &io, 0u);
    tick_to(&sender, &io, 200u);
    CHECK(qos_sender_phase(&sender) == QOS_PHASE_DONE);
    CHECK(qos_sender_csv_row(&sender, &log.records[0], line, sizeof line));
    CHECK(strcmp(line, "b01_fifo,critical,1,measure,0,,,none") == 0);
}

static void test_millisecond_wraparound(void)
{
    qos_sender_t sender;
    qos_record_log_t log;
    fake_io_t io;
    uint32_t start = 0xFFFFFF00u;
    uint32_t t;

    setup(&sender, &io, &log, &QOS_PROFILE_CRITICAL, 64u, 0u, 2000u, 0u,
          2000u);

    qos_sender_start(&sender, start);
    for (t = 0u; t <= 1800u; t += 200u) {
        tick_to(&sender, &io, start + t);
    }
    CHECK(sender.counters.sent == 10u);
    CHECK(log.records[0].sent_ms == start);
    CHECK(log.records[9].sent_ms == start + 1800u);

    send_ack(&sender, &io, start + 100u, "b01_fifo", "critical", 1u);
    CHECK(log.records[0].ack_status == QOS_ACK_ON_TIME);
    CHECK(log.records[0].rtt_ms == 100u);

    /* Timeout also works across the wrap. */
    tick_to(&sender, &io, start + 2000u);
    CHECK(log.records[0].ack_status == QOS_ACK_ON_TIME);
    CHECK(log.records[1].ack_status == QOS_ACK_NONE);
    CHECK(sender.counters.timed_out == 9u); /* the other nine sends */
}

static void test_routine_profile_through_sender(void)
{
    qos_sender_t sender;
    qos_record_log_t log;
    fake_io_t io;

    setup(&sender, &io, &log, &QOS_PROFILE_ROUTINE, 64u, 0u, 1000u, 0u,
          2000u);
    tick_to(&sender, &io, 0u);
    CHECK(io.sends == 1u);
    CHECK(io.last_len == 256u);
    CHECK(strncmp(io.last_payload, "Q1|b01_fifo|routine|1", 21) == 0);
    CHECK(io.last_payload[21] == ' ');
}

int main(void)
{
    test_a1_parsing();
    test_init_validation();
    test_schedule_and_phases();
    test_full_spec_run_emits_350_messages();
    test_ack_matching_and_rtt();
    test_timeout_then_late_ack();
    test_late_ack_during_drain();
    test_csv_rows();
    test_millisecond_wraparound();
    test_routine_profile_through_sender();

    printf("%s: %d checks, %d failure(s)\n",
           failures == 0 ? "PASS" : "FAIL", checks, failures);
    return failures == 0 ? 0 : 1;
}
