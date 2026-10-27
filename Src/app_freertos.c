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
#include "watchdog_port.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  sensor_sample_t sample;
  sensor_acquisition_diagnostics_t diagnostics;
  UBaseType_t sensor_stack_words;
  uint32_t published_at_tick;
  bool sensor_ready;
} watchdog_runtime_snapshot_t;
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
#define SENSOR_TASK_STACK_MIN_WORDS 64U
#define DEFAULT_TASK_STACK_MIN_WORDS 128U
#define WATCHDOG_HEARTBEAT_TIMEOUT_TICKS 1500U
#define WATCHDOG_SENSOR_TIMEOUT_TICKS 3000U

static watchdog_runtime_snapshot_t watchdogRuntimeMailbox;
static const system_watchdog_policy_t monitoringWatchdogPolicy = {
  .heartbeat_timeout_ticks = WATCHDOG_HEARTBEAT_TIMEOUT_TICKS,
  .sensor_timeout_ticks = WATCHDOG_SENSOR_TIMEOUT_TICKS,
};
static watchdog_port_t monitoringWatchdogPort;

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
  .stack_size = 352 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void StartSensorTask(void *argument);
static void reportSensorRuntime(
    const sensor_sample_t *sample,
    const sensor_acquisition_diagnostics_t *diagnostics,
    UBaseType_t sensorStackWords,
    UBaseType_t defaultStackWords);
static const char *stackHealthStatus(
    UBaseType_t sensorStackWords,
    UBaseType_t defaultStackWords);
static void publishWatchdogRuntimeSnapshot(
    const sensor_sample_t *sample,
    const sensor_acquisition_diagnostics_t *diagnostics,
    UBaseType_t sensorStackWords,
    uint32_t currentTick);
static bool readWatchdogRuntimeSnapshot(
    watchdog_runtime_snapshot_t *snapshot);
static bool monitoringWatchdogRefresh(void *context);
static const char *watchdogDecisionName(
    system_watchdog_decision_t decision);
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
watchdog_runtime_snapshot_t watchdogSnapshot;
system_watchdog_decision_t watchdogDecision =
    SYSTEM_WATCHDOG_NOT_CONFIGURED;
system_watchdog_decision_t lastReportedWatchdogDecision =
    SYSTEM_WATCHDOG_NOT_CONFIGURED;
uint32_t lastReportedWatchdogSequence = 0U;
uint32_t lastReportedSensorSequence = 0U;
(void)argument;

if (!system_supervisor_initialize(
    &supervisor,
    stm32_status_indicator_port()))
{
  Error_Handler();
}

monitoringWatchdogPort.context = NULL;
monitoringWatchdogPort.refresh = monitoringWatchdogRefresh;
if (!system_supervisor_configure_watchdog(
    &supervisor,
    &monitoringWatchdogPort,
    &monitoringWatchdogPolicy))
{
  Error_Handler();
}

