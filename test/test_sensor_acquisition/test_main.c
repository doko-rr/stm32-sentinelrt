#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <unity.h>
#include "sensor_acquisition.h"

typedef struct {
    bool initialize_result;
    environmental_sensor_operation_t start_result;
    environmental_sensor_operation_t poll_result;
    environmental_sensor_reading_t reading;
    uint32_t initialize_calls;
    uint32_t start_calls;
    uint32_t poll_calls;
} fake_sensor_t;

static fake_sensor_t fake;
static sensor_acquisition_t service;

static bool adapter_initialize(void *context)
{
    fake_sensor_t *sensor = (fake_sensor_t *)context;
    sensor->initialize_calls++;
    return sensor->initialize_result;
}

static environmental_sensor_operation_t adapter_start(void *context)
{
    fake_sensor_t *sensor = (fake_sensor_t *)context;
    sensor->start_calls++;
    return sensor->start_result;
}

static environmental_sensor_operation_t adapter_poll(
    void *context,
    environmental_sensor_reading_t *reading)
{
    fake_sensor_t *sensor = (fake_sensor_t *)context;
    sensor->poll_calls++;
    *reading = sensor->reading;
    return sensor->poll_result;
}

static environmental_sensor_port_t make_port(void)
{
    environmental_sensor_port_t port = {
        &fake, adapter_initialize, adapter_start, adapter_poll};
    return port;
}

static sensor_acquisition_config_t make_config(void)
{
    sensor_acquisition_config_t config = {
        {-40.0F, 85.0F, 0.0F, 100.0F}, 100U, 20U, 250U};
    return config;
}

static bool initialize_service(uint32_t tick)
{
    environmental_sensor_port_t port = make_port();
    sensor_acquisition_config_t config = make_config();
    return sensor_acquisition_initialize(&service, &port, &config, tick);
}

void setUp(void)
{
    fake = (fake_sensor_t){0};
    service = (sensor_acquisition_t){0};
    fake.initialize_result = true;
    fake.start_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
    fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_PENDING;
    fake.reading.temperature_celsius = 26.0F;
    fake.reading.relative_humidity_percent = 70.0F;
}
void tearDown(void) {}

static void test_initialize_does_not_start_measurement(void)
{
    TEST_ASSERT_TRUE(initialize_service(10U));
    TEST_ASSERT_EQUAL(SENSOR_ACQUISITION_IDLE, service.state);
    TEST_ASSERT_EQUAL_UINT32(0U, fake.start_calls);
}

static void test_process_starts_then_polls_without_blocking(void)
{
    TEST_ASSERT_TRUE(initialize_service(10U));
    sensor_acquisition_process(&service, 10U);
    TEST_ASSERT_EQUAL(SENSOR_ACQUISITION_WAITING_FOR_SAMPLE, service.state);
    TEST_ASSERT_EQUAL_UINT32(1U, fake.start_calls);
    sensor_acquisition_process(&service, 11U);
    TEST_ASSERT_EQUAL_UINT32(1U, fake.poll_calls);
}

static void test_valid_reading_is_retained(void)
{
    TEST_ASSERT_TRUE(initialize_service(0U));
    sensor_acquisition_process(&service, 0U);
    fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
    sensor_acquisition_process(&service, 5U);
    sensor_sample_t latest = sensor_acquisition_latest_result(&service, 5U);
    sensor_sample_t good = sensor_sample_uninitialized();
    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_VALID, latest.status);
    TEST_ASSERT_TRUE(sensor_acquisition_last_known_good(&service, 5U, &good));
}

static void test_invalid_reading_preserves_last_good(void)
{
    TEST_ASSERT_TRUE(initialize_service(0U));
    sensor_acquisition_process(&service, 0U);
    fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
    sensor_acquisition_process(&service, 1U);
    sensor_acquisition_process(&service, 101U);
    fake.reading.relative_humidity_percent = 101.0F;
    sensor_acquisition_process(&service, 102U);
    sensor_sample_t good = sensor_sample_uninitialized();
    TEST_ASSERT_EQUAL(
        SENSOR_SAMPLE_OUT_OF_RANGE,
        sensor_acquisition_latest_result(&service, 102U).status);
    TEST_ASSERT_TRUE(sensor_acquisition_last_known_good(&service, 102U, &good));
    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_VALID, good.status);
}

