/*
 * FreeRTOSConfig.h for TX32F01 (Cortex-M0, 24 MHz, 4 KB SRAM, 32 KB Flash)
 *
 * Tuned aggressively for a 4 KB SRAM device:
 *   - Static allocation only (no heap_x.c needed)
 *   - Preemption ON, tickless OFF (simple)
 *   - Disabled: timers, mutexes, event groups, stream/message buffers, hooks
 *   - 1 ms tick, 3 priorities, minimal stack 64 words (256 B)
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* === Core knobs ============================================== */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0   /* CM0 doesn't have CLZ */
#define configUSE_TICKLESS_IDLE                 0

#define configCPU_CLOCK_HZ                      (24000000UL)
#define configTICK_RATE_HZ                      (1000)
#define configMAX_PRIORITIES                    3
#define configMINIMAL_STACK_SIZE                ((uint16_t)64)   /* 64 words = 256 B */
#define configMAX_TASK_NAME_LEN                 8
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1

/* === Memory allocation ====================================== */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0
#define configTOTAL_HEAP_SIZE                   0   /* No heap_x.c at all */

/* === Features turned OFF to save Flash/RAM ================== */
#define configUSE_MUTEXES                       0
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIMERS                        0
#define configUSE_TRACE_FACILITY                0
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configUSE_APPLICATION_TASK_TAG          0

/* === Hooks turned OFF ======================================= */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          0   /* 1 or 2 if you want stack canaries */
#define configRECORD_STACK_HIGH_ADDRESS         0

/* === Co-routines disabled =================================== */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         1

/* === Queue registry (debug aid, off) ======================== */
#define configQUEUE_REGISTRY_SIZE               0

/* === INCLUDE_* : trim the public API ======================== */
#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    1   /* needed by vTaskDelay(portMAX_DELAY) */
#define INCLUDE_vTaskDelayUntil                 0
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          0
#define INCLUDE_xTaskGetCurrentTaskHandle       0
#define INCLUDE_uxTaskGetStackHighWaterMark     0
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xEventGroupSetBitFromISR        0
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              0

/* === Cortex-M0 specific (no nesting on CM0) ================= */
#define configKERNEL_INTERRUPT_PRIORITY         (255UL)
/* configMAX_SYSCALL_INTERRUPT_PRIORITY is not used on CM0 (no BASEPRI) */

/* === Assert ================================================= */
#define configASSERT(x) if ((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

/* === Map FreeRTOS handler names to TX32F01 startup names =====
 *
 * TX32F01 startup_TX32F01.s declares these as WEAK:
 *   SVC_Handler, PendSV_Handler, SysTick_Handler
 *
 * FreeRTOS port.c (RVDS/ARM_CM0) provides:
 *   vPortSVCHandler, xPortPendSVHandler, xPortSysTickHandler
 *
 * Map them so the strong FreeRTOS symbols override the weak startup ones.
 * (Using #define for source-level rename - simplest, no asm trampoline.)
 */
#define vPortSVCHandler        SVC_Handler
#define xPortPendSVHandler     PendSV_Handler
#define xPortSysTickHandler    SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
