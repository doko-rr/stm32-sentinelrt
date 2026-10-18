#include "stm32_status_indicator.h"
#include <stddef.h>
#include "stm32c0xx_nucleo.h"
static bool initialize_indicator(void *context)
{
    (void)context;
    return BSP_LED_Init(LED_GREEN) == BSP_ERROR_NONE;
}
static void toggle_indicator(void *context)
{
    (void)context;
    BSP_LED_Toggle(LED_GREEN);
}
const status_indicator_port_t *stm32_status_indicator_port(void)
{
    static const status_indicator_port_t port = {
        .context = NULL,
        .initialize = initialize_indicator,
        .toggle = toggle_indicator,
    };
    return &port;
}
