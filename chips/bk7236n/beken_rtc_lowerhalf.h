/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_rtc_lowerhalf.h
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
#ifndef __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_RTC_LOWERHALF_H
#define __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_RTC_LOWERHALF_H

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_rtc_initialize
 *
 * Description:
 *   Initialize the hardware RTC per the selected configuration.  This
 *   function is called once during the OS initialization sequence
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure
 *
 ****************************************************************************/

int up_rtc_initialize(void);

/****************************************************************************
 * Name: beken_rtc_lowerhalf
 *
 * Description:
 *   Instantiate the RTC lower half driver for the BK7236.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Return the corresponding alarm id of alarm name
 *
 ****************************************************************************/

struct rtc_lowerhalf_s *beken_rtc_lowerhalf(void);

#endif/* __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_RTC_LOWERHALF_H */
