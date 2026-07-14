/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_adc_lowerhalf.c
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
#include <nuttx/analog/ioctl.h>
#include <nuttx/analog/adc.h>

#include "driver/adc_types.h"
#include "driver/adc.h"
#include "beken_adc_lowerhalf.h"

#if defined(CONFIG_ADC)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define ADC_READ_TIMEOUT      (20000)
#define ADC_DETEC_CLK         (750000)
#define ADC_DETEC_SAMPLE_RATE (32)
#define ADC_DETEC_STEADY_CTRL (7)
#define ADC_CHANNEL_NUM       (7)

/****************************************************************************
 * Private Types
 ****************************************************************************/
struct beken_adc_chan_s
{
  uint32_t ref;                     /* Reference count */
  const uint8_t channel;            /* Channel number */
  const struct adc_callback_s *cb;  /* Upper driver callback */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  beken_adc_bind(struct adc_dev_s *dev,
                     const struct adc_callback_s *callback);
static void beken_adc_reset(struct adc_dev_s *dev);
static int  beken_adc_setup(struct adc_dev_s *dev);
static void beken_adc_shutdown(struct adc_dev_s *dev);
static void beken_adc_rxint(struct adc_dev_s *dev, bool enable);
static int  beken_adc_ioctl(struct adc_dev_s *dev, int cmd, unsigned long arg);

/****************************************************************************
 * Private Variables
 ****************************************************************************/
static const struct adc_ops_s g_adcops =
{
  .ao_bind        = beken_adc_bind,
  .ao_reset       = beken_adc_reset,
  .ao_setup       = beken_adc_setup,
  .ao_shutdown    = beken_adc_shutdown,
  .ao_rxint       = beken_adc_rxint,
  .ao_ioctl       = beken_adc_ioctl,
};

static struct beken_adc_chan_s g_adc_chan[ADC_CHANNEL_NUM] =
{
  {
    .ref     = 0,
    .channel = ADC_1,
    .cb      = NULL
  },
  {
    .ref     = 0,
    .channel = ADC_2,
    .cb      = NULL
  },
  {
    .ref     = 0,
    .channel = ADC_4,
    .cb      = NULL
  },
  {
    .ref     = 0,
    .channel = ADC_12,
    .cb      = NULL
  },
  {
    .ref     = 0,
    .channel = ADC_13,
    .cb      = NULL
  },
  {
    .ref     = 0,
    .channel = ADC_14,
    .cb      = NULL
  },
  {
    .ref     = 0,
    .channel = ADC_15,
    .cb      = NULL
  }
};

static struct adc_dev_s g_adc_dev[ADC_CHANNEL_NUM] =
{
  {
    .ad_ops  = &g_adcops,
    .ad_priv = &g_adc_chan[0]
  },
  {
    .ad_ops  = &g_adcops,
    .ad_priv = &g_adc_chan[1]
  },
  {
    .ad_ops  = &g_adcops,
    .ad_priv = &g_adc_chan[2]
  },
  {
    .ad_ops  = &g_adcops,
    .ad_priv = &g_adc_chan[3]
  },
  {
    .ad_ops  = &g_adcops,
    .ad_priv = &g_adc_chan[4]
  },
  {
    .ad_ops  = &g_adcops,
    .ad_priv = &g_adc_chan[5]
  },
  {
    .ad_ops  = &g_adcops,
    .ad_priv = &g_adc_chan[6]
  }
};

static mutex_t g_lock = NXMUTEX_INITIALIZER;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_adc_read
 *
 * Description:
 *   Read ADC value and pass it to up.
 *
 * Input Parameters:
 *   dev - ADC device pointer
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/
static void beken_adc_read(struct adc_dev_s *dev)
{
  int ret;
  uint16_t sample_value;
  int32_t vol_value;

  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(dev->ad_priv != NULL);

  struct beken_adc_chan_s *priv = (struct beken_adc_chan_s *)dev->ad_priv;

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return;
    }

  bk_adc_read(&sample_value, ADC_READ_TIMEOUT);
  vol_value = (int32_t)(1000*bk_adc_data_calculate(sample_value, priv->channel));

  priv->cb->au_receive(dev, priv->channel, vol_value);

  nxmutex_unlock(&g_lock);
}

/****************************************************************************
 * Name: beken_adc_bind
 *
 * Description:
 *   Bind the upper-half driver callbacks to the lower-half implementation.
 *   This must be called early in order to receive ADC event notifications.
 *
 * Input Parameters:
 *   dev - pointer to device structure used by the driver
 *   callback - callback for upper
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/
static int beken_adc_bind(struct adc_dev_s *dev,
                    const struct adc_callback_s *callback)
{
  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(dev->ad_priv != NULL);
  DEBUGASSERT(callback != NULL);

  struct beken_adc_chan_s *priv = (struct beken_adc_chan_s *)dev->ad_priv;

  priv->cb = callback;

  return OK;
}

/****************************************************************************
 * Name: beken_adc_reset
 *
 * Description:
 *   Reset the ADC device.  Called early to initialize the hardware.
 *   This is called, before adc_setup() and on error conditions.
 *
 * Input Parameters:
 *   dev - pointer to device structure used by the driver
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/
static void beken_adc_reset(struct adc_dev_s *dev)
{
  irqstate_t flags;

  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(dev->ad_priv != NULL);

  struct beken_adc_chan_s *priv = (struct beken_adc_chan_s *)dev->ad_priv;

  flags = enter_critical_section();

  for (int i = 0; i < ADC_CHANNEL_NUM; i++)
    {
      if (priv->channel != g_adc_chan[i].channel)
      {
        if (g_adc_chan[i].ref > 0)
          {
            goto out;
          }
      }
    }

  /* Do nothing if ADC instance is currently in use */

  if (priv->ref > 0)
    {
      goto out;
    }

  bk_adc_stop();
