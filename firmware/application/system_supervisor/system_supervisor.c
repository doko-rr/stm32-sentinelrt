#include "system_supervisor.h"

#include <limits.h>
#include <stddef.h>

static void increment_saturating(uint32_t *counter)
{
    if (counter != NULL && *counter < UINT32_MAX)
    {
        (*counter)++;
    }
}

bool system_supervisor_initialize(
    system_supervisor_t *supervisor,
    const status_indicator_port_t *indicator)
{
    if (supervisor == NULL || !status_indicator_port_is_valid(indicator))
    {
        return false;
    }

    *supervisor = (system_supervisor_t){0};
    system_health_reset(&supervisor->health);
    if (!indicator->initialize(indicator->context))
    {
        return false;
    }

    supervisor->status_indicator = *indicator;
    supervisor->watchdog_decision = SYSTEM_WATCHDOG_NOT_CONFIGURED;
    supervisor->initialized = true;
    return true;
}

bool system_supervisor_configure_watchdog(
    system_supervisor_t *supervisor,
    const watchdog_port_t *watchdog)
{
    if (supervisor == NULL || !supervisor->initialized
        || !watchdog_port_is_valid(watchdog))
    {
        return false;
    }

    supervisor->watchdog = *watchdog;
    supervisor->watchdog_baseline_ready = false;
    supervisor->watchdog_decision = SYSTEM_WATCHDOG_NOT_READY;
    return true;
}

void system_supervisor_run_heartbeat(system_supervisor_t *supervisor)
{
    if (supervisor == NULL || !supervisor->initialized)
    {
        return;
    }

    supervisor->status_indicator.toggle(
        supervisor->status_indicator.context
    );
    system_health_record_heartbeat(&supervisor->health);
}

system_watchdog_decision_t system_supervisor_service_watchdog(
    system_supervisor_t *supervisor,
    uint32_t sensor_sequence,
    bool sensor_stack_ok,
    bool default_stack_ok)
{
    uint32_t heartbeat;

    if (supervisor == NULL || !supervisor->initialized
        || !watchdog_port_is_valid(&supervisor->watchdog))
    {
        return SYSTEM_WATCHDOG_NOT_CONFIGURED;
    }

    if (!sensor_stack_ok || !default_stack_ok)
    {
        supervisor->watchdog_decision =
            SYSTEM_WATCHDOG_BLOCKED_STACK_CRITICAL;
        return supervisor->watchdog_decision;
    }

    heartbeat = system_health_heartbeat_count(&supervisor->health);
    if (!supervisor->watchdog_baseline_ready)
    {
        supervisor->last_watchdog_heartbeat = heartbeat;
        supervisor->last_watchdog_sensor_sequence = sensor_sequence;
        supervisor->watchdog_baseline_ready = true;
        supervisor->watchdog_decision = SYSTEM_WATCHDOG_NOT_READY;
        return supervisor->watchdog_decision;
    }

    if (heartbeat == supervisor->last_watchdog_heartbeat
        || sensor_sequence == supervisor->last_watchdog_sensor_sequence)
    {
        supervisor->watchdog_decision =
            SYSTEM_WATCHDOG_BLOCKED_NO_PROGRESS;
        return supervisor->watchdog_decision;
    }

    if (!supervisor->watchdog.refresh(supervisor->watchdog.context))
    {
        supervisor->watchdog_decision = SYSTEM_WATCHDOG_REFRESH_FAILED;
        return supervisor->watchdog_decision;
    }

    supervisor->last_watchdog_heartbeat = heartbeat;
    supervisor->last_watchdog_sensor_sequence = sensor_sequence;
    increment_saturating(&supervisor->watchdog_refresh_count);
    supervisor->watchdog_decision = SYSTEM_WATCHDOG_REFRESHED;
    return supervisor->watchdog_decision;
}
