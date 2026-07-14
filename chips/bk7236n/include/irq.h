/****************************************************************************
 * vendor/beken/chip/bk7236n/include/irq.h
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/* This file should never be included directly but, rather, only indirectly
 * through nuttx/irq.h
 */

#ifndef __VENDOR_BEKEN_CHIP_BK7236N_INCLUDE_IRQ_H
#define __VENDOR_BEKEN_CHIP_BK7236N_INCLUDE_IRQ_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

/* Include chip-specific IRQ definitions (including IRQ numbers) */

#include <nuttx/config.h>

#include <sys/types.h>

#include <arch/chip/irq.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Processor Exceptions (vectors 0-15) */

#define BK7236N_IRQ_RESERVED       (0) /* Reserved vector (only used with CONFIG_DEBUG_FEATURES) */
                                      /* Vector  0: Reset stack pointer value */
                                      /* Vector  1: Reset (not handler as an IRQ) */
#define BK7236N_IRQ_NMI            (2) /* Vector  2: Non-Maskable Interrupt (NMI) */
#define BK7236N_IRQ_HARDFAULT      (3) /* Vector  3: Hard fault */
#define BK7236N_IRQ_MEMFAULT       (4) /* Vector  4: Memory management (MPU) */
#define BK7236N_IRQ_BUSFAULT       (5) /* Vector  5: Bus fault */
#define BK7236N_IRQ_USAGEFAULT     (6) /* Vector  6: Usage fault */
#define NVIC_IRQ_SECUREFAULT      (7) /* Vector  7: Secure fault */
                                      /* Vectors 8-10: Reserved */
#define BK7236N_IRQ_SVCALL        (11) /* Vector 11: SVC call */
#define BK7236N_IRQ_DBGMONITOR    (12) /* Vector 12: Debug Monitor */
                                      /* Vector 13: Reserved */
#define BK7236N_IRQ_PENDSV        (14) /* Vector 14: Pendable system service request */ 
#define BK7236N_IRQ_SYSTICK       (15) /* Vector 15: System tick */ 
#define BK7236N_IRQ_FIRST         (16)   /* Vector number of the first interrupt */

/* External interrupts numbers */

