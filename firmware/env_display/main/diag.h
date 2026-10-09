/* Diagnostics snapshot contract: callers allocate diag_snapshot_t and pass it
 * to diag_get(), which overwrites it with a coherent point-in-time view of
 * uptime, heap usage, persisted reboot count, and Wi-Fi status.
 */
#pragma once

#include <stdint.h>

#include "wifi.h"

typedef struct {
    uint64_t uptime_seconds;
    uint32_t free_heap_bytes;
    uint32_t minimum_free_heap_bytes;
    uint32_t reboot_count;
    wifi_status_t wifi;
} diag_snapshot_t;

void diag_init(void);
void diag_get(diag_snapshot_t *out);
