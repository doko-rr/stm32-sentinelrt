#include "system_supervisor.h"
#include <stddef.h>
bool system_supervisor_initialize(system_supervisor_t *supervisor, const status_indicator_port_t *indicator)
{
    if (supervisor == NULL || !status_indicator_port_is_valid(indicator)) { return false; }
    system_health_reset(&supervisor->health);
    if (!indicator->initialize(indicator->context)) { return false; }
    supervisor->status_indicator = indicator;
    supervisor->initialized = true;
    return true;
}
void system_supervisor_run_heartbeat(system_supervisor_t *supervisor)
{
    if (supervisor == NULL || !supervisor->initialized) { return; }
    supervisor->status_indicator->toggle(supervisor->status_indicator->context);
    system_health_record_heartbeat(&supervisor->health);
}
