/****************************************************************************
 * vendor/beken/boards/bk7236n/bk7236n-evb/src/beken_board_i2cdev.h
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
#ifndef __VENDOR_BEKEN_BOARDS_BK7236N_BK7236N_EVB_SRC_BEKEN_BOARD_I2CDEV_H
#define __VENDOR_BEKEN_BOARDS_BK7236N_BK7236N_EVB_SRC_BEKEN_BOARD_I2CDEV_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: board_i2cdev_initialize
 *
 * Description:
 *   Initialize I2C driver and register the /dev/i2c device.
 *
 * Input Parameters:
 *   chan - The I2C channel number, used to build the device path as /dev/i2cN
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/

#if defined(CONFIG_I2C_DRIVER)
int board_i2cdev_initialize(int chan);
#endif

#endif /* __VENDOR_BEKEN_BOARDS_BK7236N_BK7236N_EVB_SRC_BEKEN_BOARD_I2CDEV_H */
