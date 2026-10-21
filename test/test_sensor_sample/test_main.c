#include <math.h>
#include <stdint.h>

#include <unity.h>

#include "sensor_sample.h"

static const sensor_sample_limits_t environmental_limits = {
    .minimum_temperature_celsius = -40.0F,
    .maximum_temperature_celsius = 85.0F,
    .minimum_relative_humidity_percent = 0.0F,
    .maximum_relative_humidity_percent = 100.0F,
};

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_uninitialized_sample_is_deterministic(void)
{
    sensor_sample_t sample = sensor_sample_uninitialized();

    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_NOT_INITIALIZED, sample.status);
    TEST_ASSERT_EQUAL_UINT32(0U, sample.captured_at_tick);
    TEST_ASSERT_EQUAL_UINT32(0U, sample.sequence);
    TEST_ASSERT_FALSE(sensor_sample_is_usable(&sample));
}

static void test_valid_measurements_produce_usable_sample(void)
{
    sensor_sample_t sample = sensor_sample_create(
        26.5F,
        72.0F,
        100U,
        7U,
        &environmental_limits
    );

    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_VALID, sample.status);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 26.5F, sample.temperature_celsius);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001F,
        72.0F,
        sample.relative_humidity_percent
    );
    TEST_ASSERT_TRUE(sensor_sample_is_usable(&sample));
}

static void test_out_of_range_measurement_is_rejected(void)
{
    sensor_sample_t sample = sensor_sample_create(
        26.5F,
        101.0F,
        100U,
        8U,
        &environmental_limits
    );

    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_OUT_OF_RANGE, sample.status);
    TEST_ASSERT_FALSE(sensor_sample_is_usable(&sample));
}

static void test_non_finite_measurement_is_rejected(void)
{
    sensor_sample_t sample = sensor_sample_create(
        NAN,
        50.0F,
        100U,
        9U,
        &environmental_limits
    );

    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_OUT_OF_RANGE, sample.status);
}

static void test_invalid_limits_are_rejected(void)
{
    sensor_sample_limits_t reversed_limits = environmental_limits;
    reversed_limits.minimum_temperature_celsius = 90.0F;
    reversed_limits.maximum_temperature_celsius = -40.0F;

    sensor_sample_t sample = sensor_sample_create(
        25.0F,
        50.0F,
        100U,
        10U,
        &reversed_limits
    );

    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_OUT_OF_RANGE, sample.status);
}

static void test_timeout_does_not_invent_measurements(void)
{
    sensor_sample_t sample = sensor_sample_failure(
        SENSOR_SAMPLE_TIMEOUT,
        120U,
        11U
    );

    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_TIMEOUT, sample.status);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.0F, sample.temperature_celsius);
    TEST_ASSERT_FALSE(sensor_sample_is_usable(&sample));
}

static void test_freshness_marks_only_old_valid_sample_as_stale(void)
{
    sensor_sample_t sample = sensor_sample_create(
        25.0F,
        60.0F,
        100U,
        12U,
        &environmental_limits
    );

    sensor_sample_t fresh = sensor_sample_apply_freshness(
        &sample,
        150U,
        50U
    );
    sensor_sample_t stale = sensor_sample_apply_freshness(
        &sample,
        151U,
        50U
    );

    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_VALID, fresh.status);
    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_STALE, stale.status);
}

static void test_age_calculation_handles_tick_wraparound(void)
{
    sensor_sample_t sample = sensor_sample_create(
        25.0F,
        60.0F,
        UINT32_MAX - 4U,
        13U,
        &environmental_limits
    );

    /* Five ticks to wrap plus three ticks after wrap equals eight ticks. */
    TEST_ASSERT_EQUAL_UINT32(8U, sensor_sample_age_ticks(&sample, 3U));
}

static void test_failure_status_is_not_replaced_by_stale(void)
{
    sensor_sample_t timeout = sensor_sample_failure(
        SENSOR_SAMPLE_TIMEOUT,
        100U,
        14U
    );

    sensor_sample_t result = sensor_sample_apply_freshness(
        &timeout,
        1000U,
        50U
    );

    TEST_ASSERT_EQUAL(SENSOR_SAMPLE_TIMEOUT, result.status);
}

static void test_status_names_support_diagnostics(void)
{
    TEST_ASSERT_EQUAL_STRING(
        "HARDWARE_ERROR",
        sensor_sample_status_name(SENSOR_SAMPLE_HARDWARE_ERROR)
    );
    TEST_ASSERT_EQUAL_STRING(
        "UNKNOWN",
        sensor_sample_status_name((sensor_sample_status_t)99)
    );
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uninitialized_sample_is_deterministic);
    RUN_TEST(test_valid_measurements_produce_usable_sample);
    RUN_TEST(test_out_of_range_measurement_is_rejected);
    RUN_TEST(test_non_finite_measurement_is_rejected);
    RUN_TEST(test_invalid_limits_are_rejected);
    RUN_TEST(test_timeout_does_not_invent_measurements);
    RUN_TEST(test_freshness_marks_only_old_valid_sample_as_stale);
    RUN_TEST(test_age_calculation_handles_tick_wraparound);
    RUN_TEST(test_failure_status_is_not_replaced_by_stale);
    RUN_TEST(test_status_names_support_diagnostics);
    return UNITY_END();
}
