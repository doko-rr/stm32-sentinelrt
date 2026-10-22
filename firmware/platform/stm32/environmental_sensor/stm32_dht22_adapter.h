#ifndef STM32_DHT22_ADAPTER_H
#define STM32_DHT22_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "environmental_sensor_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STM32_DHT22_DATA_BITS 40U
#define STM32_DHT22_START_LOW_US 1100U
#define STM32_DHT22_TIMEOUT_US 5000U

typedef enum
{
    STM32_DHT22_UNINITIALIZED = 0,
    STM32_DHT22_IDLE,
    STM32_DHT22_START_LOW,
    STM32_DHT22_CAPTURING,
    STM32_DHT22_FRAME_READY
} stm32_dht22_state_t;

typedef struct
{
    void *context;
    bool (*initialize)(void *context);
    uint32_t (*now_us)(void *context);
    bool (*drive_low)(void *context);
    bool (*release_line)(void *context);
    bool (*enable_capture)(void *context);
    void (*disable_capture)(void *context);
} stm32_dht22_hardware_t;

typedef struct
{
    stm32_dht22_hardware_t hardware;
    volatile stm32_dht22_state_t state;
    volatile uint16_t high_pulses_us[STM32_DHT22_DATA_BITS];
    volatile uint8_t pulse_count;
    volatile uint32_t rising_at_us;
    volatile bool high_active;
    uint32_t phase_started_us;
} stm32_dht22_adapter_t;

bool stm32_dht22_adapter_configure(
    stm32_dht22_adapter_t *adapter,
    const stm32_dht22_hardware_t *hardware
);

environmental_sensor_port_t stm32_dht22_adapter_port(
    stm32_dht22_adapter_t *adapter
);

void stm32_dht22_adapter_on_edge(
    stm32_dht22_adapter_t *adapter,
    uint32_t timestamp_us,
    bool level_high
);

stm32_dht22_state_t stm32_dht22_adapter_state(
    const stm32_dht22_adapter_t *adapter
);

#ifdef __cplusplus
}
#endif

#endif
