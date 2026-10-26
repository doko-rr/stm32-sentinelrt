#ifndef SENTINELRT_STM32_IWDG_ADAPTER_H
#define SENTINELRT_STM32_IWDG_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32c0xx_hal.h"
#include "stm32c0xx_hal_iwdg.h"
#include "../../../ports/watchdog_port.h"

typedef struct
{
    IWDG_HandleTypeDef handle;
    bool configured;
    bool initialized;
} stm32_iwdg_adapter_t;

bool stm32_iwdg_adapter_configure(
    stm32_iwdg_adapter_t *adapter,
    uint32_t prescaler,
    uint32_t reload
);

bool stm32_iwdg_adapter_initialize(stm32_iwdg_adapter_t *adapter);

watchdog_port_t stm32_iwdg_adapter_port(stm32_iwdg_adapter_t *adapter);

#endif
