/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "system_supervisor.h"
#include "stm32_status_indicator.h"
#include "stm32_sensor_composition.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
#define SENSOR_TASK_STACK_WORDS 256U

static osThreadId_t sensorTaskHandle;
static StaticTask_t sensorTaskControlBlock;
static StackType_t sensorTaskStack[SENSOR_TASK_STACK_WORDS];

static const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .cb_mem = &sensorTaskControlBlock,
  .cb_size = sizeof(sensorTaskControlBlock),
  .stack_mem = sensorTaskStack,
  .stack_size = sizeof(sensorTaskStack),
  .priority = (osPriority_t) osPriorityNormal
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void StartSensorTask(void *argument);
static void reportSensorRuntime(
    const sensor_sample_t *sample,
    const sensor_acquisition_diagnostics_t *diagnostics,
    UBaseType_t sensorStackWords,
    UBaseType_t defaultStackWords);
/* USER CODE END FunctionPrototypes */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);
  if (sensorTaskHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartDefaultTask */
/**
* @brief Function implementing the defaultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN defaultTask */
system_supervisor_t supervisor = {0};
(void)argument;

if (!system_supervisor_initialize(
    &supervisor,
    stm32_status_indicator_port()))
{
  Error_Handler();
}

for (;;)
{
  system_supervisor_run_heartbeat(&supervisor);
  osDelay(500U);
}
/* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void reportSensorRuntime(
    const sensor_sample_t *sample,
    const sensor_acquisition_diagnostics_t *diagnostics,
    UBaseType_t sensorStackWords,
    UBaseType_t defaultStackWords)
{
  int32_t temperatureTenths;
  uint32_t humidityTenths;

  if (sample == NULL || diagnostics == NULL)
  {
    return;
  }

  temperatureTenths = (int32_t)(sample->temperature_celsius * 10.0F);
  humidityTenths = (uint32_t)(sample->relative_humidity_percent * 10.0F);

  printf(
      "SENSOR status=%s seq=%lu temp=%ld.%01ldC humidity=%lu.%01lu%% "
      "attempts=%lu success=%lu range=%lu timeout=%lu hw=%lu "
      "fail_streak=%lu peak_fail_streak=%lu "
      "stack_sensor=%lu stack_default=%lu\r\n",
      sensor_sample_status_name(sample->status),
      (unsigned long)sample->sequence,
      (long)(temperatureTenths / 10),
      (long)(temperatureTenths < 0
          ? -(temperatureTenths % 10)
          : temperatureTenths % 10),
      (unsigned long)(humidityTenths / 10U),
      (unsigned long)(humidityTenths % 10U),
      (unsigned long)diagnostics->total_attempts,
      (unsigned long)diagnostics->successful_samples,
      (unsigned long)diagnostics->out_of_range_samples,
      (unsigned long)diagnostics->timeout_failures,
      (unsigned long)diagnostics->hardware_failures,
      (unsigned long)diagnostics->consecutive_failures,
      (unsigned long)diagnostics->peak_consecutive_failures,
      (unsigned long)sensorStackWords,
      (unsigned long)defaultStackWords
  );
}

static void StartSensorTask(void *argument)
{
  bool sensorReady;
  uint32_t lastReportedSequence = 0U;

  (void)argument;
  sensorReady = stm32_sensor_composition_initialize(osKernelGetTickCount());
  printf(
      "SentinelRT sensor runtime %s\r\n",
      sensorReady ? "started" : "initialization_failed"
  );

  for (;;)
  {
    if (sensorReady)
    {
      uint32_t currentTick = osKernelGetTickCount();
      sensor_sample_t sample;
      sensor_acquisition_diagnostics_t diagnostics;

      stm32_sensor_composition_process(currentTick);
      sample = stm32_sensor_composition_latest(currentTick);

      if (sample.sequence != 0U
          && sample.sequence != lastReportedSequence)
      {
        if (stm32_sensor_composition_diagnostics(&diagnostics))
        {
          reportSensorRuntime(
              &sample,
              &diagnostics,
              uxTaskGetStackHighWaterMark(NULL),
              uxTaskGetStackHighWaterMark(defaultTaskHandle)
          );
          lastReportedSequence = sample.sequence;
        }
      }
    }
    osDelay(1U);
  }
}
/* USER CODE END Application */

