/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_spi.h
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
#ifndef __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_SPI_H
#define __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_SPI_H

#ifdef CONFIG_SPI_DRIVER

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <soc/bk7236n/spi_cap.h>
#include <sdkconfig.h>
#include <driver/hal/hal_spi_types.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_spibus_initialize
 *
 * Description:
 *   Initialize the selected SPI bus.
 *
 * Input Parameters:
 *   port     - Port number (for hardware that has multiple SPI interfaces)
 *
 * Returned Value:
 *   Valid SPI device structure reference on success; NULL on failure.
 *
 ****************************************************************************/

struct spi_dev_s *beken_spibus_initialize(spi_id_t port);

/****************************************************************************
 * Name: beken_spibus_uninitialize
 *
 * Description:
 *   Uninitialize an SPI bus.
 *
 * Input Parameters:
 *   dev      - Device-specific state data
 *
 * Returned Value:
 *   Zero (OK) is returned on success. Otherwise -1 (ERROR).
 *
 ****************************************************************************/

int beken_spibus_uninitialize(struct spi_dev_s *dev);

#endif
#endif/* __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_SPI_H */

