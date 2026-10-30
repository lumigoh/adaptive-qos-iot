/*
 * Host-side tests for the shared Q1 formatter.
 *
 * These are software checks only: they prove the payload bytes produced by
 * qos_format_q1() match docs/architecture.md. They do not exercise Wi-Fi,
 * UDP transmission, DSCP marking, RTT measurement, or any ESP32 hardware.
 *
 * Build and run: see firmware/README.md.
 */

#include <stdio.h>
#include <string.h>

#include "qos_protocol.h"

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

/* Fixed-size payload with canaries, so overruns past the payload are caught. */
typedef struct {
    unsigned char canary_before[8];
    char payload[QOS_MAX_PAYLOAD_BYTES];
    unsigned char canary_after[8];
} guarded_buffer_t;

static void guarded_init(guarded_buffer_t *buf)
{
    memset(buf->canary_before, 0xA5, sizeof buf->canary_before);
    memset(buf->payload, 0xCC, sizeof buf->payload);
    memset(buf->canary_after, 0x5A, sizeof buf->canary_after);
}

static int canaries_intact(const guarded_buffer_t *buf)
{
    size_t i;

    for (i = 0; i < sizeof buf->canary_before; i++) {
        if (buf->canary_before[i] != 0xA5) {
            return 0;
        }
    }
    for (i = 0; i < sizeof buf->canary_after; i++) {
        if (buf->canary_after[i] != 0x5A) {
            return 0;
        }
    }
    return 1;
}

/* Builds the expected payload: core text followed by spaces. */
static size_t build_expected(char *out, size_t payload_bytes,
                             const char *core)
{
    size_t core_len = strlen(core);

    memcpy(out, core, core_len);
    memset(out + core_len, ' ', payload_bytes - core_len);
    return payload_bytes;
}

static int payload_equals(const char *actual, size_t actual_len,
                          const char *core, size_t payload_bytes)
{
    char expected[QOS_MAX_PAYLOAD_BYTES];

    if (actual_len != payload_bytes) {
        return 0;
    }
    build_expected(expected, payload_bytes, core);
    return memcmp(actual, expected, payload_bytes) == 0;
}

static size_t first_non_space_after(const char *payload, size_t from,
                                    size_t len)
{
    size_t i;

    for (i = from; i < len; i++) {
        if (payload[i] != ' ') {
            return i;
        }
    }
    return len;
}

/* ------------------------------------------------------------------------- */

static void test_critical_payload_is_exactly_128_bytes(void)
{
    guarded_buffer_t buf;
    size_t len = 0;
    size_t i;

    guarded_init(&buf);
    CHECK_STATUS(QOS_OK,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "r001", 42,
                               buf.payload, sizeof buf.payload, &len));
    CHECK(len == 128u);
    CHECK(canaries_intact(&buf));
    CHECK(payload_equals(buf.payload, len, "Q1|r001|critical|42", 128u));

    /* Padding is ASCII 0x20 only, and nothing after the core is written. */
    CHECK(strncmp(buf.payload, "Q1|r001|critical|42", 19) == 0);
    CHECK(buf.payload[19] == ' ');
    CHECK(first_non_space_after(buf.payload, 19, len) == len);

    /* No NUL terminator, newline, or CR inside the payload. */
    for (i = 0; i < len; i++) {
        CHECK(buf.payload[i] != '\0');
        CHECK(buf.payload[i] != '\n');
        CHECK(buf.payload[i] != '\r');
    }
    CHECK(buf.payload[127] == ' ');
}

static void test_critical_fields_match_protocol(void)
{
    guarded_buffer_t buf;
    size_t len = 0;
    char actual[128 + 1];
    char expected[128 + 1];
    size_t i;

    guarded_init(&buf);
    CHECK_STATUS(QOS_OK,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "b01_fifo", 300,
                               buf.payload, sizeof buf.payload, &len));
    CHECK(len == 128u);

    memcpy(actual, buf.payload, len);
    actual[len] = '\0';
    build_expected(expected, 128u, "Q1|b01_fifo|critical|300");
    expected[128] = '\0';
    CHECK(strcmp(actual, expected) == 0);

    /* Padding starts exactly after the unpadded core and is all spaces. */
    i = strlen("Q1|b01_fifo|critical|300");
    CHECK(i == 24u);
    CHECK(first_non_space_after(buf.payload, i, len) == len);
    CHECK(len - i == 128u - 24u);
}

static void test_sequence_field(void)
{
    guarded_buffer_t buf;
    size_t len = 0;

    guarded_init(&buf);
    CHECK_STATUS(QOS_OK,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "r001", 1,
                               buf.payload, sizeof buf.payload, &len));
    CHECK(len == 128u);
    CHECK(strncmp(buf.payload, "Q1|r001|critical|1", 18) == 0);
    CHECK(buf.payload[18] == ' ');

    guarded_init(&buf);
    CHECK_STATUS(QOS_OK,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "r001", 4294967295u,
                               buf.payload, sizeof buf.payload, &len));
    CHECK(len == 128u);
    CHECK(strncmp(buf.payload, "Q1|r001|critical|4294967295", 27) == 0);
    CHECK(buf.payload[27] == ' ');
}

