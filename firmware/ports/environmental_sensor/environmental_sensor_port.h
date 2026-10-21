#ifndef SENTINELRT_ENVIRONMENTAL_SENSOR_PORT_H
#define SENTINELRT_ENVIRONMENTAL_SENSOR_PORT_H

#include <stdbool.h>

/**
 * Describes the outcome of one hardware-facing sensor operation.
 *
 * PENDING is not an error. It tells the application that sensor conversion is
 * still in progress, allowing the FreeRTOS task to yield instead of blocking.
 */
typedef enum
{
    ENVIRONMENTAL_SENSOR_OPERATION_OK = 0,
    ENVIRONMENTAL_SENSOR_OPERATION_PENDING,
    ENVIRONMENTAL_SENSOR_OPERATION_NOT_READY,
    ENVIRONMENTAL_SENSOR_OPERATION_TIMEOUT,
    ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR,
    ENVIRONMENTAL_SENSOR_OPERATION_INVALID_ARGUMENT
} environmental_sensor_operation_t;

/**
 * Carries raw physical measurements from a hardware adapter.
 *
 * Range validation intentionally belongs to the sensor_sample domain. Keeping
 * the port limited to transport concerns prevents hardware drivers from
 * silently applying different business rules.
 */
typedef struct
{
    float temperature_celsius;
    float relative_humidity_percent;
} environmental_sensor_reading_t;

typedef bool (*environmental_sensor_initialize_fn)(void *context);
typedef environmental_sensor_operation_t
    (*environmental_sensor_start_measurement_fn)(void *context);
typedef environmental_sensor_operation_t
    (*environmental_sensor_poll_measurement_fn)(
        void *context,
        environmental_sensor_reading_t *reading
    );

/**
 * Hardware abstraction used by the sensor acquisition application service.
 *
 * context is owned by the concrete adapter and must outlive this port. The
 * domain and application layers never interpret or free that pointer.
 */
typedef struct
{
    void *context;
    environmental_sensor_initialize_fn initialize;
    environmental_sensor_start_measurement_fn start_measurement;
    environmental_sensor_poll_measurement_fn poll_measurement;
} environmental_sensor_port_t;

/** Returns true only when every callback required by the contract is present. */
bool environmental_sensor_port_is_valid(
    const environmental_sensor_port_t *port
);

/**
 * Initializes the concrete sensor adapter.
 *
 * This call may configure hardware, but it must not start a measurement. A
 * false result lets the application remain in a safe uninitialized state.
 */
bool environmental_sensor_port_initialize(
    const environmental_sensor_port_t *port
);

/**
 * Starts a non-blocking sensor conversion.
 *
 * The adapter returns immediately. The application later calls poll_measurement
 * until the result is OK or a terminal failure is reported.
 */
environmental_sensor_operation_t environmental_sensor_port_start_measurement(
    const environmental_sensor_port_t *port
);

/**
 * Polls a conversion without blocking the caller.
 *
 * reading is updated only when the adapter reports OK. Preserving the caller's
 * previous value on PENDING or failure prevents partially written measurements
 * from escaping into the domain layer.
 */
environmental_sensor_operation_t environmental_sensor_port_poll_measurement(
    const environmental_sensor_port_t *port,
    environmental_sensor_reading_t *reading
);

const char *environmental_sensor_operation_name(
    environmental_sensor_operation_t operation
);

#endif
