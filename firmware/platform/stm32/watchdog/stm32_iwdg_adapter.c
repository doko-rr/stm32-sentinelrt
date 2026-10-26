#include "stm32_iwdg_adapter.h"

#include <stddef.h>

static bool configuration_is_valid(uint32_t prescaler, uint32_t reload)
{
    return IS_IWDG_PRESCALER(prescaler)
        && IS_IWDG_RELOAD(reload);
}

static bool refresh(void *context)
{
    stm32_iwdg_adapter_t *adapter = context;

    if (adapter == NULL || !adapter->initialized)
    {
        return false;
    }

    return HAL_IWDG_Refresh(&adapter->handle) == HAL_OK;
}

bool stm32_iwdg_adapter_configure(
    stm32_iwdg_adapter_t *adapter,
    uint32_t prescaler,
    uint32_t reload)
{
    if (adapter == NULL || !configuration_is_valid(prescaler, reload))
    {
        return false;
    }

    *adapter = (stm32_iwdg_adapter_t){0};
    adapter->handle.Instance = IWDG;
    adapter->handle.Init.Prescaler = prescaler;
    adapter->handle.Init.Window = IWDG_WINDOW_DISABLE;
    adapter->handle.Init.Reload = reload;
    adapter->configured = true;
    return true;
}

bool stm32_iwdg_adapter_initialize(stm32_iwdg_adapter_t *adapter)
{
    if (adapter == NULL || !adapter->configured || adapter->initialized)
    {
        return false;
    }

    if (HAL_IWDG_Init(&adapter->handle) != HAL_OK)
    {
        return false;
    }

    adapter->initialized = true;
    return true;
}

watchdog_port_t stm32_iwdg_adapter_port(stm32_iwdg_adapter_t *adapter)
{
    watchdog_port_t port = {
        .context = adapter,
        .refresh = refresh,
    };

    if (adapter == NULL || !adapter->configured)
    {
        port.refresh = NULL;
    }

    return port;
}