static void test_routine_profile_uses_same_formatter(void)
{
    guarded_buffer_t buf;
    size_t len = 0;

    guarded_init(&buf);
    CHECK_STATUS(QOS_OK,
                 qos_format_q1(&QOS_PROFILE_ROUTINE, "b01_fifo", 1200,
                               buf.payload, sizeof buf.payload, &len));
    CHECK(len == 256u);
    CHECK(canaries_intact(&buf));
    CHECK(payload_equals(buf.payload, len, "Q1|b01_fifo|routine|1200", 256u));
    CHECK(buf.payload[255] == ' ');
    CHECK(QOS_PROFILE_ROUTINE.traffic_class == QOS_CLASS_ROUTINE);
    CHECK(QOS_PROFILE_ROUTINE.dscp == 0u);
    CHECK(QOS_PROFILE_ROUTINE.tos == 0x00u);
    CHECK(QOS_PROFILE_ROUTINE.interval_ms == 50u);
    CHECK(QOS_PROFILE_CRITICAL.interval_ms == 200u);
    CHECK(QOS_PROFILE_CRITICAL.dscp == 40u);
    CHECK(QOS_PROFILE_CRITICAL.tos == 0xA0u);
}

static void test_run_id_validation(void)
{
    char too_long[QOS_RUN_ID_MAX_LEN + 2];

    CHECK_STATUS(QOS_OK, qos_validate_run_id("b01_fifo"));
    CHECK_STATUS(QOS_OK, qos_validate_run_id("A"));
    CHECK_STATUS(QOS_OK, qos_validate_run_id("a1_B-2"));
    CHECK_STATUS(QOS_ERR_RUN_ID, qos_validate_run_id(""));
    CHECK_STATUS(QOS_ERR_RUN_ID, qos_validate_run_id(NULL));
    CHECK_STATUS(QOS_ERR_RUN_ID, qos_validate_run_id("has space"));
    CHECK_STATUS(QOS_ERR_RUN_ID, qos_validate_run_id("has|pipe"));
    CHECK_STATUS(QOS_ERR_RUN_ID, qos_validate_run_id("line\nbreak"));
    CHECK_STATUS(QOS_ERR_RUN_ID, qos_validate_run_id("tab\there"));

    memset(too_long, 'a', sizeof too_long);
    too_long[QOS_RUN_ID_MAX_LEN] = '\0';
    CHECK_STATUS(QOS_OK, qos_validate_run_id(too_long)); /* exactly 24 chars */
    too_long[QOS_RUN_ID_MAX_LEN] = 'a';
    too_long[QOS_RUN_ID_MAX_LEN + 1] = '\0';
    CHECK_STATUS(QOS_ERR_RUN_ID, qos_validate_run_id(too_long)); /* 25 chars */
}

static void test_rejects_invalid_input_without_writing(void)
{
    guarded_buffer_t buf;
    size_t len = 12345u;

    guarded_init(&buf);
    CHECK_STATUS(QOS_ERR_RUN_ID,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "bad id", 1,
                               buf.payload, sizeof buf.payload, &len));
    CHECK(len == 12345u);
    CHECK(canaries_intact(&buf));

    guarded_init(&buf);
    CHECK_STATUS(QOS_ERR_SEQUENCE,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "r001", 0,
                               buf.payload, sizeof buf.payload, &len));
    CHECK(len == 12345u);
    CHECK(canaries_intact(&buf));

    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_format_q1(NULL, "r001", 1, buf.payload,
                               sizeof buf.payload, &len));
    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, NULL, 1, buf.payload,
                               sizeof buf.payload, &len));
    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "r001", 1, NULL,
                               sizeof buf.payload, &len));
    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "r001", 1, buf.payload,
                               sizeof buf.payload, NULL));
}

static void test_rejects_short_buffer(void)
{
    guarded_buffer_t buf;
    size_t len = 12345u;

    guarded_init(&buf);
    CHECK_STATUS(QOS_ERR_BUFFER_TOO_SMALL,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "r001", 42,
                               buf.payload, 127u, &len));
    CHECK(len == 12345u);
    CHECK(canaries_intact(&buf));

    /* Exactly 128 bytes must be accepted (no hidden NUL terminator needed). */
    guarded_init(&buf);
    CHECK_STATUS(QOS_OK,
                 qos_format_q1(&QOS_PROFILE_CRITICAL, "r001", 42,
                               buf.payload, 128u, &len));
    CHECK(len == 128u);
    CHECK(canaries_intact(&buf));
}

static void test_rejects_inconsistent_profile(void)
{
    qos_profile_t bad = QOS_PROFILE_CRITICAL;
    guarded_buffer_t buf;
    size_t len = 0;

    guarded_init(&buf);
    bad.payload_bytes = QOS_MAX_PAYLOAD_BYTES + 1u;
    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_format_q1(&bad, "r001", 1, buf.payload,
                               sizeof buf.payload, &len));

    bad = QOS_PROFILE_CRITICAL;
    bad.class_name = NULL;
    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_format_q1(&bad, "r001", 1, buf.payload,
                               sizeof buf.payload, &len));

    bad = QOS_PROFILE_CRITICAL;
    bad.class_name = "crit|ical";
    CHECK_STATUS(QOS_ERR_ARGUMENT,
                 qos_format_q1(&bad, "r001", 1, buf.payload,
                               sizeof buf.payload, &len));
}

int main(void)
{
    test_critical_payload_is_exactly_128_bytes();
    test_critical_fields_match_protocol();
    test_sequence_field();
    test_routine_profile_uses_same_formatter();
    test_run_id_validation();
    test_rejects_invalid_input_without_writing();
    test_rejects_short_buffer();
    test_rejects_inconsistent_profile();

    printf("%s: %d checks, %d failure(s)\n",
           failures == 0 ? "PASS" : "FAIL", checks, failures);
    return failures == 0 ? 0 : 1;
}