#define	BK7236N_IRQ_DMA_NS                 (BK7236N_IRQ_FIRST + 0)
#define	BK7236N_IRQ_ENCP_S                 (BK7236N_IRQ_FIRST + 1)
#define	BK7236N_IRQ_ENCP_NS                (BK7236N_IRQ_FIRST + 2)
#define	BK7236N_IRQ_TIMER0                 (BK7236N_IRQ_FIRST + 3)
#define	BK7236N_IRQ_UART0                  (BK7236N_IRQ_FIRST + 4)
#define	BK7236N_IRQ_PWM0                   (BK7236N_IRQ_FIRST + 5)
#define	BK7236N_IRQ_I2C0                   (BK7236N_IRQ_FIRST + 6)
#define	BK7236N_IRQ_SPI0                   (BK7236N_IRQ_FIRST + 7)
#define	BK7236N_IRQ_SARADC                 (BK7236N_IRQ_FIRST + 8)
#define	BK7236N_IRQ_UART3                  (BK7236N_IRQ_FIRST + 9)
#define	BK7236N_IRQ_SDIO                   (BK7236N_IRQ_FIRST + 10)
#define	BK7236N_IRQ_GDMA                   (BK7236N_IRQ_FIRST + 11)
#define	BK7236N_IRQ_LA                     (BK7236N_IRQ_FIRST + 12)
#define	BK7236N_IRQ_TIMER1                 (BK7236N_IRQ_FIRST + 13)
#define	BK7236N_IRQ_I2C1                   (BK7236N_IRQ_FIRST + 14)
#define	BK7236N_IRQ_UART1                  (BK7236N_IRQ_FIRST + 15)
#define	BK7236N_IRQ_UART2                  (BK7236N_IRQ_FIRST + 16)
#define	BK7236N_IRQ_SPI1                   (BK7236N_IRQ_FIRST + 17)
#define	BK7236N_IRQ_LED                    (BK7236N_IRQ_FIRST + 18)
#define	BK7236N_IRQ_CKMN                   (BK7236N_IRQ_FIRST + 21)
#define	BK7236N_IRQ_I2S0                   (BK7236N_IRQ_FIRST + 24)
#define	BK7236N_IRQ_PHY_MBP                (BK7236N_IRQ_FIRST + 29)
#define	BK7236N_IRQ_PHY_RIU                (BK7236N_IRQ_FIRST + 30)
#define	BK7236N_IRQ_MAC_INT_TX_RX_TIMER    (BK7236N_IRQ_FIRST + 31)
#define	BK7236N_IRQ_MAC_INT_TX_RX_MISC     (BK7236N_IRQ_FIRST + 32)
#define	BK7236N_IRQ_MAC_INT_RX_TRIGGER     (BK7236N_IRQ_FIRST + 33)
#define	BK7236N_IRQ_MAC_INT_TX_TRIGGER     (BK7236N_IRQ_FIRST + 34)
#define	BK7236N_IRQ_MAC_INT_PORT_TRIGGER   (BK7236N_IRQ_FIRST + 35)
#define	BK7236N_IRQ_MAC_INT_GEN            (BK7236N_IRQ_FIRST + 36)
#define	BK7236N_IRQ_HSU                    (BK7236N_IRQ_FIRST + 37)
#define	BK7236N_IRQ_INT_MAC_WAKEUP         (BK7236N_IRQ_FIRST + 38)
#define	BK7236N_IRQ_DM                     (BK7236N_IRQ_FIRST + 39)
#define	BK7236N_IRQ_BLE                    (BK7236N_IRQ_FIRST + 40)
#define	BK7236N_IRQ_BT                     (BK7236N_IRQ_FIRST + 41)
#define	BK7236N_IRQ_QSPI0                  (BK7236N_IRQ_FIRST + 42)
#define	BK7236N_IRQ_THREAD                 (BK7236N_IRQ_FIRST + 48)
#define	BK7236N_IRQ_OTP                    (BK7236N_IRQ_FIRST + 50)
#define	BK7236N_IRQ_DPLL_UNLOCK            (BK7236N_IRQ_FIRST + 51)
#define	BK7236N_IRQ_GPIO                   (BK7236N_IRQ_FIRST + 55)
#define	BK7236N_IRQ_GPIO_NS                (BK7236N_IRQ_FIRST + 56)
#define	BK7236N_IRQ_GPIO_ANA               (BK7236N_IRQ_FIRST + 58)
#define	BK7236N_IRQ_RTC                    (BK7236N_IRQ_FIRST + 59)
#define	BK7236N_IRQ_GPIO_ABNORMAL          (BK7236N_IRQ_FIRST + 60)
#define	BK7236N_IRQ_RTC_ABNORMAL           (BK7236N_IRQ_FIRST + 61)

/* IRQ numbers.  The IRQ number corresponds vector number and hence map
 * directly to bits in the INTC.  This does, however, waste several
 * words of memory in the IRQ to handle mapping tables.
 */

#define NR_IRQS                  (77)

/* NVIC priority levels */

#define NVIC_SYSH_PRIORITY_MIN     0xf0 /* All bits set in minimum priority */
#define NVIC_SYSH_PRIORITY_DEFAULT 0x80 /* Midpoint is the default */
#define NVIC_SYSH_PRIORITY_MAX     0x00 /* Zero is maximum priority */
#define NVIC_SYSH_PRIORITY_STEP    0x10 /* Four bits of interrupt priority used */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Inline functions
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __VENDOR_BEKEN_CHIP_BK7236N_INCLUDE_IRQ_H */