out:
  leave_critical_section(flags);
}

/****************************************************************************
 * Name: beken_adc_setup
 *
 * Description:
 *   Configure the ADC. This method is called the first time that the ADC
 *   device is opened.  This will occur when the port is first opened.
 *   This setup includes configuring.
 *
 * Input Parameters:
 *   dev - pointer to device structure used by the driver
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/
static int beken_adc_setup(struct adc_dev_s *dev)
{
  int ret;

  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(dev->ad_priv != NULL);

  struct beken_adc_chan_s *priv = (struct beken_adc_chan_s *)dev->ad_priv;

  for (int i = 0; i < ADC_CHANNEL_NUM; i++)
    {
      if (priv->channel != g_adc_chan[i].channel)
      {
        if (g_adc_chan[i].ref > 0)
          {
            return -EBUSY;
          }
      }
    }

  /* Do nothing when the ADC device is already set up */

  if (priv->ref > 0)
    {
      priv->ref++;
      return OK;
    }

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  adc_config_t adc_config;

  adc_config.chan = priv->channel;
  adc_config.adc_mode = ADC_CONTINUOUS_MODE;
  adc_config.clk = ADC_DETEC_CLK;
  adc_config.src_clk = ADC_SCLK_XTAL;
  adc_config.saturate_mode = ADC_SATURATE_MODE_3;
  adc_config.sample_rate = ADC_DETEC_SAMPLE_RATE;
  adc_config.steady_ctrl = ADC_DETEC_STEADY_CTRL;
  adc_config.adc_filter = 0;

  bk_adc_init(priv->channel);
  bk_adc_set_config(&adc_config);
  bk_adc_enable_bypass_clalibration();

  nxmutex_unlock(&g_lock);

  priv->ref++;

  return OK;
}

/****************************************************************************
 * Name: beken_adc_rxint
 *
 * Description:
 *   Call to enable or disable RX interrupts.
 *
 * Input Parameters:
 *   dev - pointer to device structure used by the driver
 *   enable - enable adc sampling interrupt
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/
static void beken_adc_rxint(struct adc_dev_s *dev, bool enable)
{
}

/****************************************************************************
 * Name: beken_adc_ioctl
 *
 * Description:
 *   All ioctl calls will be routed through this method.
 *
 * Input Parameters:
 *   dev - pointer to device structure used by the driver
 *   cmd - command
 *   arg - arguments passed with command
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/
static int beken_adc_ioctl(struct adc_dev_s *dev, int cmd, unsigned long arg)
{
  int ret;

  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(dev->ad_priv != NULL);

  switch (cmd)
    {
      case ANIOC_TRIGGER:
        {
          bk_adc_start();
          beken_adc_read(dev);
          ret = OK;
        }
        break;

      case ANIOC_GET_NCHANNELS:
        {
          ret = 1;
        }
        break;

      default:
        {
          ret = -ENOTTY;
        }
        break;
    }

  return ret;
}

/****************************************************************************
 * Name: beken_adc_shutdown
 *
 * Description:
 *   Disable the ADC.  This method is called when the ADC device is closed.
 *   This method reverses the operation the setup method.
 *
 * Input Parameters:
 *   dev - pointer to device structure used by the driver
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/
static void beken_adc_shutdown(struct adc_dev_s *dev)
{
  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(dev->ad_priv != NULL);

  struct beken_adc_chan_s *priv = (struct beken_adc_chan_s *)dev->ad_priv;

  /* Decrement count only when ADC device is in use */

  if (priv->ref > 0)
    {
      priv->ref--;

  /* Shutdown the ADC device only when not in use */

      if (!priv->ref)
        {
          bk_adc_stop();
          bk_adc_deinit(priv->channel);
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_adc_init_ch
 *
 * Description:
 *   Initialize the ADC.
 *
 * Input Parameters:
 *   ch - ADC channel number
 *
 * Returned Value:
 *   adc struct of device lower
 *
 ****************************************************************************/
struct adc_dev_s *beken_adc_init_ch(int ch)
{
  struct adc_dev_s *dev = NULL;

  if (ch >= 0 && ch < ADC_CHANNEL_NUM)
    {
      dev = &g_adc_dev[ch];
    }

  return dev;
}

/****************************************************************************
 * Name: beken_adc_initialize
 *
 * Description:
 *   Bind the configuration adc channel to an adc lower half instance.
 *
 * Input Parameters:
 *   devpath - The full path to the adc channel.  This should be of the
 *             form /dev/adc0
 *   chan    - the chan's number.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/
int beken_adc_initialize(const char *devpath, int chan)
{
  int ret = OK;
  struct adc_dev_s * lower = NULL;

  lower = beken_adc_init_ch(chan);
  if (!lower)
   {
     return -ENODEV;
   }

  ret = adc_register(devpath,lower);
  return ret;
}

#endif
