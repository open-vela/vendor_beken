/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_tim_lowerhalf.h
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
#ifndef __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_TIM_LOWERHALF_H
#define __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_TIM_LOWERHALF_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Private definetions */

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This structure describes the args of timer callback function.
 */

struct beken_arg
{
  void                      *arg;       /*The arguments of Vela callback function*/
  uint32_t                  timer_id;   /* The ID of timer*/
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
* Name: beken_timer_get_callback_arg
*
* Description:
*   Get the args of timer time out callback function
*
* Input Parameters:
*   NONE
*
* Returned Value:
*   NONE
*
****************************************************************************/

struct beken_arg * beken_timer_get_callback_arg(void);

/****************************************************************************
 * Name: beken_timer_initialize
 *
 * Description:
 *   Bind the configuration timer to a timer lower half instance and
 *   register the timer drivers at 'devpath'
 *
 * Input Parameters:
 *   devpath - The full path to the timer device.  This should be of the
 *             form /dev/timer0
 *   timer   - The timer's number.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/

int beken_timer_initialize(const char *devpath, int timer);

#endif /* __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_TIM_LOWERHALF_H */

