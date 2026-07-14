/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_tim_lowerhalf.c
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <nuttx/timers/timer.h>
#include <nuttx/clock.h>
#include <arch/board/board.h>

#include "driver/timer.h"
#include "beken_tim_lowerhalf.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define BEKEN_TIMER_MAX_CNT 0xffffffff

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This structure provides the private representation of the "lower-half"
 * driver state structure.  This structure must be cast-compatible with the
 * timer_lowerhalf_s structure.
 */

struct beken_lowerhalf_s
{
  const struct timer_ops_s *ops;        /* Lower half operations */
  tccb_t                    callback;   /* Current user interrupt callback */
  void                     *arg;        /* Argument passed to upper half callback */
  bool                      started;    /* True: Timer has been started */
  uint32_t                  timer_id;   /* The ID of timer*/
  uint8_t                   flag;       /* The status of TCFLAGS_* */
  uint32_t                  timeout;    /* The current timeout * */
};

static struct beken_arg g_arg_input[SOC_TIMER_CHAN_NUM_PER_UNIT];

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int beken_tim_start(struct timer_lowerhalf_s *lower);
static int beken_tim_stop(struct timer_lowerhalf_s *lower);
static int beken_tim_getstatus(struct timer_lowerhalf_s *lower,
                                        struct timer_status_s *status);
static int beken_tim_settimeout(struct timer_lowerhalf_s *lower,
                                         uint32_t timeout);
static void beken_tim_setcallback(struct timer_lowerhalf_s *lower,
                                          tccb_t callback,void *arg);
static int beken_tim_maxtimeout(struct timer_lowerhalf_s *lower,
                                          uint32_t *maxtimeout);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* "Lower half" driver methods */

static const struct timer_ops_s g_timer_ops =
{
  .start       = beken_tim_start,
  .stop        = beken_tim_stop,
  .getstatus   = beken_tim_getstatus,
  .settimeout  = beken_tim_settimeout,
  .setcallback = beken_tim_setcallback,
  .maxtimeout  = beken_tim_maxtimeout,
  .ioctl       = NULL,
};

static struct beken_lowerhalf_s g_tim0_lowerhalf =
{
  .ops         = &g_timer_ops,
  .timer_id    = TIMER_ID0,
};

static struct beken_lowerhalf_s g_tim1_lowerhalf =
{
  .ops         = &g_timer_ops,
  .timer_id    = TIMER_ID1,
};

static struct beken_lowerhalf_s g_tim2_lowerhalf =
{
  .ops         = &g_timer_ops,
  .timer_id    = TIMER_ID2,
};

static struct beken_lowerhalf_s g_tim3_lowerhalf =
{
  .ops         = &g_timer_ops,
  .timer_id    = TIMER_ID3,
};

static struct beken_lowerhalf_s g_tim4_lowerhalf =
{
  .ops         = &g_timer_ops,
  .timer_id    = TIMER_ID4,
};

static struct beken_lowerhalf_s g_tim5_lowerhalf =
{
  .ops         = &g_timer_ops,
  .timer_id    = TIMER_ID5,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
* Name: beken_tim_callback_wrapper
*
* Description:
*   Timer expiration handler
*
* Input Parameters:
*   next_interval- The value of next interval.
*   arg          - Should be the same argument provided when beken_tim_start()
*                 was called.
*
* Returned Value:
*   True: The function is complete.
*
****************************************************************************/

static void beken_tim_callback_wrapper(timer_id_t timer_id)
{
  struct beken_arg *arg_input_temp = &g_arg_input[timer_id];
  struct beken_lowerhalf_s *priv =
    (struct beken_lowerhalf_s *)arg_input_temp->arg;
  uint32_t next_interval = 0;

  priv->flag |= TCFLAGS_HANDLER;

  if (priv->callback != NULL &&
      priv->callback(&next_interval, priv->arg))
    {
      if (next_interval > 0)
        {
          /* Set a value to the alarm */
          priv->timeout = next_interval;

          beken_tim_stop((struct timer_lowerhalf_s *)priv);
          beken_tim_start((struct timer_lowerhalf_s *)priv);
        }
    }
}

/****************************************************************************
 * Name: beken_tim_start
 *
 * Description:
 *   Start the timer, resetting the time to the current timeout,
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of the
 *           "lower-half" driver state structure.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int beken_tim_start(struct timer_lowerhalf_s *lower)
{
  struct beken_lowerhalf_s *priv = (struct beken_lowerhalf_s *)lower;
  int rtn_value = -EBUSY;

  if (!priv->started)
    {
      /*To avoid the oneshot_lowerhalf use the same timer id.*/
      bk_timer_stop(priv->timer_id);
      rtn_value = bk_timer_delay_with_callback(priv->timer_id, priv->timeout , beken_tim_callback_wrapper);

      if(rtn_value == OK)
        {
          priv->started = true;
          priv->flag    = TCFLAGS_ACTIVE;
          return OK;
        }
    }

  /* Return EBUSY to indicate that the timer was already running */
  return rtn_value;
}

/****************************************************************************
 * Name: beken_tim_stop
 *
 * Description:
 *   Stop the timer
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of the
 *           "lower-half" driver state structure.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int beken_tim_stop(struct timer_lowerhalf_s *lower)
{
  struct beken_lowerhalf_s *priv = (struct beken_lowerhalf_s *)lower;
  int rtn_value = -ENODEV;

  if (priv->started)
    {
      rtn_value = bk_timer_stop(priv->timer_id);

      if(rtn_value == OK)
        {
          priv->started = false;
          priv->flag    = 0;
          return OK;
        }
    }

  /* Return ENODEV to indicate that the timer was not running */
  return rtn_value;
}