static void test_timeout_is_wraparound_safe(void)
{
    TEST_ASSERT_TRUE(initialize_service(UINT32_MAX - 5U));
    sensor_acquisition_process(&service, UINT32_MAX - 5U);
    sensor_acquisition_process(&service, 16U);
    TEST_ASSERT_EQUAL(
        SENSOR_SAMPLE_TIMEOUT,
        sensor_acquisition_latest_result(&service, 16U).status);
}

static void test_hardware_error_preserves_last_good(void)
{
    TEST_ASSERT_TRUE(initialize_service(0U));
    sensor_acquisition_process(&service, 0U);
    fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
    sensor_acquisition_process(&service, 1U);
    sensor_acquisition_process(&service, 101U);
    fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    sensor_acquisition_process(&service, 102U);
    sensor_sample_t good = sensor_sample_uninitialized();
    TEST_ASSERT_TRUE(sensor_acquisition_last_known_good(&service, 102U, &good));
}

static void test_last_good_becomes_stale(void)
{
    TEST_ASSERT_TRUE(initialize_service(0U));
    sensor_acquisition_process(&service, 0U);
    fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
    sensor_acquisition_process(&service, 1U);
    sensor_sample_t good = sensor_sample_uninitialized();
    TEST_ASSERT_TRUE(sensor_acquisition_last_known_good(&service, 252U, &good));
    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_STALE, good.status);
}

static void test_diagnostic_counter_saturates(void)
{
    TEST_ASSERT_TRUE(initialize_service(0U));
    service.diagnostics.hardware_failures = UINT32_MAX;
    fake.start_result = ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    sensor_acquisition_process(&service, 0U);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, service.diagnostics.hardware_failures);
}

static void test_health_snapshot_starts_at_zero(void)
{
    sensor_acquisition_diagnostics_t d = {0};
    TEST_ASSERT_TRUE(initialize_service(0U));
    TEST_ASSERT_TRUE(sensor_acquisition_get_diagnostics(&service, &d));
    TEST_ASSERT_EQUAL_UINT32(0U, d.total_attempts);
    TEST_ASSERT_EQUAL_UINT32(0U, d.consecutive_failures);
    TEST_ASSERT_EQUAL_UINT32(0U, d.peak_consecutive_failures);
    TEST_ASSERT_EQUAL_UINT32(0U, d.last_success_sequence);
}

static void test_start_failure_counts_attempt_and_failure_streak(void)
{
    sensor_acquisition_diagnostics_t d = {0};
    TEST_ASSERT_TRUE(initialize_service(0U));
    fake.start_result = ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    sensor_acquisition_process(&service, 0U);
    TEST_ASSERT_TRUE(sensor_acquisition_get_diagnostics(&service, &d));
    TEST_ASSERT_EQUAL_UINT32(1U, d.total_attempts);
    TEST_ASSERT_EQUAL_UINT32(1U, d.hardware_failures);
    TEST_ASSERT_EQUAL_UINT32(1U, d.consecutive_failures);
    TEST_ASSERT_EQUAL_UINT32(1U, d.peak_consecutive_failures);
}

static void test_two_timeouts_track_current_and_peak_streak(void)
{
    sensor_acquisition_diagnostics_t d = {0};
    TEST_ASSERT_TRUE(initialize_service(0U));
    sensor_acquisition_process(&service, 0U);
    sensor_acquisition_process(&service, 21U);
    sensor_acquisition_process(&service, 121U);
    sensor_acquisition_process(&service, 142U);
    TEST_ASSERT_TRUE(sensor_acquisition_get_diagnostics(&service, &d));
    TEST_ASSERT_EQUAL_UINT32(2U, d.timeout_failures);
    TEST_ASSERT_EQUAL_UINT32(2U, d.consecutive_failures);
    TEST_ASSERT_EQUAL_UINT32(2U, d.peak_consecutive_failures);
}

