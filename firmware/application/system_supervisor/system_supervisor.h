#ifndef SENTINELRT_SYSTEM_SUPERVISOR_H
#define SENTINELRT_SYSTEM_SUPERVISOR_H
#include <stdbool.h>
#include "status_indicator_port.h"
#include "system_health.h"
typedef struct {
    const status_indicator_port_t *status_indicator;
    system_health_t health;
    bool initialized;
} system_supervisor_t;
bool system_supervisor_initialize(system_supervisor_t *supervisor, const status_indicator_port_t *indicator);
void system_supervisor_run_heartbeat(system_supervisor_t *supervisor);
#endif
