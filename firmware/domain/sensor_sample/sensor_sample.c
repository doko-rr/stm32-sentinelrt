#include "sensor_sample.h"

#include <math.h>
#include <stddef.h>

static bool limits_are_valid(const sensor_sample_limits_t *limits)
{
    if (limits == NULL)
    {
        return false;
    }

    return isfinite(limits->minimum_temperature_celsius)
        && isfinite(limits->maximum_temperature_celsius)
        && isfinite(limits->minimum_relative_humidity_percent)
        && isfinite(limits->maximum_relative_humidity_percent)
        && limits->minimum_temperature_celsius
            <= limits->maximum_temperature_celsius
        && limits->minimum_relative_humidity_percent
            <= limits->maximum_relative_humidity_percent;
}

static bool measurements_are_in_range(
    float temperature_celsius,
    float relative_humidity_percent,
    const sensor_sample_limits_t *limits
)
{
    if (!isfinite(temperature_celsius)
        || !isfinite(relative_humidity_percent))
    {
        return false;
    }

    return temperature_celsius >= limits->minimum_temperature_celsius
        && temperature_celsius <= limits->maximum_temperature_celsius
        && relative_humidity_percent
            >= limits->minimum_relative_humidity_percent
        && relative_humidity_percent
            <= limits->maximum_relative_humidity_percent;
}

sensor_sample_t sensor_sample_uninitialized(void)
{
    const sensor_sample_t sample = {
        .temperature_celsius = 0.0F,
        .relative_humidity_percent = 0.0F,
        .captured_at_tick = 0U,
        .sequence = 0U,
        .status = SENSOR_SAMPLE_NOT_INITIALIZED,
    };

    return sample;
}

sensor_sample_t sensor_sample_create(
    float temperature_celsius,
    float relative_humidity_percent,
    uint32_t captured_at_tick,
    uint32_t sequence,
    const sensor_sample_limits_t *limits
)
{
    sensor_sample_t sample = {
        .temperature_celsius = temperature_celsius,
        .relative_humidity_percent = relative_humidity_percent,
        .captured_at_tick = captured_at_tick,
        .sequence = sequence,
        .status = SENSOR_SAMPLE_OUT_OF_RANGE,
    };

    if (limits_are_valid(limits)
        && measurements_are_in_range(
            temperature_celsius,
            relative_humidity_percent,
            limits))
    {
        sample.status = SENSOR_SAMPLE_VALID;
    }

    return sample;
}

sensor_sample_t sensor_sample_failure(
    sensor_sample_status_t failure_status,
    uint32_t observed_at_tick,
    uint32_t sequence
)
{
    sensor_sample_t sample = sensor_sample_uninitialized();

    sample.captured_at_tick = observed_at_tick;
    sample.sequence = sequence;

    if (failure_status == SENSOR_SAMPLE_TIMEOUT
        || failure_status == SENSOR_SAMPLE_HARDWARE_ERROR)
    {
        sample.status = failure_status;
    }
    else
    {
        /* Normalization prevents callers from constructing a valid sample
         * without supplying actual measurements and validation limits. */
        sample.status = SENSOR_SAMPLE_HARDWARE_ERROR;
    }

    return sample;
}

uint32_t sensor_sample_age_ticks(
    const sensor_sample_t *sample,
    uint32_t current_tick
)
{
    if (sample == NULL)
    {
        return 0U;
    }

    /* Unsigned subtraction intentionally handles monotonic tick wraparound. */
    return current_tick - sample->captured_at_tick;
}

sensor_sample_t sensor_sample_apply_freshness(
    const sensor_sample_t *sample,
    uint32_t current_tick,
    uint32_t stale_after_ticks
)
{
    if (sample == NULL)
    {
        return sensor_sample_uninitialized();
    }

    sensor_sample_t result = *sample;

    if (result.status == SENSOR_SAMPLE_VALID
        && sensor_sample_age_ticks(&result, current_tick) > stale_after_ticks)
    {
        result.status = SENSOR_SAMPLE_STALE;
    }

    return result;
}

bool sensor_sample_is_usable(const sensor_sample_t *sample)
{
    return sample != NULL && sample->status == SENSOR_SAMPLE_VALID;
}

const char *sensor_sample_status_name(sensor_sample_status_t status)
{
    switch (status)
    {
        case SENSOR_SAMPLE_NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        case SENSOR_SAMPLE_VALID:
            return "VALID";
        case SENSOR_SAMPLE_OUT_OF_RANGE:
            return "OUT_OF_RANGE";
        case SENSOR_SAMPLE_TIMEOUT:
            return "TIMEOUT";
        case SENSOR_SAMPLE_STALE:
            return "STALE";
        case SENSOR_SAMPLE_HARDWARE_ERROR:
            return "HARDWARE_ERROR";
        default:
            return "UNKNOWN";
    }
}
