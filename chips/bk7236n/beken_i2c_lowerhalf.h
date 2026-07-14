/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_i2c_lowerhalf.h
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
#ifndef __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_I2C_LOWERHALF_H
#define __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_I2C_LOWERHALF_H

#include <nuttx/config.h>

/****************************************************************************
 * Name: beken_i2c_init_ch
 *
 * Description:
 *   Initialize one i2c master channel for use with the upper_level I2C driver.
 *
 * Input Parameters:
 *   ch - A ch to be initialized as i2c master channel.
 *
 * Returned Value:
 *   A lower device of struct i2c_master_s
 *
 ****************************************************************************/
struct i2c_master_s * beken_i2c_init_ch(int ch);

#endif /* __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_I2C_LOWERHALF_H */

