#include "system_health.h"
#include <stddef.h>
void system_health_reset(system_health_t *health)
{
    if (health == NULL) { return; }
    health->heartbeat_count = 0U;
    health->initialized = true;
}
void system_health_record_heartbeat(system_health_t *health)
{
    if (health == NULL || !health->initialized) { return; }
    if (health->heartbeat_count < UINT32_MAX) { health->heartbeat_count++; }
}
uint32_t system_health_heartbeat_count(const system_health_t *health)
{
    if (health == NULL || !health->initialized) { return 0U; }
    return health->heartbeat_count;
}
