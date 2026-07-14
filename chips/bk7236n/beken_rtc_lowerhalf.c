/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_rtc_lowerhalf.c
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

#include <complex.h>
#include <nuttx/config.h>
#include <nuttx/spinlock.h>

#include <sys/types.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/timers/rtc.h>
#include <nuttx/timers/arch_rtc.h>
#include <time.h>

#include "driver/aon_rtc.h"
#include "beken_rtc_lowerhalf.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

#ifdef CONFIG_RTC_ALARM
struct beken_cbinfo_s
{
  volatile rtc_alarm_callback_t cb;  /* Callback when the alarm expires */
  volatile void *priv;               /* Private argurment to accompany callback */
};
#endif

/* This is the private type for the RTC state.  It must be cast compatible
 * with struct rtc_lowerhalf_s.
 */

struct beken_lowerhalf_s
{
  /* This is the contained reference to the read-only, lower-half
   * operations vtable (which may lie in FLASH or ROM)
   */

  const struct rtc_ops_s *ops;

  mutex_t devlock;      /* Threads can only exclusively access the RTC */
  struct rtc_time rtc_base;
  struct rtc_time rtc_alarm;
#ifdef CONFIG_RTC_ALARM
  /* Alarm callback information */

  struct beken_cbinfo_s cbinfo[AON_RTC_MAX_ALARM_CNT];
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Prototypes for static methods in struct rtc_ops_s */

static int beken_rdtime(struct rtc_lowerhalf_s *lower,
                              struct rtc_time *rtctime);
static int beken_settime(struct rtc_lowerhalf_s *lower,
                              const struct rtc_time *rtctime);
static bool beken_havesettime(struct rtc_lowerhalf_s *lower);

#ifdef CONFIG_RTC_ALARM
static int beken_setalarm(struct rtc_lowerhalf_s *lower,
                                const struct lower_setalarm_s *alarminfo);
static int beken_setrelative(struct rtc_lowerhalf_s *lower,
                                    const struct lower_setrelative_s *alarminfo);
static int beken_cancelalarm(struct rtc_lowerhalf_s *lower,
                                    int alarmid);
static int beken_rdalarm(struct rtc_lowerhalf_s *lower,
                              struct lower_rdalarm_s *alarminfo);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* BK7236 RTC driver operations */

static const struct rtc_ops_s g_rtc_ops =
{
  .rdtime      = beken_rdtime,
  .settime     = beken_settime,
  .havesettime = beken_havesettime,
#ifdef CONFIG_RTC_ALARM
  .setalarm    = beken_setalarm,
  .setrelative = beken_setrelative,
  .cancelalarm = beken_cancelalarm,
  .rdalarm     = beken_rdalarm,
#endif
};

/* BK7236 RTC device state */

static struct beken_lowerhalf_s g_rtc_lowerhalf =
{
  .ops              = &g_rtc_ops,
  .devlock          = NXMUTEX_INITIALIZER,
  .rtc_base.tm_year = 70,
  .rtc_base.tm_mday = 1,
};

static const char * g_alarm_name[AON_RTC_MAX_ALARM_CNT] =
{
  "alarm_0",
  "alarm_1",
  "alarm_2",
  "alarm_3",
  "alarm_4",
  "alarm_5",
  "alarm_6",
  "alarm_7"
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
* Name: beken_get_time_stamps_ms
*
* Description:
*   Get the time stamps of RTC
*
* Input Parameters:
*   None
*
* Returned Value:
*  uint64_t Value; The time stamps of RTC in ms.
*
****************************************************************************/

static uint64_t beken_get_time_stamps_ms(void)
{

  uint64_t time_diff = (uint64_t) (bk_aon_rtc_get_current_tick(AON_RTC_ID_1) / (uint32_t)bk_rtc_get_ms_tick_count());

  return time_diff;
}

/****************************************************************************
* Name: beken_rdtime
*
* Description:
*   Implements the rdtime() method of the RTC driver interface
*
* Input Parameters:
*   lower   - A reference to RTC lower half driver state structure
*   rcttime - The location in which to return the current RTC time.
*
* Returned Value:
*   Zero (OK) is returned on success; a negated errno value is returned
*   on any failure.
*
****************************************************************************/

 static int beken_rdtime(struct rtc_lowerhalf_s *lower,
                                struct rtc_time *rtctime)
{
  DEBUGASSERT(lower != NULL && rtctime != NULL);

  struct beken_lowerhalf_s *priv;
  uint64_t time_stamp_s;
  struct rtc_time tim;
  priv = (struct beken_lowerhalf_s *)lower;

  time_stamp_s = (uint64_t)(beken_get_time_stamps_ms() / 1000);

  tim = priv->rtc_base;

  time_stamp_s += (uint64_t)mktime((struct tm *)&tim);//read the time relative to 1900
  gmtime_r((const time_t *)&time_stamp_s, (struct tm *)rtctime);

  return OK;
}

/****************************************************************************
* Name: beken_settime
*
* Description:
*   Implements the settime() method of the RTC driver interface
*
* Input Parameters:
*   lower   - A reference to RTC lower half driver state structure
*   rcttime - The new time to set
*
* Returned Value:
*   Zero (OK) is returned on success; a negated errno value is returned
*   on any failure.
*
****************************************************************************/

static int beken_settime(struct rtc_lowerhalf_s *lower,
                                const struct rtc_time *rtctime)
{

  struct beken_lowerhalf_s *priv = (struct beken_lowerhalf_s *)lower;

  //TODO
  //bk_rtc_reset_counter(AON_RTC_ID_1);

  priv->rtc_base = *rtctime;

  return OK;
}

/****************************************************************************
* Name: beken_havesettime
*
* Description:
*   Implements the havesettime() method of the RTC driver interface
*
* Input Parameters:
*   lower   - A reference to RTC lower half driver state structure
*
* Returned Value:
*   Returns true if RTC date-time have been previously set.
*
****************************************************************************/

static bool beken_havesettime(struct rtc_lowerhalf_s *lower)
{
  DEBUGASSERT(lower != NULL);

  struct beken_lowerhalf_s *priv = (struct beken_lowerhalf_s *)lower;

  return (priv->rtc_base.tm_year != 0);
}

#ifdef CONFIG_RTC_ALARM

/****************************************************************************
* Name: beken_setalarm
*
* Description:
*   Set a new alarm.  This function implements the setalarm() method of the
*   RTC driver interface
*
* Input Parameters:
*   lower - A reference to RTC lower half driver state structure
*   alarminfo - Provided information needed to set the alarm
*
* Returned Value:
*   Zero (OK) is returned on success; a negated errno value is returned
*   on any failure.
*
****************************************************************************/

static int beken_setalarm(struct rtc_lowerhalf_s *lower,
                                 const struct lower_setalarm_s *alarminfo)
{
  int rtn = -EIO;
  alarm_info_t alarm_info_p = {0};
  struct rtc_time rtctime = {0};
  uint64_t alarm_tick_seconds = 0;
  struct beken_lowerhalf_s *priv;
  DEBUGASSERT(lower != NULL && alarminfo != NULL);
  DEBUGASSERT((0 <= alarminfo->id) && (alarminfo->id < AON_RTC_MAX_ALARM_CNT));

  priv = (struct beken_lowerhalf_s *)lower;

  rtn = beken_rdtime(lower,&rtctime);

  if (rtn != OK)
    {
      return rtn;
    }
  alarm_tick_seconds = (uint64_t)(mktime((struct tm *)&alarminfo->time) - mktime((struct tm *)&rtctime));
  memcpy(alarm_info_p.name,g_alarm_name[alarminfo->id],sizeof(alarm_info_p.name));

  alarm_info_p.period_tick =  alarm_tick_seconds * bk_rtc_get_clock_freq();
  alarm_info_p.period_cnt  = 1;
  alarm_info_p.callback    = alarminfo->cb;
  alarm_info_p.param_p     = alarminfo->priv;

  bk_err_t  bk_rtn = bk_alarm_register(AON_RTC_ID_1, &alarm_info_p);

  if (bk_rtn == BK_ERR_NO_MEM)
    {
      return -ENOMEM;
    }
  else if (bk_rtn)
    {
      return -EIO;
    }
  priv->cbinfo[alarminfo->id].cb   =  alarminfo->cb;
  priv->cbinfo[alarminfo->id].priv = alarminfo->priv;
  priv->rtc_alarm                  = alarminfo->time;

  return OK;
}

/****************************************************************************
* Name: beken_setrelative
*
* Description:
*   Set a new alarm relative to the current time.  This function implements
*   the setrelative() method of the RTC driver interface
*
* Input Parameters:
*   lower - A reference to RTC lower half driver state structure
*   alarminfo - Provided information needed to set the alarm
*
* Returned Value:
*   Zero (OK) is returned on success; a negated errno value is returned
*   on any failure.
*
****************************************************************************/

static int beken_setrelative(struct rtc_lowerhalf_s *lower,
                                      const struct lower_setrelative_s *alarminfo)
{
  int rtn = -EIO;
  alarm_info_t alarm_info_p = {0};
  struct rtc_time rtctime = {0};
  int64_t alarm_tick_seconds = 0;
  struct beken_lowerhalf_s *priv;

  DEBUGASSERT(lower != NULL && alarminfo != NULL);
  DEBUGASSERT((0 <= alarminfo->id) && (alarminfo->id < AON_RTC_MAX_ALARM_CNT));

  priv = (struct beken_lowerhalf_s *)lower;

  rtn = beken_rdtime(lower,&rtctime);

  if (rtn != OK)
    {
      return rtn;
    }

  alarm_tick_seconds = alarminfo->reltime + (uint64_t)(mktime((struct tm *)&rtctime));
  memcpy(alarm_info_p.name,g_alarm_name[alarminfo->id],sizeof(alarm_info_p.name));

  alarm_info_p.period_tick =  alarminfo->reltime * bk_rtc_get_clock_freq();
  alarm_info_p.period_cnt  = 1;
  alarm_info_p.callback    = alarminfo->cb;
  alarm_info_p.param_p     = alarminfo->priv;

  bk_err_t  bk_rtn = bk_alarm_register(AON_RTC_ID_1, &alarm_info_p);

  if (bk_rtn == BK_ERR_NO_MEM)
    {
      return -ENOMEM;
    }
  else if (bk_rtn)
    {
      return -EIO;
    }

  priv->cbinfo[alarminfo->id].cb   =  alarminfo->cb;
  priv->cbinfo[alarminfo->id].priv = alarminfo->priv;

  gmtime_r(&alarm_tick_seconds, (struct tm *)&priv->rtc_alarm);

  return OK;
}

/****************************************************************************
* Name: beken_cancelalarm
*
* Description:
*   Cancel the current alarm.  This function implements the cancelalarm()
*   method of the RTC driver interface
*
* Input Parameters:
*   lower - A reference to RTC lower half driver state structure
*   alarminfo - Provided information needed to set the alarm
*
* Returned Value:
*   Zero (OK) is returned on success; a negated errno value is returned
*   on any failure.
*
****************************************************************************/

static int beken_cancelalarm(struct rtc_lowerhalf_s *lower,int alarmid)
{
  struct beken_lowerhalf_s *priv;

  DEBUGASSERT(lower != NULL);
  DEBUGASSERT((0 <= alarmid) && (alarmid< AON_RTC_MAX_ALARM_CNT));
  priv = (struct beken_lowerhalf_s *)lower;

  bk_err_t  bk_rtn = bk_alarm_unregister(AON_RTC_ID_1, (uint8_t *)g_alarm_name[alarmid]);

  if (bk_rtn)
    {
      return -EIO;
    }

  priv->cbinfo[alarmid].cb   =  NULL;
  priv->cbinfo[alarmid].priv = NULL;
  memset(&priv->rtc_alarm, 0, sizeof(priv->rtc_alarm));

  return OK;
}

/****************************************************************************
* Name: beken_rdalarm
*
* Description:
*   Query the RTC alarm.
*
* Input Parameters:
*   lower - A reference to RTC lower half driver state structure
*   alarminfo - Provided information needed to query the alarm
*
* Returned Value:
*   Zero (OK) is returned on success; a negated errno value is returned
*   on any failure.
*
****************************************************************************/

static int beken_rdalarm(struct rtc_lowerhalf_s *lower,struct lower_rdalarm_s *alarminfo)
{
  struct beken_lowerhalf_s *priv;

  DEBUGASSERT(lower != NULL && alarminfo != NULL);
  DEBUGASSERT((0 <= alarminfo->id) && (alarminfo->id < AON_RTC_MAX_ALARM_CNT));

  priv = (struct beken_lowerhalf_s *)lower;

  if(alarminfo->id == 0)
    {
      *alarminfo->time = priv->rtc_alarm;
    }

  return OK;
}

#endif
/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_rtc_initialize
 *
 * Description:
 *   Initialize the hardware RTC per the selected configuration.  This
 *   function is called once during the OS initialization sequence
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure
 *
 ****************************************************************************/

int up_rtc_initialize(void)
{
  int rtn = -EIO;
  struct rtc_lowerhalf_s *lower = NULL;

  lower = beken_rtc_lowerhalf();

  if (lower == NULL)
    {
       return rtn;
    }
  else
    {
       /* Bind the lower half driver */
       up_rtc_set_lowerhalf(lower, false);
       /* register the combined RTC driver as /dev/rtc0*/
       rtn = rtc_initialize(0, lower);
    }

 return rtn;
}

/****************************************************************************
 * Name: beken_rtc_lowerhalf
 *
 * Description:
 *   Instantiate the RTC lower half driver for the BK7236.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   On success, a non-NULL RTC lower interface is returned.  NULL is
 *   returned on any failure.
 *
 ****************************************************************************/

struct rtc_lowerhalf_s *beken_rtc_lowerhalf(void)
{
  return (struct rtc_lowerhalf_s *)&g_rtc_lowerhalf;
}

/****************************************************************************
* Name: beken_get_alarm_id
*
* Description:
*   Get the corresponding alarm id.
*
* Input Parameters:
*   None
*
* Returned Value:
*   Return the corresponding alarm id of alarm name
*
****************************************************************************/

uint32_t beken_get_alarm_id(uint8_t *name)
{
  uint32_t alarm_num = 0;

  for( alarm_num = 0; alarm_num < AON_RTC_MAX_ALARM_CNT;alarm_num++)
    {
      if(strcmp(g_alarm_name[alarm_num],(const char *)name) == 0)
        {
          return alarm_num;
        }
    }

  return alarm_num;
}

