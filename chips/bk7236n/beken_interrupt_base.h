/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_interrupt_base.h
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
#ifndef __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_INTERRUPT_BASE_H
#define __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_INTERRUPT_BASE_H

#pragma once
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#include <common/bk_err.h>
#include "driver/int_types.h"

#define BK_ERR_INT_DEVICE_NONE                  (BK_ERR_INT_BASE - 1) /**< icu device number is invalid */
#define BK_ERR_INT_NOT_EXIST                    (BK_ERR_INT_BASE - 2) /**< icu device number is invalid */

typedef struct {
  icu_int_src_t src;
  uint8_t int_bit;
  uint8_t int_prio;
  uint8_t group;
} icu_int_map_t;

typedef void (*isr_func_t) (void);

/****************************************************************************
* Public Functions
****************************************************************************/

/****************************************************************************
* Name: bk_int_isr_register
*
* Description:
*   Initialize the hardware RTC per the selected configuration.  This
*   function is called once during the OS initialization sequence
*
* Input Parameters:
*   src:The interrupt source id.
*   isr_callback:The callback function of interrupt src.
*   arg:The arguments of the callback function .
*
* Returned Value:
*   Zero (OK) on success; a negated errno on failure
*
****************************************************************************/

int bk_int_isr_register(icu_int_src_t src, isr_func_t isr_callback, void *arg);

/****************************************************************************
* Name: bk_int_isr_unregister
*
* Description:
*   Unregister the isr
*
* Input Parameters:
*   src:The interrupt source id.
*
* Returned Value:
*   Zero (OK) on success; a negated errno on failure
*
****************************************************************************/

int bk_int_isr_unregister(icu_int_src_t src);

/****************************************************************************
* Name: bk_get_int_number
*
* Description:
*   Get the priority number of intterrupt
*
* Input Parameters:
*   src:The interrupt source id.
*
* Returned Value:
*   The priority number
*
****************************************************************************/

int32_t bk_get_int_number(icu_int_src_t src);

#endif /* __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_RTC_LOWERHALF_H */