static void test_success_resets_streak_and_records_sequence(void)
{
    sensor_acquisition_diagnostics_t d = {0};
    TEST_ASSERT_TRUE(initialize_service(0U));
    fake.start_result = ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    sensor_acquisition_process(&service, 0U);
    fake.start_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
    fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
    sensor_acquisition_process(&service, 100U);
    sensor_acquisition_process(&service, 101U);
    TEST_ASSERT_TRUE(sensor_acquisition_get_diagnostics(&service, &d));
    TEST_ASSERT_EQUAL_UINT32(1U, d.successful_samples);
    TEST_ASSERT_EQUAL_UINT32(0U, d.consecutive_failures);
    TEST_ASSERT_EQUAL_UINT32(1U, d.peak_consecutive_failures);
    TEST_ASSERT_EQUAL_UINT32(2U, d.last_success_sequence);
}

static void test_out_of_range_counts_as_failure_streak(void)
{
    sensor_acquisition_diagnostics_t d = {0};
    TEST_ASSERT_TRUE(initialize_service(0U));
    fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
    fake.reading.relative_humidity_percent = 101.0F;
    sensor_acquisition_process(&service, 0U);
    sensor_acquisition_process(&service, 1U);
    TEST_ASSERT_TRUE(sensor_acquisition_get_diagnostics(&service, &d));
    TEST_ASSERT_EQUAL_UINT32(1U, d.out_of_range_samples);
    TEST_ASSERT_EQUAL_UINT32(1U, d.consecutive_failures);
}

static void test_health_snapshot_rejects_null_and_is_read_only(void)
{
    sensor_acquisition_diagnostics_t d = {0};
    sensor_acquisition_state_t before;
    TEST_ASSERT_TRUE(initialize_service(0U));
    before = service.state;
    TEST_ASSERT_FALSE(sensor_acquisition_get_diagnostics(NULL, &d));
    TEST_ASSERT_FALSE(sensor_acquisition_get_diagnostics(&service, NULL));
    TEST_ASSERT_TRUE(sensor_acquisition_get_diagnostics(&service, &d));
    TEST_ASSERT_EQUAL(before, service.state);
}

static void test_new_health_counters_saturate(void)
{
    sensor_acquisition_diagnostics_t d = {0};
    TEST_ASSERT_TRUE(initialize_service(0U));
    service.diagnostics.total_attempts = UINT32_MAX;
    service.diagnostics.consecutive_failures = UINT32_MAX;
    service.diagnostics.peak_consecutive_failures = UINT32_MAX;
    fake.start_result = ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    sensor_acquisition_process(&service, 0U);
    TEST_ASSERT_TRUE(sensor_acquisition_get_diagnostics(&service, &d));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, d.total_attempts);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, d.consecutive_failures);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, d.peak_consecutive_failures);
}

