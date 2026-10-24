#include "stm32_sensor_composition.h"

#include "main.h"
#include "sensor_acquisition.h"
#include "stm32_dht22_adapter.h"
#include "stm32_dht22_hal.h"
#include "tim.h"

#define SENSOR_MEASUREMENT_INTERVAL_TICKS 2000U
#define SENSOR_MEASUREMENT_TIMEOUT_TICKS 20U
#define SENSOR_STALE_AFTER_TICKS 10000U

static stm32_dht22_adapter_t dht22_adapter;
static stm32_dht22_hal_t dht22_hal;
static sensor_acquisition_t acquisition;
static bool composition_initialized;

static const sensor_acquisition_config_t acquisition_config = {
    .limits = {
        .minimum_temperature_celsius = -40.0F,
        .maximum_temperature_celsius = 80.0F,
        .minimum_relative_humidity_percent = 0.0F,
        .maximum_relative_humidity_percent = 100.0F,
    },
    .measurement_interval_ticks = SENSOR_MEASUREMENT_INTERVAL_TICKS,
    .measurement_timeout_ticks = SENSOR_MEASUREMENT_TIMEOUT_TICKS,
    .stale_after_ticks = SENSOR_STALE_AFTER_TICKS,
};

bool stm32_sensor_composition_initialize(uint32_t current_tick)
{
    stm32_dht22_hardware_t hardware;
    environmental_sensor_port_t sensor_port;

    composition_initialized = false;
    if (!stm32_dht22_hal_configure(&dht22_hal, &dht22_adapter, &htim1,
            DHT22_DATA_GPIO_Port, DHT22_DATA_Pin))
    {
        return false;
    }

    hardware = stm32_dht22_hal_hardware(&dht22_hal);
    if (!stm32_dht22_adapter_configure(&dht22_adapter, &hardware))
    {
        return false;
    }

    sensor_port = stm32_dht22_adapter_port(&dht22_adapter);
    if (!sensor_acquisition_initialize(&acquisition, &sensor_port,
            &acquisition_config, current_tick))
    {
        return false;
    }

    composition_initialized = true;
    return true;
}

void stm32_sensor_composition_process(uint32_t current_tick)
{
    if (composition_initialized)
    {
        sensor_acquisition_process(&acquisition, current_tick);
    }
}

bool stm32_sensor_composition_is_initialized(void)
{
    return composition_initialized;
}

sensor_sample_t stm32_sensor_composition_latest(uint32_t current_tick)
{
    if (!composition_initialized)
    {
        return sensor_sample_uninitialized();
    }
    return sensor_acquisition_latest_result(&acquisition, current_tick);
}
