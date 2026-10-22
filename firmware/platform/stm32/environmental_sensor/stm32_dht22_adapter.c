#include "stm32_dht22_adapter.h"

#include <stddef.h>
#include <string.h>

#include "dht22_protocol.h"

static uint32_t elapsed_us(uint32_t now_us, uint32_t started_at_us)
{
    return now_us - started_at_us;
}

static bool hardware_is_valid(const stm32_dht22_hardware_t *hardware)
{
    return hardware != NULL
        && hardware->initialize != NULL
        && hardware->now_us != NULL
        && hardware->drive_low != NULL
        && hardware->release_line != NULL
        && hardware->enable_capture != NULL
        && hardware->disable_capture != NULL;
}

static void reset_capture(stm32_dht22_adapter_t *adapter)
{
    adapter->pulse_count = 0U;
    adapter->rising_at_us = 0U;
    adapter->high_active = false;
    memset(
        (void *)adapter->high_pulses_us,
        0,
        sizeof(adapter->high_pulses_us)
    );
}

static void return_to_idle(stm32_dht22_adapter_t *adapter)
{
    adapter->hardware.disable_capture(adapter->hardware.context);
    adapter->state = STM32_DHT22_IDLE;
}

static bool initialize_adapter(void *context)
{
    stm32_dht22_adapter_t *adapter = context;

    if (adapter == NULL || !hardware_is_valid(&adapter->hardware))
    {
        return false;
    }

    if (!adapter->hardware.initialize(adapter->hardware.context))
    {
        return false;
    }

    adapter->hardware.disable_capture(adapter->hardware.context);
    reset_capture(adapter);
    adapter->phase_started_us = adapter->hardware.now_us(
        adapter->hardware.context
    );
    adapter->state = STM32_DHT22_IDLE;
    return true;
}

static environmental_sensor_operation_t start_measurement(void *context)
{
    stm32_dht22_adapter_t *adapter = context;

    if (adapter == NULL)
    {
        return ENVIRONMENTAL_SENSOR_OPERATION_INVALID_ARGUMENT;
    }

    if (adapter->state == STM32_DHT22_UNINITIALIZED)
    {
        return ENVIRONMENTAL_SENSOR_OPERATION_NOT_READY;
    }

    if (adapter->state != STM32_DHT22_IDLE)
    {
        return ENVIRONMENTAL_SENSOR_OPERATION_PENDING;
    }

    reset_capture(adapter);
    adapter->hardware.disable_capture(adapter->hardware.context);

    if (!adapter->hardware.drive_low(adapter->hardware.context))
    {
        adapter->state = STM32_DHT22_IDLE;
        return ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    }

    adapter->phase_started_us = adapter->hardware.now_us(
        adapter->hardware.context
    );
    adapter->state = STM32_DHT22_START_LOW;
    return ENVIRONMENTAL_SENSOR_OPERATION_OK;
}

static environmental_sensor_operation_t begin_capture(
    stm32_dht22_adapter_t *adapter,
    uint32_t now_us
)
{
    if (!adapter->hardware.release_line(adapter->hardware.context))
    {
        adapter->state = STM32_DHT22_IDLE;
        return ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    }

    if (!adapter->hardware.enable_capture(adapter->hardware.context))
    {
        adapter->hardware.disable_capture(adapter->hardware.context);
        adapter->state = STM32_DHT22_IDLE;
        return ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    }

    adapter->phase_started_us = now_us;
    adapter->state = STM32_DHT22_CAPTURING;
    return ENVIRONMENTAL_SENSOR_OPERATION_PENDING;
}

static bool snapshot_pulses(
    stm32_dht22_adapter_t *adapter,
    uint16_t output_pulses[STM32_DHT22_DATA_BITS]
)
{
    size_t index;

    /* Capture is disabled before copying so task code sees one complete frame. */
    adapter->hardware.disable_capture(adapter->hardware.context);

    if (adapter->pulse_count != STM32_DHT22_DATA_BITS)
    {
        return false;
    }

    for (index = 0U; index < STM32_DHT22_DATA_BITS; ++index)
    {
        output_pulses[index] = adapter->high_pulses_us[index];
    }

    return true;
}

