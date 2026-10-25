#ifndef STM32_SENSOR_COMPOSITION_H
#define STM32_SENSOR_COMPOSITION_H

#include <stdbool.h>
#include <stdint.h>
#include "sensor_acquisition.h"
#include "sensor_sample.h"

#ifdef __cplusplus
extern "C" {
#endif

bool stm32_sensor_composition_initialize(uint32_t current_tick);
void stm32_sensor_composition_process(uint32_t current_tick);
bool stm32_sensor_composition_is_initialized(void);
sensor_sample_t stm32_sensor_composition_latest(uint32_t current_tick);
bool stm32_sensor_composition_diagnostics(
    sensor_acquisition_diagnostics_t *diagnostics
);

#ifdef __cplusplus
}
#endif

#endif
