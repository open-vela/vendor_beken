/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_i2c_lowerhalf.c
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
#include <nuttx/i2c/i2c_master.h>

#include "soc/bk7236n/i2c_cap.h"
#include "driver/i2c_types.h"
#include "driver/i2c.h"
#include "beken_i2c_lowerhalf.h"

#if defined(CONFIG_I2C_DRIVER)

#ifdef CONFIG_I2C_POLLED
  #error CONFIG_I2C_POLLED Not supported
#endif

#ifdef CONFIG_I2C_TRACE
  #error CONFIG_I2C_TRACE Not supported
#endif

#ifdef CONFIG_I2C_RESET
  #error CONFIG_I2C_RESET Not supported
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/
struct beken_i2c_priv_s
{
  const struct i2c_ops_s *ops;

  uint32_t id;/* I2C instance */
  uint32_t wait_time;/* write or Read waittime(ms) */
  mutex_t lock;/* Mutual exclusion mutex */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int beken_i2c_transfer(struct i2c_master_s *dev,struct i2c_msg_s *msgs, int count);

/****************************************************************************
 * Private Variables
 ****************************************************************************/
/* I2C interface */
static const struct i2c_ops_s beken_i2c_ops =
{
  .transfer = beken_i2c_transfer
#ifdef CONFIG_I2C_RESET
  , .reset  = NULL
#endif
};

/* I2C device structures */
static struct beken_i2c_priv_s beken_i2c0_priv =
{
  .ops        = &beken_i2c_ops,
  .id         = I2C_ID_0,
  .wait_time  = 500, /* Ms */
  .lock       = NXMUTEX_INITIALIZER,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
/****************************************************************************
 * Name: beken_i2c_transfer
 *
 * Description:
 *   send messages to slave
 *
 * Input Parameters:
 *   dev   - A pointer the publicly visible representation of the
 *             "lower-half" driver state structure.
 *   msgs  - messages to send.
 *   count  - Number of messages to send.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/
static int beken_i2c_transfer(struct i2c_master_s *dev,struct i2c_msg_s *msgs, int count)
{
  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(count > 0);
  struct beken_i2c_priv_s *priv = (struct beken_i2c_priv_s *)dev;

  int ret    = OK;
  int ret_op = BK_OK;

  int i;

  ret = nxmutex_lock(&priv->lock);

  if (ret < 0)
   {
     return ret;
   }

  for (i = 0; i < count; i++)
    {
      struct i2c_msg_s *msg = &msgs[i];

      if (msg->flags & I2C_M_READ)
        {
          ret_op = bk_i2c_master_read(priv->id, msg->addr, msg->buffer,
                                      msg->length, priv->wait_time);
        }
      else
        {
          ret_op = bk_i2c_master_write(priv->id, msg->addr, msg->buffer,
                                       msg->length, priv->wait_time);
        }

      if (ret_op != BK_OK)
        {
          break;
        }
    }

  ret = nxmutex_unlock(&priv->lock);

  if (ret < 0)
   {
     return ret;
   }

  switch (ret_op)
   {
     case BK_OK:
       return OK;
     case BK_ERR_I2C_SM_BUS_BUSY:
       return -EBUSY;
     case BK_ERR_I2C_ACK_TIMEOUT:
     case BK_ERR_I2C_SCL_TIMEOUT:
       return -ETIMEDOUT;
     default:
       return -EIO;
   }
}

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
struct i2c_master_s * beken_i2c_init_ch(int ch)
{
  struct beken_i2c_priv_s *lower = NULL;

  switch (ch)
   {
     case 0:
     {
       lower = &beken_i2c0_priv;
       break;
     }
     default:
     {
       lower = NULL;
       break;
     }
   }

  return (struct i2c_master_s *)lower;
}

#endif
