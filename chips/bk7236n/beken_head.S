/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_head.S
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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "chip.h"

    .syntax    unified
    .arch      armv8-m.main
    .globl     bfhfnmi_handler

    .macro    vector_entry    handler
    .long    \handler+1
    .endm

    .section .vectors, "a"
    .align 2
    .globl    __Vectors
__Vectors:

    .long    __StackTop

    /* Core interrupts */
    vector_entry    Reset_Handler           /* Reset Handler */
    vector_entry    NMI_Handler             /* NMI Handler */
    vector_entry    HardFault_Handler       /* Hard Fault Handler */
    vector_entry    MemManage_Handler       /* MPU Fault Handler */
    vector_entry    BusFault_Handler        /* Bus Fault Handler */
    vector_entry    UsageFault_Handler      /* Usage Fault Handler */
    vector_entry    SecureFault_Handler     /* Secure Fault Handler */
    .long    0                              /* Reserved */
    .long    0                              /* Reserved */
    .long    0                              /* Reserved */
    vector_entry    SVC_Handler             /* SVCall Handler */
    vector_entry    DebugMon_Handler        /* Debug Monitor Handler */
    .long    0                              /* Reserved */
    vector_entry    PendSV_Handler          /* PendSV Handler */
    vector_entry    SysTick_Handler         /* SysTick Handler */

    /* External interrupts */
    vector_entry    DMA_NS_Handler          /* 0: DMA NS Handler */
    vector_entry    ENCP_S_Handler          /* 1: Encrypt enginee S Handler */
    vector_entry    ENCP_NS_Handler         /* 2: Encrypt enginee NS Handler */
    vector_entry    TFM_TIMER0_Handler      /* 3: TFM Timer Handler */
    vector_entry    UART0_Handler           /* 4: UART 0 Handler */
    .rept    43                             /* bootrom will check the magic word:asc ii:bk7236*/
    .long    0
    .endr
    /*.long    0x454b4542
    .long    0x0000004e
    */
    .long    0x32374b42
    .long    0x004E3633


    .size    __Vectors, . - __Vectors


    .extern   exception_common

/*  Macro to define default handlers. */
    .macro    def_irq_handler    handler_name
    .align    1
    .thumb_func
    \handler_name:
    b        exception_common
    .endm

    /* Core interrupts */
    def_irq_handler      NMI_Handler
    def_irq_handler      HardFault_Handler
    def_irq_handler      MemManage_Handler
    def_irq_handler      BusFault_Handler
    def_irq_handler      UsageFault_Handler
    def_irq_handler      SecureFault_Handler
    def_irq_handler      SVC_Handler
    def_irq_handler      DebugMon_Handler
    def_irq_handler      PendSV_Handler
    def_irq_handler      SysTick_Handler

    /* External interrupts */
    def_irq_handler     DMA_NS_Handler                /* 0: DMA NS Handler */
    def_irq_handler     ENCP_S_Handler                 /* 1: Encrypt enginee S Handler */
    def_irq_handler     ENCP_NS_Handler                /* 2: Encrypt enginee NS Handler */
    def_irq_handler     TFM_TIMER0_Handler             /* 3: TFM Timer Handler */
    def_irq_handler     UART0_Handler                  /* 15: TIMER 0 Handler */

    .section .text
    .global bfhfnmi_handler
    .type bfhfnmi_handler, %function
    bfhfnmi_handler:
    /* Infinite loop or other error handling */
    b .

