#include "sensor_acquisition.h"

#include <limits.h>
#include <stddef.h>

static void increment_saturating(uint32_t *counter)
{
    if (counter != NULL && *counter < UINT32_MAX)
    {
        /* Saturation prevents long-running diagnostics from wrapping to zero. */
        (*counter)++;
    }
}

static void record_failure_streak(sensor_acquisition_t *service)
{
    increment_saturating(&service->diagnostics.consecutive_failures);
    if (service->diagnostics.consecutive_failures
        > service->diagnostics.peak_consecutive_failures)
    {
        service->diagnostics.peak_consecutive_failures =
            service->diagnostics.consecutive_failures;
    }
}

static void record_success(sensor_acquisition_t *service, uint32_t sequence)
{
    increment_saturating(&service->diagnostics.successful_samples);
    service->diagnostics.consecutive_failures = 0U;
    service->diagnostics.last_success_sequence = sequence;
}

static bool config_is_valid(const sensor_acquisition_config_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    /* Signed deadline comparison is unambiguous below half the tick range. */
    return config->measurement_interval_ticks > 0U
        && config->measurement_interval_ticks <= INT32_MAX
        && config->measurement_timeout_ticks > 0U
        && config->measurement_timeout_ticks <= INT32_MAX;
}

static bool deadline_reached(uint32_t now, uint32_t deadline)
{
    /* Signed delta comparison remains correct across uint32 tick wraparound. */
    return (int32_t)(now - deadline) >= 0;
}

static uint32_t allocate_sequence(sensor_acquisition_t *service)
{
    uint32_t sequence = service->next_sequence;
    if (service->next_sequence < UINT32_MAX)
    {
        service->next_sequence++;
    }
    return sequence;
}

static void schedule_next(sensor_acquisition_t *service, uint32_t now)
{
    service->next_measurement_at_tick =
        now + service->config.measurement_interval_ticks;
    service->state = SENSOR_ACQUISITION_IDLE;
}

static void record_failure(
    sensor_acquisition_t *service,
    sensor_sample_status_t status,
    uint32_t now
)
{
    service->latest_result = sensor_sample_failure(
        status, now, allocate_sequence(service));

    if (status == SENSOR_SAMPLE_TIMEOUT)
    {
        increment_saturating(&service->diagnostics.timeout_failures);
    }
    else
    {
        increment_saturating(&service->diagnostics.hardware_failures);
    }
    record_failure_streak(service);
    schedule_next(service, now);
}

static void begin_measurement(sensor_acquisition_t *service, uint32_t now)
{
    environmental_sensor_operation_t result;

    increment_saturating(&service->diagnostics.total_attempts);
    result = environmental_sensor_port_start_measurement(&service->sensor_port);

    if (result == ENVIRONMENTAL_SENSOR_OPERATION_OK
        || result == ENVIRONMENTAL_SENSOR_OPERATION_PENDING)
    {
        service->measurement_started_at_tick = now;
        service->state = SENSOR_ACQUISITION_WAITING_FOR_SAMPLE;
        return;
    }

    record_failure(
        service,
        result == ENVIRONMENTAL_SENSOR_OPERATION_TIMEOUT
            ? SENSOR_SAMPLE_TIMEOUT
            : SENSOR_SAMPLE_HARDWARE_ERROR,
        now);
}

static void accept_reading(
    sensor_acquisition_t *service,
    const environmental_sensor_reading_t *reading,
    uint32_t now
)
{
    sensor_sample_t sample = sensor_sample_create(
        reading->temperature_celsius,
        reading->relative_humidity_percent,
        now,
        allocate_sequence(service),
        &service->config.limits);

    service->latest_result = sample;
    if (sample.status == SENSOR_SAMPLE_VALID)
    {
        service->last_known_good_sample = sample;
        service->has_last_known_good_sample = true;
        record_success(service, sample.sequence);
    }
    else
    {
        /* An invalid attempt stays visible but never replaces trusted history. */
        increment_saturating(&service->diagnostics.out_of_range_samples);
        record_failure_streak(service);
    }
    schedule_next(service, now);
}

