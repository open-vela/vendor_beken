/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_trng_lowerhalf.c
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

#include <nuttx/config.h>

#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/fs/fs.h>
#include <nuttx/drivers/drivers.h>

#include "driver/trng_types.h"
#include "driver/trng.h"

#if defined(CONFIG_DEV_RANDOM) || defined(CONFIG_DEV_URANDOM_ARCH)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static ssize_t beken_rng_read(struct file *filep, char *buffer,
                                size_t buflen);

/****************************************************************************
 * Private Types
 ****************************************************************************/
struct rng_dev_s
{
  uint8_t *rd_buf;
  mutex_t  rd_lock;         /* mutex for read RNG data */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/
static struct rng_dev_s g_rngdev =
{
  .rd_lock = NXMUTEX_INITIALIZER,
};

static const struct file_operations g_rngops =
{
  .read = beken_rng_read,       /* read */
};

/****************************************************************************
 * Private functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_rng_read
 ****************************************************************************/
static ssize_t beken_rng_read(struct file *filep, char *buffer,
                              size_t buflen)
{
  struct rng_dev_s *priv = (struct rng_dev_s *)&g_rngdev;
  ssize_t read_len;
  uint8_t *rd_buf = (uint8_t *)buffer;

  if (nxmutex_lock(&priv->rd_lock) != OK)
    {
      return -EBUSY;
    }

  read_len = buflen;

  /* Wait until the buffer is filled */

  while (buflen > 0)
    {
      uint32_t word = (uint32_t)bk_rand();
      uint32_t to_copy = MIN(sizeof(word), buflen);

      memcpy(rd_buf, &word, to_copy);
      rd_buf += to_copy;
      buflen -= to_copy;
    }

  /* Release rd_lock for next read */

  nxmutex_unlock(&priv->rd_lock);
  return read_len;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: devrandom_register
 *
 * Description:
 *   Initialize the RNG hardware and register the /dev/random driver.
 *   Must be called BEFORE devurandom_register.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/
#ifdef CONFIG_DEV_RANDOM
void devrandom_register(void)
{
  register_driver("/dev/random", &g_rngops, 0444, NULL);
}
#endif

/****************************************************************************
 * Name: devurandom_register
 *
 * Description:
 *   Register /dev/urandom
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/
#ifdef CONFIG_DEV_URANDOM_ARCH
void devurandom_register(void)
{
  register_driver("/dev/urandom", &g_rngops, 0444, NULL);
}
#endif
#endif