/****************************************************************************
 * Name: beken_tim_getstatus
 *
 * Description:
 *   Get timer status.
 *
 * Input Parameters:
 *   lower  - A pointer the publicly visible representation of the "lower-
 *            half" driver state structure.
 *   status - The location to return the status information.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/
static int beken_tim_getstatus(struct timer_lowerhalf_s *lower,
                                       struct timer_status_s *status)
{
  struct beken_lowerhalf_s *priv = (struct beken_lowerhalf_s *)lower;

  status->flags    = priv->flag;
  status->timeleft = bk_timer_get_time(priv->timer_id, 1, 0, TIMER_UNIT_US);
  status->timeout  = priv->timeout;

  return OK;
}

/****************************************************************************
 * Name: beken_tim_settimeout
 *
 * Description:
 *   Set a new timeout value (and reset the timer)
 *
 * Input Parameters:
 *   lower   - A pointer the publicly visible representation of the
 *             "lower-half" driver state structure.
 *   timeout - The new timeout value in us .
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int beken_tim_settimeout(struct timer_lowerhalf_s *lower,
                                         uint32_t timeout)
{
  struct beken_lowerhalf_s *priv = (struct beken_lowerhalf_s *)lower;

  if (priv->started)
    {
      return -EPERM;
    }

  priv->timeout = timeout;

  return OK;
}

/****************************************************************************
 * Name: beken_tim_setcallback
 *
 * Description:
 *   Call this user provided timeout callback.
 *
 * Input Parameters:
 *   lower      - A pointer the publicly visible representation of the
 *                "lower-half" driver state structure.
 *   callback   - The new timer expiration function pointer.  If this
 *                function pointer is NULL, then the reset-on-expiration
 *                behavior is restored,
 *   arg        - Argument that will be provided in the callback
 *
 * Returned Value:
 *   The previous timer expiration function pointer or NULL is there was
 *   no previous function pointer.
 *
 ****************************************************************************/

static void beken_tim_setcallback(struct timer_lowerhalf_s *lower,
                                           tccb_t callback, void *arg)
{
  struct beken_lowerhalf_s *priv = (struct beken_lowerhalf_s *)lower;

  irqstate_t flags = enter_critical_section();

  /* Save the new callback */

  priv->callback = callback;
  priv->arg      = arg;

  g_arg_input[priv->timer_id].arg      = (void *)priv;
  g_arg_input[priv->timer_id].timer_id = priv->timer_id;

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: beken_tim_maxtimeout
 *
 * Description:
 *   Get the maximum timeout value
 *
 * Input Parameters:
 *   lower       - A pointer the publicly visible representation of
 *                 the "lower-half" driver state structure.
 *   maxtimeout  - A pointer to the variable that will store the max timeout.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int beken_tim_maxtimeout(struct timer_lowerhalf_s *lower,
                                         uint32_t *max_timeout)
{
  DEBUGASSERT(max_timeout);

  *max_timeout = (uint32_t)(BEKEN_TIMER_MAX_CNT * USEC_PER_SEC /
                          (TIMER_CLOCK_FREQ_XTAL * 1000));

  return OK;
}

/****************************************************************************
* Public Functions
****************************************************************************/

/****************************************************************************
* Name: beken_timer_get_callback_arg
*
* Description:
*   Get the args of timer time out callback function
*
* Input Parameters:
*   NONE
*
* Returned Value:
*   NONE
*
****************************************************************************/

struct beken_arg * beken_timer_get_callback_arg(void)
{
  return g_arg_input;
}

/****************************************************************************
 * Name: beken_timer_initialize
 *
 * Description:
 *   Bind the configuration timer to a timer lower half instance and
 *   register the timer drivers at 'devpath'
 *
 * Input Parameters:
 *   devpath - The full path to the timer device.  This should be of the
 *             form /dev/timer0
 *   timer   - the timer's number.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/

int beken_timer_initialize(const char *devpath, int timer)
{
  struct beken_lowerhalf_s *lower;

  switch (timer)
    {
      case TIMER_ID0:
        lower = &g_tim0_lowerhalf;
        break;
      case TIMER_ID1:
        lower = &g_tim1_lowerhalf;
        break;
      case TIMER_ID2:
        lower = &g_tim2_lowerhalf;
        break;
      case TIMER_ID3:
        lower = &g_tim3_lowerhalf;
        break;
      case TIMER_ID4:
        lower = &g_tim4_lowerhalf;
        break;
      case TIMER_ID5:
        lower = &g_tim5_lowerhalf;
        break;
      default:
        return -ENODEV;
    }

  /* Initialize the elements of lower half state structure */

  lower->started  = false;
  lower->callback = NULL;

  /* Register the timer driver as /dev/timerX.  The returned value from
   * timer_register is a handle that could be used with timer_unregister().
   * REVISIT: The returned handle is discard here.
   */

  void *drvr = timer_register(devpath,
                              (struct timer_lowerhalf_s *)lower);
  if (drvr == NULL)
    {
      /* The actual cause of the failure may have been a failure to allocate
       * perhaps a failure to register the timer driver (such as if the
       * 'depath' were not unique).  We know here but we return EEXIST to
       * indicate the failure (implying the non-unique devpath).
       */
      return -EEXIST;
    }

  return OK;
}

