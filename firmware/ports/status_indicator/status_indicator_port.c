#include "status_indicator_port.h"
#include <stddef.h>
bool status_indicator_port_is_valid(const status_indicator_port_t *port)
{
    return port != NULL && port->initialize != NULL && port->toggle != NULL;
}
