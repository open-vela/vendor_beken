/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_wdt_lowerhalf.c
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
#include <nuttx/arch.h>

#include <inttypes.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/timers/watchdog.h>
#include <arch/board/board.h>

#include "driver/wdt.h"
#include "beken_wdt_lowerhalf.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define WDT_INT_WDT_PERIOD_MS (1000)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This structure provides the private representation of the "lower-half"
* driver state structure.  This structure must be cast-compatible with the
* well-known watchdog_lowerhalf_s structure.
*/

struct beken_wdt_lowerhalf_s
{
  const struct watchdog_ops_s  *ops; /* Lower half operations */
  uint32_t lastreset;                /* The last reset time */
  uint32_t timeout;
  uint8_t started;
};

/****************************************************************************
* Private Function Prototypes
****************************************************************************/

/* "Lower half" driver methods **********************************************/

static int beken_start(struct watchdog_lowerhalf_s *lower);
static int beken_stop(struct watchdog_lowerhalf_s *lower);
static int beken_keepalive(struct watchdog_lowerhalf_s *lower);
static int beken_getstatus(struct watchdog_lowerhalf_s *lower,
                                 struct watchdog_status_s *status);
static int beken_settimeout(struct watchdog_lowerhalf_s *lower,
                                   uint32_t timeout);

/****************************************************************************
* Private Data
****************************************************************************/

/* "Lower half" driver methods */

static const struct watchdog_ops_s g_wdtops =
{
  .start      = beken_start,
  .stop       = beken_stop,
  .keepalive  = beken_keepalive,
  .getstatus  = beken_getstatus,
  .settimeout = beken_settimeout,
  .capture    = NULL,
  .ioctl      = NULL,
};

/* "Lower half" driver state */

static struct beken_wdt_lowerhalf_s g_wdt_dev =
{
  .ops       = &g_wdtops,
  .lastreset = 0,
  .started   = false,
  .timeout   = WDT_INT_WDT_PERIOD_MS,
};


/****************************************************************************
* Private Functions
****************************************************************************/

/****************************************************************************
* Name: bk7236_start
*
* Description:
*   Start the watchdog timer, resetting the time to the current timeout,
*
* Input Parameters:
*   lower - A pointer the publicly visible representation of the
*           "lower-half" driver state structure.
*
* Returned Values:
*   Zero on success; a negated errno value on failure.
*
****************************************************************************/

static int beken_start(struct watchdog_lowerhalf_s *lower)
{
  struct beken_wdt_lowerhalf_s *priv = (struct beken_wdt_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  /* Have we already been started? */

  if (!priv->started && bk_wdt_is_driver_inited())
    {
      bk_wdt_start( priv->timeout);
      priv->lastreset = clock_systime_ticks();
      priv->started = 1;
    }

  return OK;
}

/****************************************************************************
* Name: beken_stop
*
* Description:
*   Stop the watchdog timer
*
* Input Parameters:
*   lower - A pointer the publicly visible representation of the
*           "lower-half" driver state structure.
*
* Returned Values:
*   Zero on success; a negated errno value on failure.
*
****************************************************************************/

static int beken_stop(struct watchdog_lowerhalf_s *lower)
{
  struct beken_wdt_lowerhalf_s *priv = (struct beken_wdt_lowerhalf_s *)lower;

  if (priv->started && bk_wdt_is_driver_inited())
    {
      bk_wdt_stop();
      priv->started = 0;
    }

  return OK;
}

/****************************************************************************
* Name: beken_keepalive
*
* Description:
*   Reset the watchdog timer to the current timeout value, prevent any
*   imminent watchdog timeouts.  This is sometimes referred as "pinging"
*   the watchdog timer or "petting the dog".
*
* Input Parameters:
*   lower - A pointer the publicly visible representation of the
*           "lower-half" driver state structure.
*
* Returned Values:
*   Zero on success; a negated errno value on failure.
*
****************************************************************************/

static int beken_keepalive(struct watchdog_lowerhalf_s *lower)
{
  struct beken_wdt_lowerhalf_s *priv = (struct beken_wdt_lowerhalf_s *)lower;

  if (priv->started && bk_wdt_is_driver_inited())
    {
      /* Reload the WDT timer */
      bk_wdt_feed();
      priv->lastreset = clock_systime_ticks();
    }

  return OK;
}

/****************************************************************************
* Name: beken_getstatus
*
* Description:
*   Get the current watchdog timer status
*
* Input Parameters:
*   lower  - A pointer the publicly visible representation of
*            the "lower-half" driver state structure.
*   status - The location to return the watchdog status information.
*
* Returned Values:
*   Zero on success; a negated errno value on failure.
*
****************************************************************************/

static int beken_getstatus(struct watchdog_lowerhalf_s *lower,
                                 struct watchdog_status_s *status)
{
  struct beken_wdt_lowerhalf_s *priv = (struct beken_wdt_lowerhalf_s *)lower;

  uint32_t ticks;
  uint32_t elapsed;

  DEBUGASSERT(priv);

  /* Return the status bit */

  status->flags = WDFLAGS_RESET;
  if (priv->started && bk_wdt_is_driver_inited())
    {
      status->flags |= WDFLAGS_ACTIVE;
    }

  /* Return the actual timeout in milliseconds */

  status->timeout = priv->timeout;

  /* Get the elapsed time since the last ping */

  ticks   = clock_systime_ticks() - priv->lastreset;
  elapsed = (int32_t)TICK2MSEC(ticks);

  if (elapsed > status->timeout)
    {
      elapsed = status->timeout;
    }

  /* Return the approximate time until the watchdog timer expiration */

  status->timeleft = status->timeout - elapsed;

  return OK;
}

/****************************************************************************
* Name: beken_settimeout
*
* Description:
*   Set a new timeout value (and reset the watchdog timer)
*
* Input Parameters:
*   lower   - A pointer the publicly visible representation of
*             the "lower-half" driver state structure.
*   timeout - The new timeout value in milliseconds.
*
* Returned Values:
*   Zero on success; a negated errno value on failure.
*
****************************************************************************/

static int beken_settimeout(struct watchdog_lowerhalf_s *lower, uint32_t timeout)
{
  struct beken_wdt_lowerhalf_s *priv = (struct beken_wdt_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  /* Can this timeout be represented? */

  if (timeout < 1)
    {
      wderr("ERROR: Cannot represent timeout=%" PRId32 "\n",timeout);
      return -ERANGE;
    }

  priv->timeout = timeout;

  return OK;
}

/****************************************************************************
* Public Functions
****************************************************************************/

/****************************************************************************
* Name: beken_wdt_initialize
*
* Description:
*   Initialize the WDT watchdog time.  The watchdog timer is initialized and
*   registers as 'devpath.  The initial state of the watchdog time is
*   disabled.
*
* Input Parameters:
*   devpath    - The full path to the watchdog.  This should be of the form
*                /dev/watchdog0
* Returned Values:
*   Zero (OK) is returned on success; a negated errno value is returned on
*   any failure.
*
****************************************************************************/

int beken_wdt_initialize(const char *devpath)
{
  struct beken_wdt_lowerhalf_s *priv = &g_wdt_dev;
  void *handle;

  /* Register the watchdog driver as /dev/watchdog0 */

  handle = watchdog_register(devpath,(struct watchdog_lowerhalf_s *)priv);

  return (handle != NULL) ? OK : -ENODEV;
}