static void test_soak_10000_cycles_preserves_health_invariants(void)
{
    const uint32_t cycles = 10000U;
    uint32_t expected_success = 0U;
    uint32_t expected_out_of_range = 0U;
    uint32_t expected_timeout = 0U;
    uint32_t expected_hardware = 0U;
    uint32_t expected_streak = 0U;
    uint32_t expected_peak = 0U;
    uint32_t tick = 0U;
    uint32_t cycle;

    TEST_ASSERT_TRUE(initialize_service(tick));

    for (cycle = 0U; cycle < cycles; cycle++)
    {
        sensor_acquisition_diagnostics_t diagnostics = {0};
        uint32_t scenario = cycle % 10U;

        fake.start_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
        fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
        fake.reading.temperature_celsius = 26.0F;
        fake.reading.relative_humidity_percent = 70.0F;

        if (scenario == 7U)
        {
            fake.reading.relative_humidity_percent = 101.0F;
            expected_out_of_range++;
            expected_streak++;
            sensor_acquisition_process(&service, tick);
            sensor_acquisition_process(&service, tick + 1U);
        }
        else if (scenario == 8U)
        {
            fake.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_PENDING;
            expected_timeout++;
            expected_streak++;
            sensor_acquisition_process(&service, tick);
            sensor_acquisition_process(&service, tick + 21U);
        }
        else if (scenario == 9U)
        {
            fake.start_result = ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
            expected_hardware++;
            expected_streak++;
            sensor_acquisition_process(&service, tick);
        }
        else
        {
            expected_success++;
            expected_streak = 0U;
            sensor_acquisition_process(&service, tick);
            sensor_acquisition_process(&service, tick + 1U);
        }

        if (expected_streak > expected_peak)
        {
            expected_peak = expected_streak;
        }

        TEST_ASSERT_TRUE(sensor_acquisition_get_diagnostics(&service, &diagnostics));
        TEST_ASSERT_EQUAL_UINT32(cycle + 1U, diagnostics.total_attempts);
        TEST_ASSERT_EQUAL_UINT32(expected_success, diagnostics.successful_samples);
        TEST_ASSERT_EQUAL_UINT32(expected_out_of_range, diagnostics.out_of_range_samples);
        TEST_ASSERT_EQUAL_UINT32(expected_timeout, diagnostics.timeout_failures);
        TEST_ASSERT_EQUAL_UINT32(expected_hardware, diagnostics.hardware_failures);
        TEST_ASSERT_EQUAL_UINT32(expected_streak, diagnostics.consecutive_failures);
        TEST_ASSERT_EQUAL_UINT32(expected_peak, diagnostics.peak_consecutive_failures);
        TEST_ASSERT_EQUAL_UINT32(
            diagnostics.total_attempts,
            diagnostics.successful_samples + diagnostics.out_of_range_samples
                + diagnostics.timeout_failures + diagnostics.hardware_failures);

        tick += 121U;
    }

    TEST_ASSERT_EQUAL_UINT32(10000U, service.diagnostics.total_attempts);
    TEST_ASSERT_EQUAL_UINT32(7000U, service.diagnostics.successful_samples);
    TEST_ASSERT_EQUAL_UINT32(1000U, service.diagnostics.out_of_range_samples);
    TEST_ASSERT_EQUAL_UINT32(1000U, service.diagnostics.timeout_failures);
    TEST_ASSERT_EQUAL_UINT32(1000U, service.diagnostics.hardware_failures);
    TEST_ASSERT_EQUAL_UINT32(3U, service.diagnostics.consecutive_failures);
    TEST_ASSERT_EQUAL_UINT32(3U, service.diagnostics.peak_consecutive_failures);
    TEST_ASSERT_EQUAL_UINT32(9997U, service.diagnostics.last_success_sequence);
}

static void test_invalid_configuration_is_rejected(void)
{
    environmental_sensor_port_t port = make_port();
    sensor_acquisition_config_t config = make_config();
    config.measurement_timeout_ticks = 0U;
    TEST_ASSERT_FALSE(sensor_acquisition_initialize(
        &service, &port, &config, 0U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initialize_does_not_start_measurement);
    RUN_TEST(test_process_starts_then_polls_without_blocking);
    RUN_TEST(test_valid_reading_is_retained);
    RUN_TEST(test_invalid_reading_preserves_last_good);
    RUN_TEST(test_timeout_is_wraparound_safe);
    RUN_TEST(test_hardware_error_preserves_last_good);
    RUN_TEST(test_last_good_becomes_stale);
    RUN_TEST(test_diagnostic_counter_saturates);
    RUN_TEST(test_health_snapshot_starts_at_zero);
    RUN_TEST(test_start_failure_counts_attempt_and_failure_streak);
    RUN_TEST(test_two_timeouts_track_current_and_peak_streak);
    RUN_TEST(test_success_resets_streak_and_records_sequence);
    RUN_TEST(test_out_of_range_counts_as_failure_streak);
    RUN_TEST(test_health_snapshot_rejects_null_and_is_read_only);
    RUN_TEST(test_new_health_counters_saturate);
    RUN_TEST(test_soak_10000_cycles_preserves_health_invariants);
    RUN_TEST(test_invalid_configuration_is_rejected);
    return UNITY_END();
}
