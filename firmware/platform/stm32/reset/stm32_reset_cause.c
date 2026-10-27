#include "stm32_reset_cause.h"

#include <stddef.h>

#include "stm32c0xx_hal.h"
#include "stm32c0xx_hal_rcc.h"

static stm32_reset_cause_snapshot_t reset_snapshot;

static uint32_t read_reset_flags(void)
{
    uint32_t flags = 0U;

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) flags |= RESET_FLAG_IWDG;
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET) flags |= RESET_FLAG_WWDG;
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET) flags |= RESET_FLAG_SOFTWARE;
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET) flags |= RESET_FLAG_PIN;
#ifdef RCC_FLAG_PWRRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PWRRST) != RESET) flags |= RESET_FLAG_POWER_ON;
#endif
#ifdef RCC_FLAG_PORRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET) flags |= RESET_FLAG_POWER_ON;
#endif
#ifdef RCC_FLAG_BORRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET) flags |= RESET_FLAG_BROWNOUT;
#endif
#ifdef RCC_FLAG_LPWRRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET) flags |= RESET_FLAG_LOW_POWER;
#endif
#ifdef RCC_FLAG_OBLRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST) != RESET) flags |= RESET_FLAG_OPTION_BYTE;
#endif
    return flags;
}

bool stm32_reset_cause_capture(void)
{
    uint32_t flags;

    if (reset_snapshot.captured) return false;

    flags = read_reset_flags();
    reset_snapshot.observed_flags = flags;
    reset_snapshot.cause = reset_cause_classify(flags);
    reset_snapshot.captured = true;

    /* Preserve the immutable snapshot before clearing sticky RCC flags. */
    __HAL_RCC_CLEAR_RESET_FLAGS();
    return true;
}

bool stm32_reset_cause_get(stm32_reset_cause_snapshot_t *snapshot)
{
    if (snapshot == NULL || !reset_snapshot.captured) return false;
    *snapshot = reset_snapshot;
    return true;
}
