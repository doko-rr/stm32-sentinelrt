#include <string.h>

#include <unity.h>

#include "environmental_sensor_port.h"
#include "stm32_dht22_adapter.h"

typedef struct
{
    uint32_t now_us;
    bool initialize_ok;
    bool drive_low_ok;
    bool release_ok;
    bool enable_ok;
    unsigned initialize_calls;
    unsigned drive_low_calls;
    unsigned release_calls;
    unsigned enable_calls;
    unsigned disable_calls;
} fake_hardware_t;

static fake_hardware_t fake;
static stm32_dht22_adapter_t adapter;
static environmental_sensor_port_t port;

void setUp(void)
{
    memset(&fake, 0, sizeof(fake));
    memset(&adapter, 0, sizeof(adapter));
    fake.initialize_ok = true;
    fake.drive_low_ok = true;
    fake.release_ok = true;
    fake.enable_ok = true;
}

void tearDown(void)
{
}

static bool fake_initialize(void *context)
{
    fake_hardware_t *hardware = context;
    hardware->initialize_calls++;
    return hardware->initialize_ok;
}

static uint32_t fake_now_us(void *context)
{
    return ((fake_hardware_t *)context)->now_us;
}

static bool fake_drive_low(void *context)
{
    fake_hardware_t *hardware = context;
    hardware->drive_low_calls++;
    return hardware->drive_low_ok;
}

static bool fake_release(void *context)
{
    fake_hardware_t *hardware = context;
    hardware->release_calls++;
    return hardware->release_ok;
}

static bool fake_enable(void *context)
{
    fake_hardware_t *hardware = context;
    hardware->enable_calls++;
    return hardware->enable_ok;
}

static void fake_disable(void *context)
{
    ((fake_hardware_t *)context)->disable_calls++;
}

static stm32_dht22_hardware_t hardware_operations(void)
{
    stm32_dht22_hardware_t hardware = {
        &fake,
        fake_initialize,
        fake_now_us,
        fake_drive_low,
        fake_release,
        fake_enable,
        fake_disable
    };

    return hardware;
}

static void prepare_initialized_adapter(void)
{
    stm32_dht22_hardware_t hardware = hardware_operations();

    TEST_ASSERT_TRUE(stm32_dht22_adapter_configure(&adapter, &hardware));
    port = stm32_dht22_adapter_port(&adapter);
    TEST_ASSERT_TRUE(environmental_sensor_port_initialize(&port));
}

static void advance_to_capture(void)
{
    environmental_sensor_reading_t reading = {0.0F, 0.0F};

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_OK,
        environmental_sensor_port_start_measurement(&port)
    );
    fake.now_us += STM32_DHT22_START_LOW_US;
    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_PENDING,
        environmental_sensor_port_poll_measurement(&port, &reading)
    );
}

static void emit_bit(uint8_t value)
{
    stm32_dht22_adapter_on_edge(&adapter, fake.now_us, true);
    fake.now_us += value != 0U ? 70U : 28U;
    stm32_dht22_adapter_on_edge(&adapter, fake.now_us, false);
    fake.now_us += 20U;
}

static void emit_frame(const uint8_t frame[5])
{
    size_t byte_index;
    int bit_index;

    for (byte_index = 0U; byte_index < 5U; ++byte_index)
    {
        for (bit_index = 7; bit_index >= 0; --bit_index)
        {
            emit_bit((uint8_t)((frame[byte_index] >> bit_index) & 1U));
        }
    }
}

static void test_rejects_invalid_hardware(void)
{
    stm32_dht22_hardware_t hardware = hardware_operations();

    hardware.now_us = NULL;
    TEST_ASSERT_FALSE(stm32_dht22_adapter_configure(&adapter, &hardware));
}

static void test_initialize_does_not_measure(void)
{
    prepare_initialized_adapter();

    TEST_ASSERT_EQUAL_UINT(1U, fake.initialize_calls);
    TEST_ASSERT_EQUAL_UINT(0U, fake.drive_low_calls);
    TEST_ASSERT_EQUAL(STM32_DHT22_IDLE, stm32_dht22_adapter_state(&adapter));
}

static void test_start_is_non_blocking(void)
{
    prepare_initialized_adapter();

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_OK,
        environmental_sensor_port_start_measurement(&port)
    );
    TEST_ASSERT_EQUAL_UINT(1U, fake.drive_low_calls);
    TEST_ASSERT_EQUAL(STM32_DHT22_START_LOW, stm32_dht22_adapter_state(&adapter));
}

static void test_deadline_releases_line(void)
{
    environmental_sensor_reading_t reading = {1.0F, 2.0F};

    prepare_initialized_adapter();
    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_OK,
        environmental_sensor_port_start_measurement(&port)
    );
    fake.now_us = STM32_DHT22_START_LOW_US;
    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_PENDING,
        environmental_sensor_port_poll_measurement(&port, &reading)
    );
    TEST_ASSERT_EQUAL_UINT(1U, fake.release_calls);
    TEST_ASSERT_EQUAL_UINT(1U, fake.enable_calls);
}

