/*
 * Host UDP sender for the Adaptive QoS project (Critical or Routine profile).
 *
 * This runs the real shared sender core against a real UDP socket, so it can
 * be exercised locally against a receiver on 127.0.0.1 before any ESP32 is
 * available. It is a software tool: localhost runs say nothing about Wi-Fi,
 * gateway QoS, real DSCP treatment or on-device timing.
 *
 * Build: firmware/host/Makefile (or see firmware/README.md).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qos_protocol.h"
#include "qos_sender.h"
#include "qos_udp.h"

#if defined(_WIN32)
#include <windows.h>
#define QOS_STRCMP _stricmp
static uint32_t monotonic_ms(void *ctx)
{
    (void)ctx;
    return (uint32_t)GetTickCount64();
}
#else
#include <strings.h>
#include <time.h>
#define QOS_STRCMP strcasecmp
static uint32_t monotonic_ms(void *ctx)
{
    struct timespec ts;

    (void)ctx;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0u;
    }
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u +
                      (uint64_t)ts.tv_nsec / 1000000u);
}
#endif

typedef struct {
    qos_udp_t udp;
} io_context_t;

static int send_datagram(void *ctx, const char *payload, size_t len)
{
    io_context_t *io = (io_context_t *)ctx;

    return qos_udp_send(&io->udp, payload, len);
}

static void usage(const char *program)
{
    printf(
        "Usage: %s --run-id <id> [options]\n"
        "\n"
        "  --run-id <id>       1-24 chars of A-Z a-z 0-9 _ -   (required)\n"
        "  --profile <name>    critical (default) or routine\n"
        "  --dest <ipv4>       receiver address (default 192.168.60.2)\n"
        "  --port <n>          receiver UDP port (default 5001)\n"
        "  --warmup-ms <n>     warm-up length (default %u)\n"
        "  --measure-ms <n>    measured length (default %u)\n"
        "  --drain-ms <n>      drain after measuring (default %u)\n"
        "  --timeout-ms <n>    ACK timeout (default %u)\n"
        "  --csv <path>        write sender CSV here (default stdout)\n"
        "  --help\n"
        "\n"
        "Short durations are for local testing only; formal runs use the\n"
        "defaults from docs/testing.md.\n",
        program, (unsigned)QOS_WARMUP_MS, (unsigned)QOS_MEASURE_MS,
        (unsigned)QOS_DRAIN_MS, (unsigned)QOS_ACK_TIMEOUT_MS);
}

static int parse_u32(const char *text, uint32_t *out)
{
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || *text == '\0') {
        return -1;
    }
    value = strtoull(text, &end, 10);
    if (end == NULL || *end != '\0' || value > 0xFFFFFFFFull) {
        return -1;
    }
    *out = (uint32_t)value;
    return 0;
}

static int write_csv(const qos_sender_t *sender, FILE *out)
{
    char line[256];
    size_t i;

    if (!qos_sender_csv_header(line, sizeof line)) {
        return -1;
    }
    if (fprintf(out, "%s\n", line) < 0) {
        return -1;
    }
    for (i = 0u; i < sender->log->count; i++) {
        if (!qos_sender_csv_row(sender, &sender->log->records[i], line,
                                sizeof line)) {
            return -1;
        }
        if (fprintf(out, "%s\n", line) < 0) {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *run_id = NULL;
    const char *profile_name = "critical";
    const char *dest = "192.168.60.2";
    const char *csv_path = NULL;
    uint32_t port = 5001u;
    uint32_t warmup_ms = QOS_WARMUP_MS;
    uint32_t measure_ms = QOS_MEASURE_MS;
    uint32_t drain_ms = QOS_DRAIN_MS;
    uint32_t timeout_ms = QOS_ACK_TIMEOUT_MS;
    static qos_record_t records[QOS_MAX_RECORDS];
    qos_record_log_t log;
    qos_sender_t sender;
    io_context_t io;
    const qos_profile_t *profile;
    qos_status_t status;
    qos_phase_t last_phase;
    FILE *csv = stdout;
    char line[256];
    char ack[512];
    size_t received_len = 0;
    int i;
    int exit_code = 1;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(arg, "--run-id") == 0 && value != NULL) {
            run_id = value;
            i++;
        } else if (strcmp(arg, "--profile") == 0 && value != NULL) {
            profile_name = value;
            i++;
        } else if (strcmp(arg, "--dest") == 0 && value != NULL) {
            dest = value;
            i++;
        } else if (strcmp(arg, "--port") == 0 && value != NULL) {
            i++;
            if (parse_u32(value, &port) != 0 || port == 0u || port > 65535u) {
                fprintf(stderr, "invalid --port: %s\n", value);
                return 2;
            }
        } else if (strcmp(arg, "--warmup-ms") == 0 && value != NULL) {
            i++;
            if (parse_u32(value, &warmup_ms) != 0) {
                fprintf(stderr, "invalid --warmup-ms: %s\n", value);
                return 2;
            }
        } else if (strcmp(arg, "--measure-ms") == 0 && value != NULL) {
            i++;
            if (parse_u32(value, &measure_ms) != 0) {
                fprintf(stderr, "invalid --measure-ms: %s\n", value);
                return 2;
            }
        } else if (strcmp(arg, "--drain-ms") == 0 && value != NULL) {
            i++;
            if (parse_u32(value, &drain_ms) != 0) {
                fprintf(stderr, "invalid --drain-ms: %s\n", value);
                return 2;
            }
        } else if (strcmp(arg, "--timeout-ms") == 0 && value != NULL) {
            i++;
            if (parse_u32(value, &timeout_ms) != 0) {
                fprintf(stderr, "invalid --timeout-ms: %s\n", value);
                return 2;
            }
        } else if (strcmp(arg, "--csv") == 0 && value != NULL) {
            csv_path = value;
            i++;
        } else {
            fprintf(stderr, "unknown or incomplete option: %s\n\n", arg);
            usage(argv[0]);
            return 2;
        }
    }

    if (run_id == NULL) {
        fprintf(stderr, "--run-id is required\n\n");
        usage(argv[0]);
        return 2;
    }

    if (QOS_STRCMP(profile_name, "critical") == 0) {
        profile = &QOS_PROFILE_CRITICAL;
    } else if (QOS_STRCMP(profile_name, "routine") == 0) {
        profile = &QOS_PROFILE_ROUTINE;
    } else {
        fprintf(stderr, "unknown profile: %s\n", profile_name);
        return 2;
    }

    if (csv_path != NULL) {
        csv = fopen(csv_path, "w");
        if (csv == NULL) {
            fprintf(stderr, "cannot open %s for writing\n", csv_path);
            return 2;
        }
    }

    if (qos_net_init() != 0) {
        fprintf(stderr, "network start-up failed (%d)\n", qos_udp_last_error());
        goto done;
    }
    if (qos_udp_open(&io.udp, dest, (uint16_t)port) != 0) {
        fprintf(stderr, "cannot open UDP socket to %s:%u (%d)\n", dest,
                (unsigned)port, qos_udp_last_error());
        goto done;
    }

    if (qos_udp_set_tos(&io.udp, profile->tos) != 0) {
        fprintf(stderr,
                "warning: could not request TOS 0x%02X on this socket (%d); "
                "the mark is not verified\n",
                (unsigned)profile->tos, qos_udp_last_error());
    }

    log.capacity = QOS_MAX_RECORDS;
    log.count = 0u;
    log.records = records;

    status = qos_sender_init(&sender, profile, run_id, warmup_ms, measure_ms,
                             drain_ms, timeout_ms, &log, monotonic_ms,
                             send_datagram, &io);
    if (status != QOS_OK) {
        const char *reason =
            status == QOS_ERR_RUN_ID
                ? "invalid run id"
                : status == QOS_ERR_ARGUMENT
                      ? "invalid configuration or record log too small"
                      : "format error";
        fprintf(stderr, "sender init failed: %s\n", reason);
        goto close_udp;
    }

    fprintf(stderr,
            "run %s | %s | %u-byte payload every %u ms | %s:%u\n"
            "warm-up %u ms, measure %u ms, drain %u ms, ACK timeout %u ms\n"
            "host software run: not ESP32 hardware and not QoS evidence\n",
            run_id, profile->class_name, (unsigned)profile->payload_bytes,
            (unsigned)profile->interval_ms, dest, (unsigned)port,
            (unsigned)warmup_ms, (unsigned)measure_ms, (unsigned)drain_ms,
            (unsigned)timeout_ms);

    if (!qos_sender_csv_header(line, sizeof line)) {
        fprintf(stderr, "internal CSV header error\n");
        goto close_udp;
    }

    qos_sender_start(&sender, monotonic_ms(NULL));
    last_phase = qos_sender_phase(&sender);

    while (qos_sender_phase(&sender) != QOS_PHASE_DONE) {
        int ready;

        qos_sender_tick(&sender);

        if (qos_sender_phase(&sender) != last_phase) {
            last_phase = qos_sender_phase(&sender);
            fprintf(stderr, "phase: %s\n", qos_phase_name(last_phase));
        }

        ready = qos_udp_receive(&io.udp, ack, sizeof ack, &received_len, 2);
        if (ready == 1) {
            qos_sender_on_ack(&sender, ack, received_len);
        } else if (ready == -1) {
            fprintf(stderr, "socket receive failed (%d)\n",
                    qos_udp_last_error());
            goto close_udp;
        }
    }

    if (write_csv(&sender, csv) != 0) {
        fprintf(stderr, "cannot write CSV rows\n");
        goto close_udp;
    }
    if (csv != stdout) {
        fclose(csv);
        csv = stdout;
    } else {
        fflush(csv);
    }

    fprintf(stderr,
            "sent=%lu on_time=%lu late=%lu timed_out=%lu duplicate=%lu "
            "invalid=%lu unmatched=%lu send_errors=%lu\n",
            (unsigned long)sender.counters.sent,
            (unsigned long)sender.counters.on_time,
            (unsigned long)sender.counters.late,
            (unsigned long)sender.counters.timed_out,
            (unsigned long)sender.counters.duplicate_acks,
            (unsigned long)sender.counters.invalid_acks,
            (unsigned long)sender.counters.unmatched_acks,
            (unsigned long)sender.counters.send_errors);
    fprintf(stderr, "sender log written (%lu rows)\n",
            (unsigned long)sender.log->count);
    exit_code = 0;

close_udp:
    qos_udp_close(&io.udp);
    qos_net_shutdown();

done:
    if (csv != stdout) {
        fclose(csv);
    }
    return exit_code;
}
