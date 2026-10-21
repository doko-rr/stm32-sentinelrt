#include "environmental_sensor_port.h"

#include <stddef.h>

bool environmental_sensor_port_is_valid(
    const environmental_sensor_port_t *port
)
{
    return port != NULL
        && port->initialize != NULL
        && port->start_measurement != NULL
        && port->poll_measurement != NULL;
}

bool environmental_sensor_port_initialize(
    const environmental_sensor_port_t *port
)
{
    if (!environmental_sensor_port_is_valid(port))
    {
        return false;
    }

    return port->initialize(port->context);
}

environmental_sensor_operation_t environmental_sensor_port_start_measurement(
    const environmental_sensor_port_t *port
)
{
    if (!environmental_sensor_port_is_valid(port))
    {
        return ENVIRONMENTAL_SENSOR_OPERATION_INVALID_ARGUMENT;
    }

    return port->start_measurement(port->context);
}

environmental_sensor_operation_t environmental_sensor_port_poll_measurement(
    const environmental_sensor_port_t *port,
    environmental_sensor_reading_t *reading
)
{
    if (!environmental_sensor_port_is_valid(port) || reading == NULL)
    {
        return ENVIRONMENTAL_SENSOR_OPERATION_INVALID_ARGUMENT;
    }

    environmental_sensor_reading_t candidate = {0};
    environmental_sensor_operation_t result =
        port->poll_measurement(port->context, &candidate);

    if (result == ENVIRONMENTAL_SENSOR_OPERATION_OK)
    {
        /* Copy only a complete successful reading so asynchronous adapters
         * cannot expose partially updated values while conversion is pending. */
        *reading = candidate;
    }

    return result;
}

const char *environmental_sensor_operation_name(
    environmental_sensor_operation_t operation
)
{
    switch (operation)
    {
        case ENVIRONMENTAL_SENSOR_OPERATION_OK:
            return "OK";
        case ENVIRONMENTAL_SENSOR_OPERATION_PENDING:
            return "PENDING";
        case ENVIRONMENTAL_SENSOR_OPERATION_NOT_READY:
            return "NOT_READY";
        case ENVIRONMENTAL_SENSOR_OPERATION_TIMEOUT:
            return "TIMEOUT";
        case ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR:
            return "HARDWARE_ERROR";
        case ENVIRONMENTAL_SENSOR_OPERATION_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        default:
            return "UNKNOWN";
    }
}
