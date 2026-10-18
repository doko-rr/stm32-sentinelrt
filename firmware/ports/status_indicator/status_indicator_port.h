#ifndef SENTINELRT_STATUS_INDICATOR_PORT_H
#define SENTINELRT_STATUS_INDICATOR_PORT_H
#include <stdbool.h>
typedef struct {
    void *context;
    bool (*initialize)(void *context);
    void (*toggle)(void *context);
} status_indicator_port_t;
bool status_indicator_port_is_valid(const status_indicator_port_t *port);
#endif
