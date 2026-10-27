#ifndef SENTINELRT_STM32_RESET_CAUSE_H
#define SENTINELRT_STM32_RESET_CAUSE_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../domain/reset_cause/reset_cause_classifier.h"

typedef struct
{
    reset_cause_t cause;
    uint32_t observed_flags;
    bool captured;
} stm32_reset_cause_snapshot_t;

bool stm32_reset_cause_capture(void);
bool stm32_reset_cause_get(stm32_reset_cause_snapshot_t *snapshot);

#endif
