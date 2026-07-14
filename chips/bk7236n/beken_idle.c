/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_idle.c
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

#include <arch/board/board.h>
#include <nuttx/config.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/power/pm.h>

#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <nuttx/clock.h>
#include <stdarg.h>
#include <stdio.h>

#include "chip.h"
#include "arm_internal.h"
#include "driver/gpio.h"
#include "modules/pm.h"
#include "driver/aon_rtc.h"
#include "beken_uart.h"

#ifdef CONFIG_PM

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Variables
 ****************************************************************************/

static uint32_t sleep_mode = PM_MODE_DEFAULT;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_gpio_wakeup_int_wrppper
 *
 * Description:
 *   The isr callback function of GPIO wakeup.
 *
 * Input Parameters:
 *   args         - The args of register callback function.
 *   gpio_id      - The GPIO id of the beken device.
 *
 * Returned Value:
 *   OK      : The function execute successfuly.
 *   ERROR   : Can`t find the callback function corresponding the gpio_id.
 *
 ****************************************************************************/

static void beken_gpio_wakeup_int_wrppper(gpio_id_t gpio_id)
{
    (void)gpio_id;

    if (sleep_mode == PM_MODE_LOW_VOLTAGE)
      {
        bk_pm_sleep_mode_set(PM_MODE_DEFAULT);
        bk_pm_module_vote_sleep_ctrl(PM_SLEEP_MODULE_NAME_APP,0x0,0x0);
      }
    else if (sleep_mode == PM_MODE_DEEP_SLEEP)
      {
        bk_pm_sleep_mode_set(PM_MODE_DEFAULT);
      }
    else
      {
        bk_pm_sleep_mode_set(PM_MODE_DEFAULT);
        bk_pm_module_vote_sleep_ctrl(0,0x0,0x0);
        bk_pm_module_vote_sleep_ctrl(0,0x0,0x0);
        bk_pm_module_vote_sleep_ctrl(PM_SLEEP_MODULE_NAME_APP,0x0,0x0);
      }
}

/****************************************************************************
 * Name: up_idlepm
 *
 * Description:
 *   Perform IDLE state power management.
 *
 * Input Parameters:
 *   NULL.
 *
 * Returned Value:
 *   NULL.
 *
 ****************************************************************************/

static void up_idlepm(void)
{
  static enum pm_state_e oldstate = PM_NORMAL;
  enum pm_state_e newstate;
  irqstate_t flags;
  int ret;

  /* Decide, which power saving level can be obtained */

  newstate = pm_checkstate(PM_IDLE_DOMAIN);

  /* Check for state changes */

  if (newstate != oldstate)
    {
      flags = enter_critical_section();

      /* Perform board-specific, state-dependent logic here */

      _info("newstate= %d oldstate=%d\n", newstate, oldstate);

      /* Then force the global state change */

      ret = pm_changestate(PM_IDLE_DOMAIN, newstate);
      if (ret < 0)
        {
          /* The new state change failed, revert to the preceding state */

          pm_changestate(PM_IDLE_DOMAIN, oldstate);
        }
      else
        {
          /* Save the new state */

          oldstate = newstate;
        }

      /* MCU-specific power management logic */

      leave_critical_section(flags);

      switch (newstate)
        {
        case PM_NORMAL:
          sleep_mode = PM_MODE_DEFAULT;
          break;

        case PM_IDLE:
          sleep_mode = PM_MODE_NORMAL_SLEEP;
          break;

        case PM_STANDBY:
          _info("Device will enter the standby mode\n");
          bk_gpio_register_isr(GPIO_12, beken_gpio_wakeup_int_wrppper);
          bk_gpio_register_wakeup_source(GPIO_12,GPIO_INT_TYPE_FALLING_EDGE);
          bk_pm_wakeup_source_set(PM_WAKEUP_SOURCE_INT_GPIO, NULL);
          sleep_mode = PM_MODE_NORMAL_SLEEP;

          bk_pm_sleep_mode_set(PM_MODE_NORMAL_SLEEP);
          bk_pm_suppress_ticks_and_sleep(0);
          break;

        case PM_SLEEP:
          _info("Device will enter the sleep mode\n");
          bk_gpio_register_isr(GPIO_12, beken_gpio_wakeup_int_wrppper);
          bk_gpio_register_wakeup_source(GPIO_18,GPIO_INT_TYPE_FALLING_EDGE);
          bk_pm_wakeup_source_set(PM_WAKEUP_SOURCE_INT_GPIO, NULL);
          sleep_mode = PM_MODE_LOW_VOLTAGE;

          bk_pm_sleep_mode_set(PM_MODE_LOW_VOLTAGE);
          bk_pm_module_vote_sleep_ctrl(PM_SLEEP_MODULE_NAME_APP,0x1,0x0);
          bk_pm_suppress_ticks_and_sleep(0);
          extern void up_timer_initialize(void);
          up_timer_initialize();
#if defined(CONFIG_RTC) && !defined(CONFIG_SCHED_TICKLESS) && \
    !defined(CONFIG_CLOCK_TIMEKEEPING) && !defined(CONFIG_ALARM_ARCH) && \
    !defined(CONFIG_TIMER_ARCH)
          clock_resynchronize(NULL);
#endif
          extern void wd_timer(void);
          wd_timer();
          beken_flush_uart_fifo();
          break;

        default:
          break;
        }
    }
}
#else
#  define up_idlepm()
#endif


/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_idle
 *
 * Description:
 *   up_idle() is the logic that will be executed when there is no other
 *   ready-to-run task.  This is processor idle time and will continue until
 *   some interrupt occurs to cause a context switch from the idle task.
 *
 *   Processing in this state may be processor-specific. e.g., this is where
 *   power management operations might be performed.
 *
 ****************************************************************************/

void up_idle(void)
{
#if defined(CONFIG_SUPPRESS_INTERRUPTS) || defined(CONFIG_SUPPRESS_TIMER_INTS)
  /* If the system is idle and there are no timer interrupts, then process
   * "fake" timer interrupts. Hopefully, something will wake up.
   */

  nxsched_process_timer();
#else
  /* Perform IDLE mode power management */

  if (sleep_mode == PM_MODE_NORMAL_SLEEP)
    {
      bk_pm_sleep_mode_set(PM_MODE_NORMAL_SLEEP);
      bk_pm_suppress_ticks_and_sleep(0);
    }

  up_idlepm();

#endif
}