static void test_valid_frame_updates_reading(void)
{
    const uint8_t frame[5] = {0x02U, 0x2BU, 0x00U, 0xF3U, 0x20U};
    environmental_sensor_reading_t reading = {-1.0F, -1.0F};

    prepare_initialized_adapter();
    advance_to_capture();
    emit_frame(frame);

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_OK,
        environmental_sensor_port_poll_measurement(&port, &reading)
    );
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 24.3F, reading.temperature_celsius);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 55.5F, reading.relative_humidity_percent);
}

static void test_bad_checksum_preserves_reading(void)
{
    const uint8_t frame[5] = {0x02U, 0x2BU, 0x00U, 0xF3U, 0x21U};
    environmental_sensor_reading_t reading = {12.5F, 33.0F};

    prepare_initialized_adapter();
    advance_to_capture();
    emit_frame(frame);

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR,
        environmental_sensor_port_poll_measurement(&port, &reading)
    );
    TEST_ASSERT_EQUAL_FLOAT(12.5F, reading.temperature_celsius);
    TEST_ASSERT_EQUAL_FLOAT(33.0F, reading.relative_humidity_percent);
}

static void test_invalid_pulse_preserves_reading(void)
{
    environmental_sensor_reading_t reading = {17.0F, 41.0F};
    unsigned index;

    prepare_initialized_adapter();
    advance_to_capture();
    for (index = 0U; index < STM32_DHT22_DATA_BITS; ++index)
    {
        stm32_dht22_adapter_on_edge(&adapter, fake.now_us, true);
        fake.now_us += index == 10U ? 50U : 28U;
        stm32_dht22_adapter_on_edge(&adapter, fake.now_us, false);
    }

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR,
        environmental_sensor_port_poll_measurement(&port, &reading)
    );
    TEST_ASSERT_EQUAL_FLOAT(17.0F, reading.temperature_celsius);
    TEST_ASSERT_EQUAL_FLOAT(41.0F, reading.relative_humidity_percent);
}

static void test_timeout_allows_immediate_recovery(void)
{
    environmental_sensor_reading_t reading = {7.0F, 8.0F};

    prepare_initialized_adapter();
    advance_to_capture();
    fake.now_us += STM32_DHT22_TIMEOUT_US;

    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_TIMEOUT,
        environmental_sensor_port_poll_measurement(&port, &reading)
    );
    TEST_ASSERT_EQUAL(STM32_DHT22_IDLE, stm32_dht22_adapter_state(&adapter));
    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_OK,
        environmental_sensor_port_start_measurement(&port)
    );
    TEST_ASSERT_EQUAL_FLOAT(7.0F, reading.temperature_celsius);
    TEST_ASSERT_EQUAL_FLOAT(8.0F, reading.relative_humidity_percent);
}

static void test_timestamp_wraparound_is_safe(void)
{
    environmental_sensor_reading_t reading = {0.0F, 0.0F};

    prepare_initialized_adapter();
    fake.now_us = UINT32_MAX - 500U;
    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_OK,
        environmental_sensor_port_start_measurement(&port)
    );
    fake.now_us += STM32_DHT22_START_LOW_US;
    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_PENDING,
        environmental_sensor_port_poll_measurement(&port, &reading)
    );
    TEST_ASSERT_EQUAL(
        STM32_DHT22_CAPTURING,
        stm32_dht22_adapter_state(&adapter)
    );
}

static void test_hardware_failure_allows_immediate_recovery(void)
{
    environmental_sensor_reading_t reading = {4.0F, 5.0F};

    prepare_initialized_adapter();
    fake.drive_low_ok = false;
    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR,
        environmental_sensor_port_start_measurement(&port)
    );
    TEST_ASSERT_EQUAL(STM32_DHT22_IDLE, stm32_dht22_adapter_state(&adapter));

    fake.drive_low_ok = true;
    TEST_ASSERT_EQUAL(
        ENVIRONMENTAL_SENSOR_OPERATION_OK,
        environmental_sensor_port_start_measurement(&port)
    );
    TEST_ASSERT_EQUAL_FLOAT(4.0F, reading.temperature_celsius);
    TEST_ASSERT_EQUAL_FLOAT(5.0F, reading.relative_humidity_percent);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rejects_invalid_hardware);
    RUN_TEST(test_initialize_does_not_measure);
    RUN_TEST(test_start_is_non_blocking);
    RUN_TEST(test_deadline_releases_line);
    RUN_TEST(test_valid_frame_updates_reading);
    RUN_TEST(test_bad_checksum_preserves_reading);
    RUN_TEST(test_invalid_pulse_preserves_reading);
    RUN_TEST(test_timeout_allows_immediate_recovery);
    RUN_TEST(test_timestamp_wraparound_is_safe);
    RUN_TEST(test_hardware_failure_allows_immediate_recovery);
    return UNITY_END();
}
