/*
 * Copy this file to include/config.h and fill in your own values.
 * include/config.h is gitignored so credentials never reach the repository.
 */

#ifndef QOS_ESP32_CONFIG_H
#define QOS_ESP32_CONFIG_H

/* Raspberry Pi 5 access point credentials. Keep these out of Git. */
#define WIFI_SSID "replace-with-ap-ssid"
#define WIFI_PASSWORD "replace-with-ap-password"

/* Wired receiver (Kubuntu NUC). Loopback is not reachable from the ESP32. */
#define NUC_IP "192.168.60.2"
#define NUC_PORT 5001

/* Run identifier shared with the NUC and the Routine sender, e.g. b01_fifo. */
#define RUN_ID "b01_fifo"

#endif /* QOS_ESP32_CONFIG_H */
