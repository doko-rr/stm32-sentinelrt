#ifndef SENTINELRT_SYSTEM_HEALTH_H
#define SENTINELRT_SYSTEM_HEALTH_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint32_t heartbeat_count; bool initialized; } system_health_t;
void system_health_reset(system_health_t *health);
void system_health_record_heartbeat(system_health_t *health);
uint32_t system_health_heartbeat_count(const system_health_t *health);
#endif
