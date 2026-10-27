#ifndef SENTINELRT_RESET_CAUSE_CLASSIFIER_H
#define SENTINELRT_RESET_CAUSE_CLASSIFIER_H

#include <stdint.h>

typedef enum
{
    RESET_CAUSE_UNKNOWN = 0,
    RESET_CAUSE_POWER_ON,
    RESET_CAUSE_BROWNOUT,
    RESET_CAUSE_PIN,
    RESET_CAUSE_SOFTWARE,
    RESET_CAUSE_IWDG,
    RESET_CAUSE_WWDG,
    RESET_CAUSE_LOW_POWER,
    RESET_CAUSE_OPTION_BYTE
} reset_cause_t;

enum
{
    RESET_FLAG_POWER_ON = 1UL << 0,
    RESET_FLAG_BROWNOUT = 1UL << 1,
    RESET_FLAG_PIN = 1UL << 2,
    RESET_FLAG_SOFTWARE = 1UL << 3,
    RESET_FLAG_IWDG = 1UL << 4,
    RESET_FLAG_WWDG = 1UL << 5,
    RESET_FLAG_LOW_POWER = 1UL << 6,
    RESET_FLAG_OPTION_BYTE = 1UL << 7
};

reset_cause_t reset_cause_classify(uint32_t observed_flags);
const char *reset_cause_name(reset_cause_t cause);

#endif
