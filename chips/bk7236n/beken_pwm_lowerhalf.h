/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_pwm_lowerhalf.h
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
#ifndef __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_PWM_LOWERHALF_H
#define __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_PWM_LOWERHALF_H

#include <nuttx/config.h>

/****************************************************************************
 * Name: beken_pwm_initialize
 *
 * Description:
 *   Bind the configuration pwm channel to a pwm lower half instance and
 *   register the pwm drivers at 'devpath'
 *
 * Input Parameters:
 *   devpath - The full path to the pwm device.  This should be of the
 *             form /dev/pwm0
 *   chan    - the chan's number.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/
int beken_pwm_initialize(const char *devpath, int chan);

/****************************************************************************
 * Name: beken_capture_initialize
 *
 * Description:
 *   Bind the configuration pwm channel to a capture lower half instance and
 *   register the capture drivers at 'devpath'
 *
 * Input Parameters:
 *   devpath - The full path to the capture device.  This should be of the
 *             form /dev/capture0
 *   chan    - the chan's number.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/
int beken_capture_initialize(const char *devpath, int chan);

#endif /* __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_PWM_LOWERHALF_H */

