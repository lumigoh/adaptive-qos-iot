/*
 * ESP32 Critical traffic generator.
 *
 * Runs the same shared Q1 formatter, sender state machine and UDP transport as
 * the host tool; on the ESP32 the transport uses lwIP sockets so the IPv4 TOS
 * byte can be requested.
 *
 * The Timing/ACK/logging logic is host-tested. Wi-Fi operation, real packet
 * marking, timing accuracy and RTT have NOT been verified on hardware.
 *
 * Run lifecycle follows docs/testing.md but the start trigger is deliberately
 * simple: one run starts automatically after boot. A shared trigger for formal
 * runs must still be agreed with the NUC/Bulk components.
 */

#include <Arduino.h>
#include <WiFi.h>

#if __has_include("config.h")
#include "config.h"
#else
#error "Copy include/config.example.h to include/config.h and fill in your values"
#endif

#include "qos_protocol.h"
#include "qos_sender.h"
#include "qos_udp.h"

static qos_record_t records[QOS_MAX_RECORDS];
static qos_record_log_t log_store;
static qos_sender_t sender;
static qos_udp_t udp;
static bool finished;
static qos_phase_t last_phase;

static uint32_t now_ms(void *ctx)
{
    (void)ctx;
    return (uint32_t)millis();
}

static int send_datagram(void *ctx, const char *payload, size_t len)
{
    (void)ctx;
    return qos_udp_send(&udp, payload, len);
}

static void print_csv(void)
{
    char line[128];
    size_t i;

    if (!qos_sender_csv_header(line, sizeof line)) {
        Serial.println("csv header error");
        return;
    }
    Serial.println(line);
    for (i = 0u; i < log_store.count; i++) {
        if (!qos_sender_csv_row(&sender, &log_store.records[i], line,
                                sizeof line)) {
            Serial.println("csv row error");
            return;
        }
        Serial.println(line);
    }
}

static void print_counters(void)
{
    Serial.printf(
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
}

void setup()
{
    qos_status_t status;

    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("Adaptive QoS - ESP32 Critical sender");
    Serial.println("host-tested core; Wi-Fi/DSCP/timing unverified on hardware");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("connecting to Wi-Fi");
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi failed; check config.h and the Pi access point");
        while (true) {
            delay(1000);
        }
    }
    Serial.print("ip=");
    Serial.println(WiFi.localIP());

    if (qos_net_init() != 0) {
        Serial.println("network start-up failed");
        while (true) {
            delay(1000);
        }
    }
    if (qos_udp_open(&udp, NUC_IP, NUC_PORT) != 0) {
        Serial.println("UDP socket open failed");
        while (true) {
            delay(1000);
        }
    }
    if (qos_udp_set_tos(&udp, QOS_PROFILE_CRITICAL.tos) != 0) {
        Serial.printf(
            "warning: could not request TOS 0x%02X; marking is NOT verified\n",
            (unsigned)QOS_PROFILE_CRITICAL.tos);
    }

    log_store.capacity = QOS_MAX_RECORDS;
    log_store.count = 0u;
    log_store.records = records;

    status = qos_sender_init(&sender, &QOS_PROFILE_CRITICAL, RUN_ID,
                             QOS_WARMUP_MS, QOS_MEASURE_MS, QOS_DRAIN_MS,
                             QOS_ACK_TIMEOUT_MS, &log_store, now_ms,
                             send_datagram, nullptr);
    if (status != QOS_OK) {
        Serial.printf("sender init failed (status %d); check RUN_ID\n",
                      (int)status);
        while (true) {
            delay(1000);
        }
    }

    Serial.printf("run %s -> %s:%u, 128-byte Q1 every 200 ms\n", RUN_ID,
                  NUC_IP, (unsigned)NUC_PORT);
    Serial.printf("warm-up %u ms, measure %u ms, drain %u ms, ACK timeout %u ms\n",
                  (unsigned)QOS_WARMUP_MS, (unsigned)QOS_MEASURE_MS,
                  (unsigned)QOS_DRAIN_MS, (unsigned)QOS_ACK_TIMEOUT_MS);

    qos_sender_start(&sender, now_ms(nullptr));
    last_phase = qos_sender_phase(&sender);
    finished = false;
}

void loop()
{
    char buffer[512];
    size_t received = 0u;
    int ready;

    if (finished) {
        delay(1000);
        return;
    }

    qos_sender_tick(&sender);

    if (qos_sender_phase(&sender) != last_phase) {
        last_phase = qos_sender_phase(&sender);
        Serial.printf("phase: %s t=%lu ms\n", qos_phase_name(last_phase),
                      (unsigned long)now_ms(nullptr));
    }

    ready = qos_udp_receive(&udp, buffer, sizeof buffer, &received, 0);
    if (ready == 1) {
        qos_sender_on_ack(&sender, buffer, received);
    }

    if (qos_sender_phase(&sender) == QOS_PHASE_DONE) {
        print_csv();
        print_counters();
        qos_udp_close(&udp);
        finished = true;
        Serial.println("run complete");
    }
}
