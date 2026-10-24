#ifndef STM32_DHT22_HAL_H
#define STM32_DHT22_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32c0xx_hal.h"
#include "stm32_dht22_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    stm32_dht22_adapter_t *adapter;
    TIM_HandleTypeDef *timer;
    GPIO_TypeDef *data_port;
    uint16_t data_pin;
    volatile bool capture_enabled;
    bool timer_started;
} stm32_dht22_hal_t;

bool stm32_dht22_hal_configure(
    stm32_dht22_hal_t *binding,
    stm32_dht22_adapter_t *adapter,
    TIM_HandleTypeDef *timer,
    GPIO_TypeDef *data_port,
    uint16_t data_pin
);

stm32_dht22_hardware_t stm32_dht22_hal_hardware(
    stm32_dht22_hal_t *binding
);

#ifdef __cplusplus
}
#endif

#endif
