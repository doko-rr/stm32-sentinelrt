#ifndef SENTINELRT_SENSOR_ACQUISITION_H
#define SENTINELRT_SENSOR_ACQUISITION_H

#include <stdbool.h>
#include <stdint.h>
#include "environmental_sensor_port.h"
#include "sensor_sample.h"

typedef enum {
    SENSOR_ACQUISITION_UNINITIALIZED = 0,
    SENSOR_ACQUISITION_IDLE,
    SENSOR_ACQUISITION_WAITING_FOR_SAMPLE
} sensor_acquisition_state_t;

typedef struct {
    sensor_sample_limits_t limits;
    uint32_t measurement_interval_ticks;
    uint32_t measurement_timeout_ticks;
    uint32_t stale_after_ticks;
} sensor_acquisition_config_t;

typedef struct {
    uint32_t total_attempts;
    uint32_t successful_samples;
    uint32_t out_of_range_samples;
    uint32_t timeout_failures;
    uint32_t hardware_failures;
    uint32_t consecutive_failures;
    uint32_t peak_consecutive_failures;
    uint32_t last_success_sequence;
} sensor_acquisition_diagnostics_t;

/**
 * Owns non-blocking acquisition state for one environmental sensor.
 * The adapter context remains platform-owned and must outlive this service.
 */
typedef struct {
    environmental_sensor_port_t sensor_port;
    sensor_acquisition_config_t config;
    sensor_sample_t latest_result;
    sensor_sample_t last_known_good_sample;
    sensor_acquisition_diagnostics_t diagnostics;
    sensor_acquisition_state_t state;
    uint32_t measurement_started_at_tick;
    uint32_t next_measurement_at_tick;
    uint32_t next_sequence;
    bool has_last_known_good_sample;
} sensor_acquisition_t;

/** Initializes hardware but does not start a measurement. */
bool sensor_acquisition_initialize(
    sensor_acquisition_t *service,
    const environmental_sensor_port_t *sensor_port,
    const sensor_acquisition_config_t *config,
    uint32_t current_tick
);

/**
 * Advances one non-blocking state-machine step using a monotonic tick.
 * Configured intervals must remain below half of the uint32 tick range.
 */
void sensor_acquisition_process(sensor_acquisition_t *service, uint32_t current_tick);

sensor_sample_t sensor_acquisition_latest_result(
    const sensor_acquisition_t *service,
    uint32_t current_tick
);

/** Returns the most recent valid sample; the returned copy may be STALE. */
bool sensor_acquisition_last_known_good(
    const sensor_acquisition_t *service,
    uint32_t current_tick,
    sensor_sample_t *sample
);

/** Copies a read-only health snapshot without changing service state. */
bool sensor_acquisition_get_diagnostics(
    const sensor_acquisition_t *service,
    sensor_acquisition_diagnostics_t *diagnostics
);

const char *sensor_acquisition_state_name(sensor_acquisition_state_t state);

#endif
