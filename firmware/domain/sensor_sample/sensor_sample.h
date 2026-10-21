#ifndef SENTINELRT_SENSOR_SAMPLE_H
#define SENTINELRT_SENSOR_SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Describes whether a sensor sample can be trusted by the application.
 *
 * The domain uses explicit states instead of a boolean so diagnostics can
 * distinguish invalid values, communication failures, and aged data.
 */
typedef enum
{
    SENSOR_SAMPLE_NOT_INITIALIZED = 0,
    SENSOR_SAMPLE_VALID,
    SENSOR_SAMPLE_OUT_OF_RANGE,
    SENSOR_SAMPLE_TIMEOUT,
    SENSOR_SAMPLE_STALE,
    SENSOR_SAMPLE_HARDWARE_ERROR
} sensor_sample_status_t;

/**
 * Defines acceptable environmental ranges in physical units.
 *
 * Limits are supplied by configuration because valid ranges depend on the
 * installed sensor and deployment, not on the generic domain model.
 */
typedef struct
{
    float minimum_temperature_celsius;
    float maximum_temperature_celsius;
    float minimum_relative_humidity_percent;
    float maximum_relative_humidity_percent;
} sensor_sample_limits_t;

/**
 * Represents one environmental observation.
 *
 * captured_at_tick uses a monotonic platform tick. Calendar time is avoided
 * because freshness checks must work before an RTC or network clock is ready.
 */
typedef struct
{
    float temperature_celsius;
    float relative_humidity_percent;
    uint32_t captured_at_tick;
    uint32_t sequence;
    sensor_sample_status_t status;
} sensor_sample_t;

/** Returns a deterministic sample that is safe before sensor initialization. */
sensor_sample_t sensor_sample_uninitialized(void);

/**
 * Creates and validates a sample against the supplied physical limits.
 *
 * Non-finite values and invalid limit configurations produce OUT_OF_RANGE.
 * This function is deterministic and does not access hardware or RTOS APIs.
 */
sensor_sample_t sensor_sample_create(
    float temperature_celsius,
    float relative_humidity_percent,
    uint32_t captured_at_tick,
    uint32_t sequence,
    const sensor_sample_limits_t *limits
);

/**
 * Creates a failure result without inventing measurement values.
 *
 * Only TIMEOUT and HARDWARE_ERROR are accepted. Other states are normalized
 * to HARDWARE_ERROR so callers cannot accidentally report a false reading.
 */
sensor_sample_t sensor_sample_failure(
    sensor_sample_status_t failure_status,
    uint32_t observed_at_tick,
    uint32_t sequence
);

/**
 * Returns elapsed ticks using unsigned wraparound-safe arithmetic.
 *
 * The result is valid when elapsed durations remain below UINT32_MAX ticks,
 * which is the normal contract for embedded monotonic timers.
 */
uint32_t sensor_sample_age_ticks(
    const sensor_sample_t *sample,
    uint32_t current_tick
);

/**
 * Returns a copy marked STALE when a previously valid sample is too old.
 *
 * A threshold of zero means data becomes stale as soon as time advances.
 * Failure and uninitialized states are preserved because freshness must not
 * hide the original diagnostic cause.
 */
sensor_sample_t sensor_sample_apply_freshness(
    const sensor_sample_t *sample,
    uint32_t current_tick,
    uint32_t stale_after_ticks
);

bool sensor_sample_is_usable(const sensor_sample_t *sample);

const char *sensor_sample_status_name(sensor_sample_status_t status);

#endif
