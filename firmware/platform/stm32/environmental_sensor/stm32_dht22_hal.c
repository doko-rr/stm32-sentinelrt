#include "stm32_dht22_hal.h"

#include <stddef.h>

static stm32_dht22_hal_t *active_binding;

static bool binding_is_valid(const stm32_dht22_hal_t *binding)
{
    return binding != NULL
        && binding->adapter != NULL
        && binding->timer != NULL
        && binding->data_port != NULL
        && binding->data_pin != 0U;
}

static void configure_input_exti(stm32_dht22_hal_t *binding)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = binding->data_pin;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(binding->data_port, &gpio);
}

static void configure_output_open_drain(stm32_dht22_hal_t *binding)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = binding->data_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(binding->data_port, &gpio);
}

static void clear_pending_exti(const stm32_dht22_hal_t *binding)
{
    __HAL_GPIO_EXTI_CLEAR_IT(binding->data_pin);
}

static void disable_capture_internal(stm32_dht22_hal_t *binding)
{
    binding->capture_enabled = false;
    clear_pending_exti(binding);
}

static bool hardware_initialize(void *context)
{
    stm32_dht22_hal_t *binding = context;

    if (!binding_is_valid(binding))
    {
        return false;
    }

    disable_capture_internal(binding);
    HAL_GPIO_WritePin(binding->data_port, binding->data_pin, GPIO_PIN_SET);
    configure_input_exti(binding);
    clear_pending_exti(binding);

    __HAL_TIM_SET_COUNTER(binding->timer, 0U);
    if (HAL_TIM_Base_Start(binding->timer) != HAL_OK)
    {
        binding->timer_started = false;
        return false;
    }

    binding->timer_started = true;
    active_binding = binding;
    return true;
}

static uint32_t hardware_now_us(void *context)
{
    const stm32_dht22_hal_t *binding = context;

    if (!binding_is_valid(binding) || !binding->timer_started)
    {
        return 0U;
    }

    return (uint32_t)__HAL_TIM_GET_COUNTER(binding->timer);
}

static bool hardware_drive_low(void *context)
{
    stm32_dht22_hal_t *binding = context;

    if (!binding_is_valid(binding) || !binding->timer_started)
    {
        return false;
    }

    disable_capture_internal(binding);
    __HAL_TIM_SET_COUNTER(binding->timer, 0U);

    /* Set the latch low before changing direction to avoid a short high pulse. */
    HAL_GPIO_WritePin(binding->data_port, binding->data_pin, GPIO_PIN_RESET);
    configure_output_open_drain(binding);
    return true;
}

static bool hardware_release_line(void *context)
{
    stm32_dht22_hal_t *binding = context;

    if (!binding_is_valid(binding) || !binding->timer_started)
    {
        return false;
    }

    HAL_GPIO_WritePin(binding->data_port, binding->data_pin, GPIO_PIN_SET);
    configure_input_exti(binding);
    clear_pending_exti(binding);
    return true;
}

static bool hardware_enable_capture(void *context)
{
    stm32_dht22_hal_t *binding = context;

    if (!binding_is_valid(binding) || !binding->timer_started)
    {
        return false;
    }

    clear_pending_exti(binding);
    active_binding = binding;
    binding->capture_enabled = true;
    return true;
}

static void hardware_disable_capture(void *context)
{
    stm32_dht22_hal_t *binding = context;

    if (!binding_is_valid(binding))
    {
        return;
    }

    disable_capture_internal(binding);
}

bool stm32_dht22_hal_configure(
    stm32_dht22_hal_t *binding,
    stm32_dht22_adapter_t *adapter,
    TIM_HandleTypeDef *timer,
    GPIO_TypeDef *data_port,
    uint16_t data_pin
)
{
    if (binding == NULL
        || adapter == NULL
        || timer == NULL
        || data_port == NULL
        || data_pin == 0U)
    {
        return false;
    }

    binding->adapter = adapter;
    binding->timer = timer;
    binding->data_port = data_port;
    binding->data_pin = data_pin;
    binding->capture_enabled = false;
    binding->timer_started = false;
    return true;
}

stm32_dht22_hardware_t stm32_dht22_hal_hardware(
    stm32_dht22_hal_t *binding
)
{
    stm32_dht22_hardware_t hardware = {0};

    if (!binding_is_valid(binding))
    {
        return hardware;
    }

    hardware.context = binding;
    hardware.initialize = hardware_initialize;
    hardware.now_us = hardware_now_us;
    hardware.drive_low = hardware_drive_low;
    hardware.release_line = hardware_release_line;
    hardware.enable_capture = hardware_enable_capture;
    hardware.disable_capture = hardware_disable_capture;
    return hardware;
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
    stm32_dht22_hal_t *binding = active_binding;
    uint32_t timestamp_us;
    bool level_high;

    if (binding == NULL
        || !binding->capture_enabled
        || binding->adapter == NULL
        || gpio_pin != binding->data_pin)
    {
        return;
    }

    timestamp_us = (uint32_t)__HAL_TIM_GET_COUNTER(binding->timer);
    level_high = HAL_GPIO_ReadPin(binding->data_port, binding->data_pin)
        == GPIO_PIN_SET;

    stm32_dht22_adapter_on_edge(
        binding->adapter,
        timestamp_us,
        level_high
    );
}