static environmental_sensor_operation_t decode_frame(
    stm32_dht22_adapter_t *adapter,
    environmental_sensor_reading_t *reading
)
{
    uint16_t pulse_snapshot[STM32_DHT22_DATA_BITS];
    uint8_t bits[STM32_DHT22_DATA_BITS];
    uint8_t frame[DHT22_PROTOCOL_FRAME_SIZE_BYTES];
    dht22_protocol_reading_t decoded;
    dht22_pulse_limits_t limits = dht22_protocol_default_pulse_limits();
    size_t index;

    if (!snapshot_pulses(adapter, pulse_snapshot))
    {
        adapter->state = STM32_DHT22_IDLE;
        return ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    }

    for (index = 0U; index < STM32_DHT22_DATA_BITS; ++index)
    {
        if (dht22_protocol_classify_high_pulse(
                pulse_snapshot[index],
                &limits,
                &bits[index]
            ) != DHT22_PROTOCOL_OK)
        {
            adapter->state = STM32_DHT22_IDLE;
            return ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
        }
    }

    if (dht22_protocol_pack_bits(
            bits,
            STM32_DHT22_DATA_BITS,
            frame
        ) != DHT22_PROTOCOL_OK)
    {
        adapter->state = STM32_DHT22_IDLE;
        return ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    }

    if (dht22_protocol_decode_frame(
            frame,
            DHT22_PROTOCOL_FRAME_SIZE_BYTES,
            &decoded
        ) != DHT22_PROTOCOL_OK)
    {
        adapter->state = STM32_DHT22_IDLE;
        return ENVIRONMENTAL_SENSOR_OPERATION_HARDWARE_ERROR;
    }

    reading->temperature_celsius = decoded.temperature_celsius;
    reading->relative_humidity_percent = decoded.relative_humidity_percent;
    adapter->state = STM32_DHT22_IDLE;
    return ENVIRONMENTAL_SENSOR_OPERATION_OK;
}

static environmental_sensor_operation_t poll_measurement(
    void *context,
    environmental_sensor_reading_t *reading
)
{
    stm32_dht22_adapter_t *adapter = context;
    uint32_t now_us;

    if (adapter == NULL || reading == NULL)
    {
        return ENVIRONMENTAL_SENSOR_OPERATION_INVALID_ARGUMENT;
    }

    if (adapter->state == STM32_DHT22_UNINITIALIZED)
    {
        return ENVIRONMENTAL_SENSOR_OPERATION_NOT_READY;
    }

    now_us = adapter->hardware.now_us(adapter->hardware.context);

    if (adapter->state == STM32_DHT22_START_LOW)
    {
        if (elapsed_us(now_us, adapter->phase_started_us)
            < STM32_DHT22_START_LOW_US)
        {
            return ENVIRONMENTAL_SENSOR_OPERATION_PENDING;
        }

        return begin_capture(adapter, now_us);
    }

    if (adapter->state == STM32_DHT22_CAPTURING)
    {
        if (elapsed_us(now_us, adapter->phase_started_us)
            >= STM32_DHT22_TIMEOUT_US)
        {
            return_to_idle(adapter);
            return ENVIRONMENTAL_SENSOR_OPERATION_TIMEOUT;
        }

        return ENVIRONMENTAL_SENSOR_OPERATION_PENDING;
    }

    if (adapter->state == STM32_DHT22_FRAME_READY)
    {
        return decode_frame(adapter, reading);
    }

    return ENVIRONMENTAL_SENSOR_OPERATION_NOT_READY;
}

bool stm32_dht22_adapter_configure(
    stm32_dht22_adapter_t *adapter,
    const stm32_dht22_hardware_t *hardware
)
{
    if (adapter == NULL || !hardware_is_valid(hardware))
    {
        return false;
    }

    memset(adapter, 0, sizeof(*adapter));
    adapter->hardware = *hardware;
    adapter->state = STM32_DHT22_UNINITIALIZED;
    return true;
}

environmental_sensor_port_t stm32_dht22_adapter_port(
    stm32_dht22_adapter_t *adapter
)
{
    environmental_sensor_port_t port = {
        adapter,
        initialize_adapter,
        start_measurement,
        poll_measurement
    };

    return port;
}

void stm32_dht22_adapter_on_edge(
    stm32_dht22_adapter_t *adapter,
    uint32_t timestamp_us,
    bool level_high
)
{
    uint32_t duration_us;
    uint8_t index;

    if (adapter == NULL || adapter->state != STM32_DHT22_CAPTURING)
    {
        return;
    }

    if (level_high)
    {
        adapter->rising_at_us = timestamp_us;
        adapter->high_active = true;
        return;
    }

    if (!adapter->high_active)
    {
        return;
    }

    duration_us = elapsed_us(timestamp_us, adapter->rising_at_us);
    adapter->high_active = false;

    if (duration_us > UINT16_MAX)
    {
        adapter->state = STM32_DHT22_IDLE;
        return;
    }

    index = adapter->pulse_count;
    if (index >= STM32_DHT22_DATA_BITS)
    {
        return;
    }

    adapter->high_pulses_us[index] = (uint16_t)duration_us;
    adapter->pulse_count = (uint8_t)(index + 1U);

    /* Publishing FRAME_READY last prevents task code from seeing partial data. */
    if (adapter->pulse_count == STM32_DHT22_DATA_BITS)
    {
        adapter->state = STM32_DHT22_FRAME_READY;
    }
}

stm32_dht22_state_t stm32_dht22_adapter_state(
    const stm32_dht22_adapter_t *adapter
)
{
    return adapter != NULL ? adapter->state : STM32_DHT22_UNINITIALIZED;
}
