/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_oneshot_lowerhalf.c
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

#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <nuttx/kmalloc.h>
#include <nuttx/timers/oneshot.h>

#include "driver/timer.h"
#include "beken_tim_lowerhalf.h"
#include "beken_oneshot_lowerhalf.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Private definetions */

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This structure describes the state of the oneshot timer lower-half driver
 */

struct beken_oneshot_lowerhalf_s
{
  /* This is the part of the lower half driver that is visible to the upper-
   * half client of the driver.  This must be the first thing in this
   * structure so that pointers to struct oneshot_lowerhalf_s are cast
   * compatible to struct bl602_oneshot_lowerhalf_s and vice versa.
   */

  struct oneshot_lowerhalf_s lh;       /* Common lower-half driver fields */
  uint32_t                   freq;     /* The frequency of timer clock */
  uint64_t                   timeout;  /* The timeout value of the oneshot timer */

  /* Private lower half data follows */

  uint8_t                    timer_id; /* timer id 0,1 */
  uint8_t                    int_flag; /* The flag indicates whether the oneshot timer times out */
  bool                       started;  /* True: Timer has been started */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int beken_oneshot_max_delay(struct oneshot_lowerhalf_s *lower,
                                             struct timespec * ts);
static int beken_oneshot_start(struct oneshot_lowerhalf_s *lower,
                                       const struct timespec *ts);
static int beken_oneshot_cancel(struct oneshot_lowerhalf_s *lower,
                                         struct timespec * ts);
static int beken_oneshot_current(FAR struct oneshot_lowerhalf_s *lower,
                                          FAR struct timespec *ts);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Lower half operations */

static const struct oneshot_operations_s g_oneshot_ops =
{
  .max_delay = beken_oneshot_max_delay,
  .start     = beken_oneshot_start,
  .cancel    = beken_oneshot_cancel,
  .current   = beken_oneshot_current,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_oneshot_callback_wrapper
 *
 * Description:
 *   Timer expiration handler
 *
 * Input Parameters:
 *   next_interval - The value of next interval.
 *   arg           - Should be the same argument provided when beken_oneshot_start()
 *                   was called.
 *
 * Returned Value:
 *   True: The function is complete.
 *
 ****************************************************************************/

static void beken_oneshot_callback_wrapper(timer_id_t timer_id)
{
  struct beken_arg *arg_input_temp = &beken_timer_get_callback_arg()[timer_id];
  struct beken_oneshot_lowerhalf_s *priv =
    (struct beken_oneshot_lowerhalf_s *)arg_input_temp->arg;
  struct timespec ts;

  priv->int_flag = 1;

  beken_oneshot_cancel((struct oneshot_lowerhalf_s *)(priv), &ts);
  oneshot_process_callback(&priv->lh);
}

/****************************************************************************
 * Name: beken_oneshot_current
 *
 * Description:
 *   Get the oneshot current counts.
 *
 * Input Parameters:
 *   arg - Should be the same argument provided when beken_oneshot_start()
 *         was called.
 *
 * Returned Value:
 *   True: The function is complete.
 *
 ****************************************************************************/

static int beken_oneshot_current(FAR struct oneshot_lowerhalf_s *lower,
                                          FAR struct timespec *ts)
{
  struct beken_oneshot_lowerhalf_s *priv = (struct beken_oneshot_lowerhalf_s *)lower;
  uint32_t current_cnt;
  uint32_t usecs;

  current_cnt = bk_timer_get_cnt(priv->timer_id);
  usecs = (uint64_t)(current_cnt * (uint64_t)USEC_PER_SEC / priv->freq) ;

  if(priv->int_flag)
    {
      usecs += priv->timeout;
    }

  uint64_t sec = (uint64_t)(usecs / USEC_PER_SEC);
  usecs -= USEC_PER_SEC * sec;

  ts->tv_sec  = (time_t)sec;
  ts->tv_nsec = (long)(usecs * 1000);

  return OK;
}

/****************************************************************************
 * Name: beken_oneshot_max_delay
 *
 * Description:
 *   Determine the maximum delay of the one-shot timer (in microseconds)
 *
 * Input Parameters:
 *   lower - An instance of the lower-half oneshot state structure.  This
 *           structure must have been previously initialized via a call to
 *           oneshot_initialize();
 *   ts    - The location in which to return the maximum delay.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned
 *   on failure.
 *
 ****************************************************************************/

static int beken_oneshot_max_delay(struct oneshot_lowerhalf_s *lower,
                                             struct timespec * ts)
{
  struct beken_oneshot_lowerhalf_s *priv = (struct beken_oneshot_lowerhalf_s *)lower;
  uint64_t usecs;

  DEBUGASSERT(priv != NULL && ts != NULL);
  usecs = (uint64_t)(UINT32_MAX * (uint64_t)USEC_PER_SEC / priv->freq) ;

  uint64_t sec = (uint64_t)(usecs / USEC_PER_SEC);
  usecs       -= USEC_PER_SEC * sec;

  ts->tv_sec  = (time_t)sec;
  ts->tv_nsec = (long)(usecs * 1000);

  return OK;
}

/****************************************************************************
 * Name: beken_oneshot_start
 *
 * Description:
 *   Start the oneshot timer
 *
 * Input Parameters:
 *   lower   - An instance of the lower-half oneshot state structure.  This
 *             structure must have been previously initialized via a call to
 *             oneshot_initialize();
 *   handler - The function to call when when the oneshot timer expires.
 *   arg     - An opaque argument that will accompany the callback.
 *   ts      - Provides the duration of the one shot timer.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned
 *   on failure.
 *
 ****************************************************************************/

static int beken_oneshot_start(struct oneshot_lowerhalf_s *lower,
                                       const struct timespec *ts)
{
  struct beken_oneshot_lowerhalf_s *priv = (struct beken_oneshot_lowerhalf_s *)lower;
  irqstate_t flags;
  uint64_t   usec;
  int        ret;

  DEBUGASSERT(priv != NULL && ts != NULL);

  if (priv->started == true)
    {
      /* Yes.. then cancel it */

      tmrinfo("Already running... cancelling\n");
      beken_oneshot_cancel(lower, NULL);
    }

  flags = enter_critical_section();

  /* Express the delay in microseconds */

  usec = (uint64_t)ts->tv_sec * USEC_PER_SEC +
         (uint64_t)(ts->tv_nsec / NSEC_PER_USEC);

  struct beken_arg *arg_info = beken_timer_get_callback_arg();

  arg_info[priv->timer_id].arg      = (void *)priv;
  arg_info[priv->timer_id].timer_id = priv->timer_id;

  priv->timeout  = usec;
  priv->int_flag = 0;

  /* To avoid the tim_lowerhalf use the same timer id. */

  bk_timer_stop(priv->timer_id);
  ret = bk_timer_delay_with_callback(priv->timer_id, usec,
                                     beken_oneshot_callback_wrapper);
  if (ret == OK)
    {
      priv->started = true;
    }

  leave_critical_section(flags);

  return ret;
}

/****************************************************************************
 * Name: beken_oneshot_cancel
 *
 * Description:
 *   Cancel the oneshot timer and return the time remaining on the timer.
 *
 *   NOTE: This function may execute at a high rate with no timer running (as
 *   when pre-emption is enabled and disabled).
 *
 * Input Parameters:
 *   lower - Caller allocated instance of the oneshot state structure.  This
 *           structure must have been previously initialized via a call to
 *           oneshot_initialize();
 *   ts    - The location in which to return the time remaining on the
 *           oneshot timer.  A time of zero is returned if the timer is
 *           not running.
 *
 * Returned Value:
 *   Zero (OK) is returned on success.  A call to up_timer_cancel() when
 *   the timer is not active should also return success; a negated errno
 *   value is returned on any failure.
 *
 ****************************************************************************/

static int beken_oneshot_cancel(struct oneshot_lowerhalf_s *lower,
                                        struct timespec *ts)
{
  struct beken_oneshot_lowerhalf_s *priv = (struct beken_oneshot_lowerhalf_s *)lower;
  int rtn_value = -ENODEV;

  DEBUGASSERT(priv != NULL);

  /* Cancel the timer */

  if (priv->started)
    {
      rtn_value = bk_timer_stop(priv->timer_id);

      if(rtn_value == OK)
        {
          priv->started = false;
        }
      else
        {
          return -ENODEV;
        }

      rtn_value = bk_timer_cancel(priv->timer_id);

      if(rtn_value == OK)
        {
          return OK;
        }

    }

  return -ENODEV;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: oneshot_initialize
 *
 * Description:
 *   Initialize the oneshot timer and return a oneshot lower half driver
 *   instance.
 *
 * Input Parameters:
 *   chan       - Timer counter channel to be used.
 *   resolution - The required resolution of the timer in units of
 *                microseconds.  NOTE that the range is restricted to the
 *                range of uint16_t (excluding zero).
 *
 * Returned Value:
 *   On success, a non-NULL instance of the oneshot lower-half driver is
 *   returned.  NULL is return on any failure.
 *
 ****************************************************************************/

struct oneshot_lowerhalf_s *oneshot_initialize(int         chan,
                                                       uint16_t resolution)
{
  struct beken_oneshot_lowerhalf_s *priv;

  /* Allocate an instance of the lower half driver */

  priv = kmm_zalloc(sizeof(struct beken_oneshot_lowerhalf_s));
  if (priv == NULL)
    {
      tmrerr("ERROR: Failed to initialized state structure\n");
      return NULL;
    }

  /* Initialize the lower-half driver structure */

  priv->started     = false;
  priv->lh.ops      = &g_oneshot_ops;
  priv->freq        = TIMER_CLOCK_FREQ_XTAL * 1000;
  priv->timer_id    = chan;

  return &priv->lh;
}

