#include "qos_protocol.h"

#include <stdio.h>
#include <string.h>

/*
 * Longest possible unpadded message:
 *   "Q1|" + 24-char run_id + "|" + "critical" + "|" + 10-digit sequence
 *   = 3 + 24 + 1 + 8 + 1 + 10 = 47 bytes.
 */
#define QOS_CORE_MAX_LEN 48u

const qos_profile_t QOS_PROFILE_CRITICAL = {
    QOS_CLASS_CRITICAL,
    "critical",
    QOS_CRITICAL_PAYLOAD_BYTES,
    200u,
    40u,   /* DSCP CS5 */
    0xA0u  /* TOS byte */
};

const qos_profile_t QOS_PROFILE_ROUTINE = {
    QOS_CLASS_ROUTINE,
    "routine",
    QOS_ROUTINE_PAYLOAD_BYTES,
    50u,
    0u,    /* DSCP CS0 */
    0x00u  /* TOS byte */
};

static int qos_class_name_is_valid(const char *name)
{
    size_t i;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    for (i = 0; name[i] != '\0'; i++) {
        if (i >= QOS_CORE_MAX_LEN || name[i] == '|' || name[i] == ' ' ||
            name[i] == '\n' || name[i] == '\r') {
            return 0;
        }
    }
    return 1;
}

static int qos_profile_is_valid(const qos_profile_t *profile)
{
    if (profile == NULL) {
        return 0;
    }
    if (profile->traffic_class != QOS_CLASS_CRITICAL &&
        profile->traffic_class != QOS_CLASS_ROUTINE) {
        return 0;
    }
    if (profile->payload_bytes < QOS_CORE_MAX_LEN ||
        profile->payload_bytes > QOS_MAX_PAYLOAD_BYTES) {
        return 0;
    }
    return qos_class_name_is_valid(profile->class_name);
}

qos_status_t qos_validate_run_id(const char *run_id)
{
    size_t i;

    if (run_id == NULL) {
        return QOS_ERR_RUN_ID;
    }
    for (i = 0; run_id[i] != '\0'; i++) {
        char c = run_id[i];

        if (i >= QOS_RUN_ID_MAX_LEN) {
            return QOS_ERR_RUN_ID;
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return QOS_ERR_RUN_ID;
        }
    }
    if (i == 0u) {
        return QOS_ERR_RUN_ID;
    }
    return QOS_OK;
}

qos_status_t qos_format_q1(const qos_profile_t *profile,
                           const char *run_id,
                           uint32_t sequence,
                           char *buffer,
                           size_t buffer_size,
                           size_t *out_len)
{
    char core[QOS_CORE_MAX_LEN + 1u];
    size_t core_len;
    size_t required;
    int written;

    if (profile == NULL || run_id == NULL || buffer == NULL || out_len == NULL) {
        return QOS_ERR_ARGUMENT;
    }
    if (!qos_profile_is_valid(profile)) {
        return QOS_ERR_ARGUMENT;
    }
    if (qos_validate_run_id(run_id) != QOS_OK) {
        return QOS_ERR_RUN_ID;
    }
    if (sequence == 0u) {
        return QOS_ERR_SEQUENCE;
    }

    written = snprintf(core, sizeof core, "%s|%s|%s|%lu",
                       QOS_Q1_PREFIX, run_id, profile->class_name,
                       (unsigned long)sequence);
    if (written < 0 || (size_t)written >= sizeof core) {
        return QOS_ERR_ARGUMENT;
    }
    core_len = (size_t)written;

    required = core_len > profile->payload_bytes ? core_len
                                                 : profile->payload_bytes;
    if (buffer_size < required) {
        return QOS_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(buffer, core, core_len);
    memset(buffer + core_len, ' ', profile->payload_bytes - core_len);

    *out_len = profile->payload_bytes;
    return QOS_OK;
}
