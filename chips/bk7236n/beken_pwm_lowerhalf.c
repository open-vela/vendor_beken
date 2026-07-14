/****************************************************************************
 * vendor/beken/chip/bk7236n/beken_pwm_lowerhalf.c
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
#include <nuttx/timers/pwm.h>
#include <nuttx/timers/capture.h>

#include <sys/types.h>
#include <stdint.h>
#include <assert.h>
#include <debug.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <arch/irq.h>
#include <stdio.h>

#include "driver/pwm_types.h"
#include "driver/pwm.h"
#include "beken_pwm_lowerhalf.h"

#ifdef CONFIG_PWM_PULSECOUNT
  #error CONFIG_PWM_PULSECOUNT Not supported
#endif

#ifdef CONFIG_PWM_MULTICHAN
  #error CONFIG_PWM_MULTICHAN Not supported
#endif

#ifdef CONFIG_PWM_DEADTIME
  #error CONFIG_PWM_DEADTIME Not supported
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* PWM clock source */
#define PWM_CLOCK_SOURCE (26000000)

/* PWM Channel number */
#define PWM_CH_NUM       (7)

/****************************************************************************
 * Private Types
 ****************************************************************************/
struct beken_pwm_dev_s
{
  const pwm_chan_t channels;
  bool used;
};

struct beken_cap_dev_s
{
  const struct cap_ops_s *ops_s;
  struct beken_pwm_dev_s *cap_dev;
};

