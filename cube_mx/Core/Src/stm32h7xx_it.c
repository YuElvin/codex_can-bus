/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "main.h"
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "platform/stm32h750_bringup.h"
#include "task.h"
#include <stddef.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
typedef struct {
  uint32_t magic;
  uint32_t exception;
  uint32_t stacked_r0;
  uint32_t stacked_r1;
  uint32_t stacked_r2;
  uint32_t stacked_r3;
  uint32_t stacked_r12;
  uint32_t stacked_lr;
  uint32_t stacked_pc;
  uint32_t stacked_xpsr;
  uint32_t exception_lr;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t mmfar;
  uint32_t bfar;
  uint32_t afsr;
  uint32_t checksum;
} FaultRecord;

#define FAULT_RECORD_MAGIC 0x4641554cu

volatile FaultRecord g_fault_record __attribute__((section(".noinit")));

static uint32_t fault_record_checksum(const FaultRecord *record)
{
  const uint32_t *words = (const uint32_t *)record;
  uint32_t checksum = 0x5a3c19e7u;

  for (size_t i = 0u; i < (sizeof(*record) / sizeof(words[0])) - 1u; ++i) {
    checksum ^= words[i];
  }
  return checksum;
}

__attribute__((noreturn, noinline, used)) static void fault_record_capture(uint32_t *stack,
                                                                             uint32_t exception_lr,
                                                                             uint32_t exception)
{
  FaultRecord record = {
    .magic = FAULT_RECORD_MAGIC,
    .exception = exception,
    .stacked_r0 = stack[0],
    .stacked_r1 = stack[1],
    .stacked_r2 = stack[2],
    .stacked_r3 = stack[3],
    .stacked_r12 = stack[4],
    .stacked_lr = stack[5],
    .stacked_pc = stack[6],
    .stacked_xpsr = stack[7],
    .exception_lr = exception_lr,
    .cfsr = SCB->CFSR,
    .hfsr = SCB->HFSR,
    .mmfar = SCB->MMFAR,
    .bfar = SCB->BFAR,
    .afsr = SCB->AFSR,
  };

  record.checksum = fault_record_checksum(&record);
  g_fault_record = record;
  SCB_CleanDCache_by_Addr((uint32_t *)(uintptr_t)&g_fault_record, sizeof(g_fault_record));
  __DSB();
  __ISB();
  NVIC_SystemReset();
  for (;;) {
  }
}

#define FAULT_HANDLER(name, exception_code)                                                   \
  __attribute__((naked)) void name(void)                                                      \
  {                                                                                            \
    __asm volatile("tst lr, #4\n"                                                           \
                   "ite eq\n"                                                                \
                   "mrseq r0, msp\n"                                                        \
                   "mrsne r0, psp\n"                                                        \
                   "mov r1, lr\n"                                                           \
                   "movs r2, %0\n"                                                         \
                   "b fault_record_capture\n"                                               \
                   :                                                                          \
                   : "I"(exception_code));                                                  \
  }

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern FDCAN_HandleTypeDef hfdcan1;
/* USER CODE BEGIN EV */
extern FDCAN_HandleTypeDef hfdcan2;
extern SD_HandleTypeDef hsd1;

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Memory management fault.
  */
FAULT_HANDLER(MemManage_Handler, 1)

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
FAULT_HANDLER(BusFault_Handler, 2)

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
FAULT_HANDLER(UsageFault_Handler, 3)

FAULT_HANDLER(HardFault_Handler, 4)

/**
  * @brief This function handles System service call via SWI instruction.
  */
__attribute__((naked)) void SVC_Handler(void)
{
  __asm volatile("b vPortSVCHandler");
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
__attribute__((naked)) void PendSV_Handler(void)
{
  __asm volatile("b xPortPendSVHandler");
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
    xPortSysTickHandler();
  }

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles FDCAN1 interrupt 0.
  */
void FDCAN1_IT0_IRQHandler(void)
{
  /* USER CODE BEGIN FDCAN1_IT0_IRQn 0 */

  /* USER CODE END FDCAN1_IT0_IRQn 0 */
  HAL_FDCAN_IRQHandler(&hfdcan1);
  /* USER CODE BEGIN FDCAN1_IT0_IRQn 1 */

  /* USER CODE END FDCAN1_IT0_IRQn 1 */
}

/**
  * @brief This function handles FDCAN1 interrupt 1.
  */
void FDCAN1_IT1_IRQHandler(void)
{
  /* USER CODE BEGIN FDCAN1_IT1_IRQn 0 */

  /* USER CODE END FDCAN1_IT1_IRQn 0 */
  HAL_FDCAN_IRQHandler(&hfdcan1);
  /* USER CODE BEGIN FDCAN1_IT1_IRQn 1 */

  /* USER CODE END FDCAN1_IT1_IRQn 1 */
}

/**
  * @brief This function handles FDCAN2 interrupt 0.
  */
void FDCAN2_IT0_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan2);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  if (hfdcan != NULL && hfdcan->Instance == FDCAN2) {
    can2_analyzer_rx_notify_from_isr(RxFifo0ITs);
  }
}

/**
  * @brief This function handles EXTI line[9:5] interrupts.
  */
void EXTI9_5_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI9_5_IRQn 0 */

  /* USER CODE END EXTI9_5_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(W5500_INT_Pin);
  /* USER CODE BEGIN EXTI9_5_IRQn 1 */

  /* USER CODE END EXTI9_5_IRQn 1 */
}

/* USER CODE BEGIN 1 */
void SDMMC1_IRQHandler(void)
{
  HAL_SD_IRQHandler(&hsd1);
}

/* USER CODE END 1 */