for (;;)
{
  system_supervisor_run_heartbeat(&supervisor);

  if (readWatchdogRuntimeSnapshot(&watchdogSnapshot))
  {
    UBaseType_t defaultStackWords = uxTaskGetStackHighWaterMark(NULL);
    bool sensorStackOk =
        watchdogSnapshot.sensor_stack_words >= SENSOR_TASK_STACK_MIN_WORDS;
    bool defaultStackOk =
        defaultStackWords >= DEFAULT_TASK_STACK_MIN_WORDS;

    if (watchdogSnapshot.sample.sequence != lastReportedSensorSequence)
    {
      reportSensorRuntime(
          &watchdogSnapshot.sample,
          &watchdogSnapshot.diagnostics,
          watchdogSnapshot.sensor_stack_words,
          defaultStackWords
      );
      lastReportedSensorSequence = watchdogSnapshot.sample.sequence;
    }

    watchdogDecision = system_supervisor_evaluate_watchdog(
        &supervisor,
        osKernelGetTickCount(),
        watchdogSnapshot.sample.sequence,
        sensorStackOk,
        defaultStackOk
    );

    if (watchdogDecision != lastReportedWatchdogDecision
        || watchdogSnapshot.sample.sequence
            != lastReportedWatchdogSequence)
    {
      printf(
          "WD mode=MON decision=%s seq=%lu age=%lu ss=%lu ds=%lu\r\n",
          watchdogDecisionName(watchdogDecision),
          (unsigned long)watchdogSnapshot.sample.sequence,
          (unsigned long)(
              osKernelGetTickCount()
              - watchdogSnapshot.published_at_tick),
          (unsigned long)watchdogSnapshot.sensor_stack_words,
          (unsigned long)defaultStackWords
      );
      lastReportedWatchdogDecision = watchdogDecision;
      lastReportedWatchdogSequence =
          watchdogSnapshot.sample.sequence;
    }
  }

  osDelay(500U);
}
/* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static bool monitoringWatchdogRefresh(void *context)
{
  (void)context;
  return true;
}

static const char *watchdogDecisionName(
    system_watchdog_decision_t decision)
{
  switch (decision)
  {
    case SYSTEM_WATCHDOG_NOT_CONFIGURED:
      return "NCFG";
    case SYSTEM_WATCHDOG_NOT_READY:
      return "INIT";
    case SYSTEM_WATCHDOG_REFRESH_ALLOWED:
      return "ALLOW";
    case SYSTEM_WATCHDOG_REFRESHED:
      return "FED";
    case SYSTEM_WATCHDOG_BLOCKED_NO_PROGRESS:
      return "STALL";
    case SYSTEM_WATCHDOG_BLOCKED_STACK_CRITICAL:
      return "STACK";
    case SYSTEM_WATCHDOG_REFRESH_FAILED:
      return "FAIL";
    default:
      return "UNK";
  }
}

static void publishWatchdogRuntimeSnapshot(
    const sensor_sample_t *sample,
    const sensor_acquisition_diagnostics_t *diagnostics,
    UBaseType_t sensorStackWords,
    uint32_t currentTick)
{
  if (sample == NULL || diagnostics == NULL)
  {
    return;
  }

  taskENTER_CRITICAL();
  watchdogRuntimeMailbox.sample = *sample;
  watchdogRuntimeMailbox.diagnostics = *diagnostics;
  watchdogRuntimeMailbox.sensor_stack_words = sensorStackWords;
  watchdogRuntimeMailbox.published_at_tick = currentTick;
  watchdogRuntimeMailbox.sensor_ready = sample->sequence != 0U;
  taskEXIT_CRITICAL();
}

static bool readWatchdogRuntimeSnapshot(
    watchdog_runtime_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }

  taskENTER_CRITICAL();
  *snapshot = watchdogRuntimeMailbox;
  taskEXIT_CRITICAL();
  return snapshot->sensor_ready;
}

static const char *stackHealthStatus(
    UBaseType_t sensorStackWords,
    UBaseType_t defaultStackWords)
{
  if (sensorStackWords < SENSOR_TASK_STACK_MIN_WORDS
      || defaultStackWords < DEFAULT_TASK_STACK_MIN_WORDS)
  {
    return "CRITICAL";
  }
  return "OK";
}

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
      "stack_sensor=%lu stack_default=%lu stack_status=%s\r\n",
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
      (unsigned long)defaultStackWords,
      stackHealthStatus(sensorStackWords, defaultStackWords)
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
        UBaseType_t sensorStackWords = uxTaskGetStackHighWaterMark(NULL);

        if (stm32_sensor_composition_diagnostics(&diagnostics))
        {
          publishWatchdogRuntimeSnapshot(
              &sample,
              &diagnostics,
              sensorStackWords,
              currentTick
          );
          lastReportedSequence = sample.sequence;
        }
      }
    }
    osDelay(1U);
  }
}
/* USER CODE END Application */
