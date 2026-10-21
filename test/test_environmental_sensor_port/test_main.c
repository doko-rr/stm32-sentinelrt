#include <stdbool.h>
#include <stdint.h>

#include <unity.h>

#include "environmental_sensor_port.h"

typedef struct
{
    bool initialize_result;
    environmental_sensor_operation_t start_result;
    environmental_sensor_operation_t poll_result;
    environmental_sensor_reading_t next_reading;
    uint32_t initialize_calls;
    uint32_t start_calls;
    uint32_t poll_calls;
} fake_environmental_sensor_t;

static fake_environmental_sensor_t fake_sensor;

static bool fake_initialize(void *context)
{
    fake_environmental_sensor_t *fake =
        (fake_environmental_sensor_t *)context;
    fake->initialize_calls++;
    return fake->initialize_result;
}

static environmental_sensor_operation_t fake_start(void *context)
{
    fake_environmental_sensor_t *fake =
        (fake_environmental_sensor_t *)context;
    fake->start_calls++;
    return fake->start_result;
}

static environmental_sensor_operation_t fake_poll(
    void *context,
    environmental_sensor_reading_t *reading
)
{
    fake_environmental_sensor_t *fake =
        (fake_environmental_sensor_t *)context;
    fake->poll_calls++;

    /* The fake writes on every poll to verify that the port protects callers
     * from adapter output until the operation is explicitly successful. */
    *reading = fake->next_reading;
    return fake->poll_result;
}

static environmental_sensor_port_t make_fake_port(void)
{
    environmental_sensor_port_t port = {
        .context = &fake_sensor,
        .initialize = fake_initialize,
        .start_measurement = fake_start,
        .poll_measurement = fake_poll,
    };

    return port;
}

void setUp(void)
{
    fake_sensor.initialize_result = true;
    fake_sensor.start_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;
    fake_sensor.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_PENDING;
    fake_sensor.next_reading.temperature_celsius = 26.5F;
    fake_sensor.next_reading.relative_humidity_percent = 70.0F;
    fake_sensor.initialize_calls = 0U;
    fake_sensor.start_calls = 0U;
    fake_sensor.poll_calls = 0U;
}

void tearDown(void)
{
}

static void test_valid_port_requires_all_callbacks(void)
{
    environmental_sensor_port_t port = make_fake_port();

    TEST_ASSERT_TRUE(environmental_sensor_port_is_valid(&port));

    port.initialize = NULL;
    TEST_ASSERT_FALSE(environmental_sensor_port_is_valid(&port));

    port = make_fake_port();
    port.start_measurement = NULL;
    TEST_ASSERT_FALSE(environmental_sensor_port_is_valid(&port));

    port = make_fake_port();
    port.poll_measurement = NULL;
    TEST_ASSERT_FALSE(environmental_sensor_port_is_valid(&port));

    TEST_ASSERT_FALSE(environmental_sensor_port_is_valid(NULL));
}

static void test_initialize_delegates_to_adapter_context(void)
{
    environmental_sensor_port_t port = make_fake_port();

    TEST_ASSERT_TRUE(environmental_sensor_port_initialize(&port));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_sensor.initialize_calls);
}

static void test_start_measurement_returns_adapter_status(void)
{
    environmental_sensor_port_t port = make_fake_port();
    fake_sensor.start_result = ENVIRONMENTAL_SENSOR_OPERATION_NOT_READY;

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_NOT_READY,
        environmental_sensor_port_start_measurement(&port)
    );
    TEST_ASSERT_EQUAL_UINT32(1U, fake_sensor.start_calls);
}

static void test_pending_poll_preserves_previous_reading(void)
{
    environmental_sensor_port_t port = make_fake_port();
    environmental_sensor_reading_t reading = {
        .temperature_celsius = 10.0F,
        .relative_humidity_percent = 20.0F,
    };

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_PENDING,
        environmental_sensor_port_poll_measurement(&port, &reading)
    );
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 10.0F, reading.temperature_celsius);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001F,
        20.0F,
        reading.relative_humidity_percent
    );
}

static void test_successful_poll_copies_complete_reading(void)
{
    environmental_sensor_port_t port = make_fake_port();
    environmental_sensor_reading_t reading = {0};
    fake_sensor.poll_result = ENVIRONMENTAL_SENSOR_OPERATION_OK;

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_OK,
        environmental_sensor_port_poll_measurement(&port, &reading)
    );
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 26.5F, reading.temperature_celsius);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001F,
        70.0F,
        reading.relative_humidity_percent
    );
    TEST_ASSERT_EQUAL_UINT32(1U, fake_sensor.poll_calls);
}

static void test_invalid_arguments_do_not_call_adapter(void)
{
    environmental_sensor_port_t port = make_fake_port();

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_INVALID_ARGUMENT,
        environmental_sensor_port_start_measurement(NULL)
    );
    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_INVALID_ARGUMENT,
        environmental_sensor_port_poll_measurement(&port, NULL)
    );
    TEST_ASSERT_EQUAL_UINT32(0U, fake_sensor.start_calls);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_sensor.poll_calls);
}

static void test_operation_names_support_diagnostics(void)
{
    TEST_ASSERT_EQUAL_STRING(
        "PENDING",
        environmental_sensor_operation_name(
            ENVIRONMENTAL_SENSOR_OPERATION_PENDING
        )
    );
    TEST_ASSERT_EQUAL_STRING(
        "UNKNOWN",
        environmental_sensor_operation_name(
            (environmental_sensor_operation_t)99
        )
    );
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_port_requires_all_callbacks);
    RUN_TEST(test_initialize_delegates_to_adapter_context);
    RUN_TEST(test_start_measurement_returns_adapter_status);
    RUN_TEST(test_pending_poll_preserves_previous_reading);
    RUN_TEST(test_successful_poll_copies_complete_reading);
    RUN_TEST(test_invalid_arguments_do_not_call_adapter);
    RUN_TEST(test_operation_names_support_diagnostics);
    return UNITY_END();
}
