#ifndef SENTINELRT_WATCHDOG_PORT_H
#define SENTINELRT_WATCHDOG_PORT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    void *context;
    bool (*refresh)(void *context);
} watchdog_port_t;

static inline bool watchdog_port_is_valid(const watchdog_port_t *port)
{
    return port != NULL && port->refresh != NULL;
}

#endif
