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

static bool watchdog_policy_is_valid(const system_watchdog_policy_t *policy)
{
    return policy != NULL
        && policy->heartbeat_timeout_ticks > 0U
        && policy->heartbeat_timeout_ticks <= INT32_MAX
        && policy->sensor_timeout_ticks > 0U
        && policy->sensor_timeout_ticks <= INT32_MAX;
}

static bool progress_timed_out(
    uint32_t current_tick,
    uint32_t progress_tick,
    uint32_t timeout_ticks)
{
    return (uint32_t)(current_tick - progress_tick) >= timeout_ticks;
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
    const watchdog_port_t *watchdog,
    const system_watchdog_policy_t *policy)
{
    if (supervisor == NULL || !supervisor->initialized
        || !watchdog_port_is_valid(watchdog)
        || !watchdog_policy_is_valid(policy))
    {
        return false;
    }

    supervisor->watchdog = *watchdog;
    supervisor->watchdog_policy = *policy;
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

    supervisor->status_indicator.toggle(supervisor->status_indicator.context);
    system_health_record_heartbeat(&supervisor->health);
}

system_watchdog_decision_t system_supervisor_service_watchdog(
    system_supervisor_t *supervisor,
    uint32_t current_tick,
    uint32_t sensor_sequence,
    bool sensor_stack_ok,
    bool default_stack_ok)
{
    uint32_t heartbeat;

    if (supervisor == NULL || !supervisor->initialized
        || !watchdog_port_is_valid(&supervisor->watchdog)
        || !watchdog_policy_is_valid(&supervisor->watchdog_policy))
    {
        return SYSTEM_WATCHDOG_NOT_CONFIGURED;
    }

    if (!sensor_stack_ok || !default_stack_ok)
    {
        supervisor->watchdog_decision = SYSTEM_WATCHDOG_BLOCKED_STACK_CRITICAL;
        return supervisor->watchdog_decision;
    }

    heartbeat = system_health_heartbeat_count(&supervisor->health);
    if (!supervisor->watchdog_baseline_ready)
    {
        supervisor->last_watchdog_heartbeat = heartbeat;
        supervisor->last_watchdog_sensor_sequence = sensor_sequence;
        supervisor->last_heartbeat_progress_tick = current_tick;
        supervisor->last_sensor_progress_tick = current_tick;
        supervisor->watchdog_baseline_ready = true;
        supervisor->watchdog_decision = SYSTEM_WATCHDOG_NOT_READY;
        return supervisor->watchdog_decision;
    }

    if (heartbeat != supervisor->last_watchdog_heartbeat)
    {
        supervisor->last_watchdog_heartbeat = heartbeat;
        supervisor->last_heartbeat_progress_tick = current_tick;
    }

    if (sensor_sequence != supervisor->last_watchdog_sensor_sequence)
    {
        supervisor->last_watchdog_sensor_sequence = sensor_sequence;
        supervisor->last_sensor_progress_tick = current_tick;
    }

    if (progress_timed_out(
            current_tick,
            supervisor->last_heartbeat_progress_tick,
            supervisor->watchdog_policy.heartbeat_timeout_ticks)
        || progress_timed_out(
            current_tick,
            supervisor->last_sensor_progress_tick,
            supervisor->watchdog_policy.sensor_timeout_ticks))
    {
        supervisor->watchdog_decision = SYSTEM_WATCHDOG_BLOCKED_NO_PROGRESS;
        return supervisor->watchdog_decision;
    }

    if (!supervisor->watchdog.refresh(supervisor->watchdog.context))
    {
        supervisor->watchdog_decision = SYSTEM_WATCHDOG_REFRESH_FAILED;
        return supervisor->watchdog_decision;
    }

    increment_saturating(&supervisor->watchdog_refresh_count);
    supervisor->watchdog_decision = SYSTEM_WATCHDOG_REFRESHED;
    return supervisor->watchdog_decision;
}