struct beken_duty_dev_s
{
  const struct pwm_ops_s *ops_s;
  struct beken_pwm_dev_s *pwm_dev;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int beken_pwm_setup(struct pwm_lowerhalf_s *dev);
static int beken_pwm_shutdown(struct pwm_lowerhalf_s *dev);
static int beken_pwm_start(struct pwm_lowerhalf_s *dev,
                     const struct pwm_info_s *info);
static int beken_pwm_stop(struct pwm_lowerhalf_s *dev);
static int beken_pwm_ioctl(struct pwm_lowerhalf_s *dev, int cmd,
                     unsigned long arg);

static int capture_start(struct cap_lowerhalf_s *lower);
static int capture_stop(struct cap_lowerhalf_s *lower);
static int capture_getduty(struct cap_lowerhalf_s *lower, uint8_t *duty);
static int capture_getfreq(struct cap_lowerhalf_s *lower, uint32_t *freq);


/****************************************************************************
 * Private Variables
 ****************************************************************************/
static const struct pwm_ops_s g_pwm_ops =
{
  .setup       = beken_pwm_setup,
  .shutdown    = beken_pwm_shutdown,
  .start       = beken_pwm_start,
  .stop        = beken_pwm_stop,
  .ioctl       = beken_pwm_ioctl
};

static const struct cap_ops_s g_cap_ops =
{
  .start       = capture_start,
  .stop        = capture_stop,
  .getduty     = capture_getduty,
  .getfreq     = capture_getfreq,
};

static struct beken_pwm_dev_s g_pwmdev[PWM_CH_NUM] =
{
  {
   .channels    = PWM_ID_0,
   .used        = false,
  },
  {
   .channels    = PWM_ID_1,
   .used        = false,
  },
  {
   .channels    = PWM_ID_4,
   .used        = false,
  },
  {
   .channels    = PWM_ID_5,
   .used        = false,
  },
  {
   .channels    = PWM_ID_6,
   .used        = false,
  },
  {
   .channels    = PWM_ID_8,
   .used        = false,
  },
  {
   .channels    = PWM_ID_10,
   .used        = false,
  }
};

static struct beken_duty_dev_s g_dutydev[PWM_CH_NUM] =
{
  {
   .ops_s       = &g_pwm_ops,
   .pwm_dev     = &g_pwmdev[0]
  },
  {
   .ops_s       = &g_pwm_ops,
   .pwm_dev     = &g_pwmdev[1]
  },
  {
   .ops_s       = &g_pwm_ops,
   .pwm_dev     = &g_pwmdev[2]
  },
  {
   .ops_s       = &g_pwm_ops,
   .pwm_dev     = &g_pwmdev[3]
  },
  {
   .ops_s       = &g_pwm_ops,
   .pwm_dev     = &g_pwmdev[4]
  },
  {
   .ops_s       = &g_pwm_ops,
   .pwm_dev     = &g_pwmdev[5]
  },
  {
   .ops_s       = &g_pwm_ops,
   .pwm_dev     = &g_pwmdev[6]
  }
};

static struct beken_cap_dev_s g_capdev[PWM_CH_NUM] =
{
  {
   .ops_s       = &g_cap_ops,
   .cap_dev     = &g_pwmdev[0]
  },
  {
   .ops_s       = &g_cap_ops,
   .cap_dev     = &g_pwmdev[1]
  },
  {
   .ops_s       = &g_cap_ops,
   .cap_dev     = &g_pwmdev[2]
  },
  {
   .ops_s       = &g_cap_ops,
   .cap_dev     = &g_pwmdev[3]
  },
  {
   .ops_s       = &g_cap_ops,
   .cap_dev     = &g_pwmdev[4]
  },
  {
   .ops_s       = &g_cap_ops,
   .cap_dev     = &g_pwmdev[5]
  },
  {
   .ops_s       = &g_cap_ops,
   .cap_dev     = &g_pwmdev[6]
  }
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
/****************************************************************************
 * Name: capture_start
 *
 * Description:
 *   start a pwm capture
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of the
 *             "lower-half" driver state structure.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/
static int capture_start(struct cap_lowerhalf_s *lower)
{
  DEBUGASSERT(lower != NULL);

  struct  beken_cap_dev_s *priv = (struct  beken_cap_dev_s *)lower;

  int ret = OK;

  if (priv->cap_dev->used)
   {
     return -EBUSY;
   }

  pwm_capture_init_config_t config = {0};

  ret = bk_pwm_capture_init(priv->cap_dev->channels, &config);

  if (ret < 0)
   {
     return -ENODEV;
   }

  ret = bk_pwm_capture_start(priv->cap_dev->channels);

  if (ret < 0)
   {
     return -ENODEV;
   }

  priv->cap_dev->used = true;

  return OK;
}

/****************************************************************************
 * Name: capture_stop
 *
 * Description:
 *   stop a pwm capture
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of the
 *             "lower-half" driver state structure.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/
static int capture_stop(struct cap_lowerhalf_s *lower)
{
  DEBUGASSERT(lower != NULL);

  struct  beken_cap_dev_s *priv = (struct  beken_cap_dev_s *)lower;

  bk_pwm_capture_deinit(priv->cap_dev->channels);

  priv->cap_dev->used = false;

  return OK;
}

/****************************************************************************
 * Name: capture_getduty
 *
 * Description:
 *   get a pwm duty
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of the
 *             "lower-half" driver state structure.
 *   duty  - DutyCycle * 100.
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/
static int capture_getduty(struct cap_lowerhalf_s *lower, uint8_t *duty)
{
  DEBUGASSERT(lower != NULL);
  DEBUGASSERT(duty != NULL);

  struct  beken_cap_dev_s *priv = (struct  beken_cap_dev_s *)lower;

  uint32_t period;
  uint32_t pulse;

  irqstate_t flags = enter_critical_section();

  period = bk_pwm_capture_get_period_duty_cycle(priv->cap_dev->channels, 100);
  pulse  = bk_pwm_capture_get_value(priv->cap_dev->channels);

  if (period > 0)
    {
      *duty = (uint8_t)((pulse * 100ULL) / period);
    }
  else
    {
      *duty = 0;
    }

  leave_critical_section(flags);

  return OK;
}


/****************************************************************************
 * Name: capture_getfreq
 *
 * Description:
 *   get a pwm frequency
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of the
 *             "lower-half" driver state structure.
 *   freq  - Frequence in Hz .
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/
static int capture_getfreq(struct cap_lowerhalf_s *lower, uint32_t *freq)
{
  DEBUGASSERT(lower != NULL);
  DEBUGASSERT(freq != NULL);

  struct  beken_cap_dev_s *priv = (struct  beken_cap_dev_s *)lower;

  uint32_t period;

  irqstate_t flags = enter_critical_section();

  period = bk_pwm_capture_get_period_duty_cycle(priv->cap_dev->channels, 100);

  if (period > 0)
    {
      *freq = PWM_CLOCK_SOURCE / period;
    }
  else
    {
      *freq = 0;
    }

  leave_critical_section(flags);

  return OK;
}

/****************************************************************************
 * Name: beken_pwm_setup
 *
 * Description:
 *   This method is called when the driver is opened.  The lower half driver
 *   should configure and initialize the device so that it is ready for use.
 *   It should not, however, output pulses until the start method is called.
 *
 * Input Parameters:
 *   dev - A reference to the lower half PWM driver state structure
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure
 *
 ****************************************************************************/
static int beken_pwm_setup(struct pwm_lowerhalf_s *dev)
{
  DEBUGASSERT(dev != NULL);

  struct beken_duty_dev_s *priv = (struct beken_duty_dev_s *)dev;

  if (priv->pwm_dev->used)
   {
     return -EBUSY;
   }
  else
   {
     priv->pwm_dev->used = true;
     return OK;
   }
}

/****************************************************************************
 * Name: beken_pwm_shutdown
 *
 * Description:
 *   This method is called when the driver is closed.  The lower half driver
 *   stop pulsed output, free any resources, disable the timer hardware, and
 *   put the system into the lowest possible power usage state
 *
 * Input Parameters:
 *   dev - A reference to the lower half PWM driver state structure
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure
 *
 ****************************************************************************/
static int beken_pwm_shutdown(struct pwm_lowerhalf_s *dev)
{
  DEBUGASSERT(dev != NULL);

  struct beken_duty_dev_s *priv = (struct beken_duty_dev_s *)dev;

  priv->pwm_dev->used = false;

  return OK;
}

/****************************************************************************
 * Name: beken_pwm_start
 *
 * Description:
 *   (Re-)initialize the timer resources and start the pulsed output
 *
 * Input Parameters:
 *   dev  - A reference to the lower half PWM driver state structure
 *   info - A reference to the characteristics of the pulsed output
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure
 *
 ****************************************************************************/
static int beken_pwm_start(struct pwm_lowerhalf_s *dev,
                     const struct pwm_info_s *info)
{
  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(info != NULL);
  DEBUGASSERT(info->frequency != 0);

  struct beken_duty_dev_s *priv = (struct beken_duty_dev_s *)dev;
  int ret = OK;

  if (!priv->pwm_dev->used)
   {
     return -ENODEV;
   }

  unsigned int s_period = PWM_CLOCK_SOURCE/info->frequency;
  unsigned int duty = (unsigned int)(((float)(info->duty)/0xFFFF)*s_period);

  pwm_init_config_t init_config = {0};
  init_config.psc = 0,
  init_config.period_cycle = s_period;
  init_config.duty_cycle = duty;
  init_config.duty2_cycle = 0;
  init_config.duty3_cycle = 0;

  bk_pwm_deinit(priv->pwm_dev->channels);
  ret = bk_pwm_init(priv->pwm_dev->channels, &init_config);

  if (ret < 0)
   {
     return -ENODEV;
   }

  ret = bk_pwm_start(priv->pwm_dev->channels);
  if (ret < 0)
   {
     return -ENODEV;
   }

  return OK;
}

/****************************************************************************
 * Name: beken_pwm_stop
 *
 * Description:
 *   Stop the pulsed output and reset the timer resources.
 *
 * Input Parameters:
 *   dev - A reference to the lower half PWM driver state structure
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure
 *
 ****************************************************************************/
static int beken_pwm_stop(struct pwm_lowerhalf_s *dev)
{
  DEBUGASSERT(dev != NULL);

  struct beken_duty_dev_s *priv = (struct beken_duty_dev_s *)dev;
  int ret = OK;

  if (!priv->pwm_dev->used)
   {
     return -ENODEV;
   }

  ret = bk_pwm_deinit(priv->pwm_dev->channels);
  if (ret < 0)
   {
     return -ENODEV;
   }

  return OK;
}


/****************************************************************************
 * Name: beken_pwm_ioctl
 *
 * Description:
 *   Lower-half logic may support platform-specific ioctl commands
 *
 * Input Parameters:
 *   dev - A reference to the lower half PWM driver state structure
 *   cmd - The ioctl command
 *   arg - The argument accompanying the ioctl command
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure
 *
 ****************************************************************************/
static int beken_pwm_ioctl(struct pwm_lowerhalf_s *dev, int cmd,
                     unsigned long arg)
{
  DEBUGASSERT(dev != NULL);

  return -ENOTTY;
}

/****************************************************************************
 * Name: beken_pwm_init_ch
 *
 * Description:
 *   Initialize one pwm channel for use with the upper_level PWM driver.
 *
 * Input Parameters:
 *   ch - A ch to be initialized as pwm output channel.
 *
 * Returned Value:
 *   On success, a pointer to the lower half PWM driver is
 *   returned. NULL is returned on any failure.
 *
 ****************************************************************************/
struct pwm_lowerhalf_s * beken_pwm_init_ch(int ch)
{
  struct beken_duty_dev_s *lower = NULL;