static void poll_measurement(sensor_acquisition_t *service, uint32_t now)
{
    uint32_t elapsed = now - service->measurement_started_at_tick;
    if (elapsed > service->config.measurement_timeout_ticks)
    {
        record_failure(service, SENSOR_SAMPLE_TIMEOUT, now);
        return;
    }

    environmental_sensor_reading_t reading = {0};
    environmental_sensor_operation_t result =
        environmental_sensor_port_poll_measurement(
            &service->sensor_port, &reading);

    if (result == ENVIRONMENTAL_SENSOR_OPERATION_PENDING
        || result == ENVIRONMENTAL_SENSOR_OPERATION_NOT_READY)
    {
        return;
    }
    if (result == ENVIRONMENTAL_SENSOR_OPERATION_OK)
    {
        accept_reading(service, &reading, now);
        return;
    }

    record_failure(
        service,
        result == ENVIRONMENTAL_SENSOR_OPERATION_TIMEOUT
            ? SENSOR_SAMPLE_TIMEOUT
            : SENSOR_SAMPLE_HARDWARE_ERROR,
        now);
}

bool sensor_acquisition_initialize(
    sensor_acquisition_t *service,
    const environmental_sensor_port_t *sensor_port,
    const sensor_acquisition_config_t *config,
    uint32_t current_tick)
{
    if (service == NULL
        || !environmental_sensor_port_is_valid(sensor_port)
        || !config_is_valid(config))
    {
        return false;
    }

    *service = (sensor_acquisition_t){0};
    service->sensor_port = *sensor_port;
    service->config = *config;
    service->latest_result = sensor_sample_uninitialized();
    service->last_known_good_sample = sensor_sample_uninitialized();
    service->state = SENSOR_ACQUISITION_UNINITIALIZED;
    service->next_sequence = 1U;

    if (!environmental_sensor_port_initialize(&service->sensor_port))
    {
        return false;
    }

    service->state = SENSOR_ACQUISITION_IDLE;
    service->next_measurement_at_tick = current_tick;
    return true;
}

void sensor_acquisition_process(sensor_acquisition_t *service, uint32_t now)
{
    if (service == NULL
        || service->state == SENSOR_ACQUISITION_UNINITIALIZED)
    {
        return;
    }

    if (service->state == SENSOR_ACQUISITION_WAITING_FOR_SAMPLE)
    {
        poll_measurement(service, now);
        return;
    }

    if (deadline_reached(now, service->next_measurement_at_tick))
    {
        begin_measurement(service, now);
    }
}

sensor_sample_t sensor_acquisition_latest_result(
    const sensor_acquisition_t *service,
    uint32_t now)
{
    if (service == NULL)
    {
        return sensor_sample_uninitialized();
    }
    return sensor_sample_apply_freshness(
        &service->latest_result, now, service->config.stale_after_ticks);
}

bool sensor_acquisition_last_known_good(
    const sensor_acquisition_t *service,
    uint32_t now,
    sensor_sample_t *sample)
{
    if (service == NULL || sample == NULL
        || !service->has_last_known_good_sample)
    {
        return false;
    }
    *sample = sensor_sample_apply_freshness(
        &service->last_known_good_sample,
        now,
        service->config.stale_after_ticks);
    return true;
}

bool sensor_acquisition_get_diagnostics(
    const sensor_acquisition_t *service,
    sensor_acquisition_diagnostics_t *diagnostics)
{
    if (service == NULL || diagnostics == NULL)
    {
        return false;
    }
    *diagnostics = service->diagnostics;
    return true;
}

const char *sensor_acquisition_state_name(sensor_acquisition_state_t state)
{
    switch (state)
    {
        case SENSOR_ACQUISITION_UNINITIALIZED: return "UNINITIALIZED";
        case SENSOR_ACQUISITION_IDLE: return "IDLE";
        case SENSOR_ACQUISITION_WAITING_FOR_SAMPLE: return "WAITING_FOR_SAMPLE";
        default: return "UNKNOWN";
    }
}
