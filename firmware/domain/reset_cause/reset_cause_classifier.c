#include "reset_cause_classifier.h"

reset_cause_t reset_cause_classify(uint32_t observed_flags)
{
    if ((observed_flags & RESET_FLAG_IWDG) != 0U) return RESET_CAUSE_IWDG;
    if ((observed_flags & RESET_FLAG_WWDG) != 0U) return RESET_CAUSE_WWDG;
    if ((observed_flags & RESET_FLAG_SOFTWARE) != 0U) return RESET_CAUSE_SOFTWARE;
    if ((observed_flags & RESET_FLAG_LOW_POWER) != 0U) return RESET_CAUSE_LOW_POWER;
    if ((observed_flags & RESET_FLAG_OPTION_BYTE) != 0U) return RESET_CAUSE_OPTION_BYTE;
    if ((observed_flags & RESET_FLAG_BROWNOUT) != 0U) return RESET_CAUSE_BROWNOUT;
    if ((observed_flags & RESET_FLAG_POWER_ON) != 0U) return RESET_CAUSE_POWER_ON;
    if ((observed_flags & RESET_FLAG_PIN) != 0U) return RESET_CAUSE_PIN;
    return RESET_CAUSE_UNKNOWN;
}

const char *reset_cause_name(reset_cause_t cause)
{
    switch (cause)
    {
        case RESET_CAUSE_POWER_ON: return "POR";
        case RESET_CAUSE_BROWNOUT: return "BOR";
        case RESET_CAUSE_PIN: return "PIN";
        case RESET_CAUSE_SOFTWARE: return "SW";
        case RESET_CAUSE_IWDG: return "IWDG";
        case RESET_CAUSE_WWDG: return "WWDG";
        case RESET_CAUSE_LOW_POWER: return "LP";
        case RESET_CAUSE_OPTION_BYTE: return "OBL";
        default: return "UNK";
    }
}