  if (ch >= 0 && ch < PWM_CH_NUM)
   {
     lower = &g_dutydev[ch];
   }

  return (struct pwm_lowerhalf_s *)lower;

}

/****************************************************************************
 * Name: beken_cap_init_ch
 *
 * Description:
 *   Initialize one capture channel for use with the upper_level capture driver.
 *
 * Input Parameters:
 *   ch - A ch to be initialized as capture channel.
 *
 * Returned Value:
 *   On success, a pointer to the lower half capture driver is
 *   returned. NULL is returned on any failure.
 *
 ****************************************************************************/
struct cap_lowerhalf_s * beken_cap_init_ch(int ch)
{
  struct beken_cap_dev_s *lower = NULL;

  if (ch >= 0 && ch < PWM_CH_NUM)
   {
     lower = &g_capdev[ch];
   }

  return (struct cap_lowerhalf_s *)lower;
}

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
int beken_pwm_initialize(const char *devpath, int chan)
{
  int ret = OK;
  struct pwm_lowerhalf_s *  pwm_dev = beken_pwm_init_ch(chan);

  if (!pwm_dev)
   {
     return -ENODEV;
   }

  ret = pwm_register(devpath, pwm_dev);

  return ret;
}

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
int beken_capture_initialize(const char *devpath, int chan)
{
  int ret = OK;
  struct cap_lowerhalf_s *  cap_dev = beken_cap_init_ch(chan);

  if (!cap_dev)
   {
     return -ENODEV;
   }

  ret = cap_register(devpath, cap_dev);

  return ret;
}
