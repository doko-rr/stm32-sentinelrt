#ifndef SENTINELRT_SYSTEM_SUPERVISOR_H
#define SENTINELRT_SYSTEM_SUPERVISOR_H

#include <stdbool.h>
#include <stdint.h>

#include "status_indicator_port.h"
#include "system_health.h"
#include "../../ports/watchdog_port.h"

typedef enum {
    SYSTEM_WATCHDOG_NOT_CONFIGURED = 0,
    SYSTEM_WATCHDOG_NOT_READY,
    SYSTEM_WATCHDOG_REFRESHED,
    SYSTEM_WATCHDOG_BLOCKED_NO_PROGRESS,
    SYSTEM_WATCHDOG_BLOCKED_STACK_CRITICAL,
    SYSTEM_WATCHDOG_REFRESH_FAILED
} system_watchdog_decision_t;

typedef struct {
    uint32_t heartbeat_timeout_ticks;
    uint32_t sensor_timeout_ticks;
} system_watchdog_policy_t;

typedef struct {
    status_indicator_port_t status_indicator;
    watchdog_port_t watchdog;
    system_health_t health;
    system_watchdog_policy_t watchdog_policy;
    uint32_t last_watchdog_heartbeat;
    uint32_t last_watchdog_sensor_sequence;
    uint32_t last_heartbeat_progress_tick;
    uint32_t last_sensor_progress_tick;
    uint32_t watchdog_refresh_count;
    system_watchdog_decision_t watchdog_decision;
    bool initialized;
    bool watchdog_baseline_ready;
} system_supervisor_t;

bool system_supervisor_initialize(
    system_supervisor_t *supervisor,
    const status_indicator_port_t *indicator
);

bool system_supervisor_configure_watchdog(
    system_supervisor_t *supervisor,
    const watchdog_port_t *watchdog,
    const system_watchdog_policy_t *policy
);

void system_supervisor_run_heartbeat(system_supervisor_t *supervisor);

system_watchdog_decision_t system_supervisor_service_watchdog(
    system_supervisor_t *supervisor,
    uint32_t current_tick,
    uint32_t sensor_sequence,
    bool sensor_stack_ok,
    bool default_stack_ok
);

#endif
