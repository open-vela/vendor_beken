/****************************************************************************
 * vendor/beken/boards/bk7236n-evb/src/beken_ap.c
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
// specify chip arch internal header
// eg: arm_internal.h riscv_internal.h
#include "soc/bk7236n/reg_base.h"
#include "sys/boardctl.h"
#include "arm_internal.h"

#include <nuttx/timers/oneshot.h>
#include "debug.h"

#include "driver/wdt.h"
#include "driver/uart.h"
#include "driver/aon_wdt.h"
#ifdef CONFIG_WATCHDOG
#include "beken_wdt_lowerhalf.h"
#endif
#include "components/system.h"
#include "driver/aon_rtc.h"
#include "modules/pm.h"
#include "driver/pwr_clk.h"
#include "modules/wifi.h"
#include "driver/rosc_32k.h"
#include "components/bluetooth/bk_dm_bluetooth.h"
#include "components/sensor.h"
#include "driver/flash.h"
#include "driver/dma.h"
#include "driver/adc.h"
#include "driver/ckmn.h"
#include "driver/efuse.h"
#include "driver/trng.h"
#include "soc/bk7236n/timer_cap.h"
#include "beken_tim_lowerhalf.h"
#include "driver/timer.h"
#include "beken_oneshot_lowerhalf.h"
#include "beken_gpio.h"
#include "driver/pwm.h"
#include "beken_pwm_lowerhalf.h"
#include "driver/i2c.h"
#include "beken_board_i2cdev.h"
#include "driver/adc.h"
#include "beken_adc_lowerhalf.h"
#include "driver/spi.h"
#include "beken_board_spidev.h"
#include "beken_board_spislavedev.h"
#include "driver/trng.h"
#include <nuttx/config.h>
#include "beken_flash.h"
#include <nuttx/mtd/mtd.h>
#include <nuttx/mtd/configdata.h>
#include "components/event.h"
#include "components/netif.h"
#include "driver/otp.h"
#include "beken_wlan.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if defined(CONFIG_BK7236N_MTD_PARTITION) && defined (CONFIG_KVDB_NVS) && defined (CONFIG_CM_KVDB_TEST)
#define RAM_MTD_SIZE   (0x4000)
#endif

/****************************************************************************
 * External Symbols
 ****************************************************************************/

extern uint8_t __itcm_start__[];
extern uint8_t __itcm_end__[];
extern uint8_t __itcm_text[];
extern uint8_t __dtcm_content[];
extern uint8_t __dtcm_start__[];
extern uint8_t __dtcm_end__[];

/****************************************************************************
 * Private Data
 ****************************************************************************/

#if defined(CONFIG_BK7236N_MTD_PARTITION) && defined (CONFIG_KVDB_NVS) && defined (CONFIG_CM_KVDB_TEST)
static uint8_t g_rammtd[RAM_MTD_SIZE];
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

extern void sys_drv_init(void);
extern void vnd_cal_overlay(void);
extern void g_dd_init(void);
extern UINT32 drv_model_init(void);
extern int bandgap_init(void);

/****************************************************************************
 * Name: bk7236_evb_bringup
 *
 * Description:
 *   Perform architecture-specific initialization
 *
 *   CONFIG_BOARD_LATE_INITIALIZE=y
 *     Called from board_late_initialize().
 *
 *   CONFIG_BOARD_LATE_INITIALIZE=n && CONFIG_BOARDCTL=y
 *     Called from the NSH library
 *
 ****************************************************************************/

int bk7236_evb_bringup(void)
{
#ifdef CONFIG_WATCHDOG
  beken_wdt_initialize("/dev/watchdog0");
#endif
  beken_timer_initialize("/dev/timer0", TIMER_ID0);

#ifdef CONFIG_SPI_DRIVER
  board_spidev_initialize(SPI_ID_0);
#endif

#ifdef CONFIG_SPI_SLAVE_DRIVER
  board_spislavedev_initialize(SPI_ID_1);
#endif

#ifdef CONFIG_ONESHOT
  int ret = OK;
  struct oneshot_lowerhalf_s *os_lower = NULL;

  os_lower = oneshot_initialize(TIMER_ID1, 0);
  if (os_lower != NULL)
    {
      ret = oneshot_register("/dev/oneshot", os_lower);
      if (ret < 0)
        {
          sinfo("ERROR: Failed to register oneshot at /dev/oneshot: %d\n", ret);
        }
    }
  else
    {
      return -EBUSY;
    }
#endif

#ifdef CONFIG_VND_CAL
  vnd_cal_overlay();
#endif
  bk_pm_module_vote_cpu_freq(PM_DEV_ID_DEFAULT, PM_CPU_FRQ_120M);
#ifdef CONFIG_BK7236N_WIFI
  bk_event_init();
  wifi_init_config_t wifi_config = WIFI_DEFAULT_INIT_CONFIG();
  bk_wifi_init(&wifi_config);
#ifdef CONFIG_WIFI_PS_DISABLE
  bk_wifi_sta_pm_disable();
#endif
  beken_netdriver_init();
#endif

#ifdef CONFIG_BK7236N_BLE
  bk_bluetooth_init();
#endif

#ifdef CONFIG_ROSC_CALIB_SW
  bk_rosc_32k_calib();
#endif

#if defined(CONFIG_DEV_GPIO) && !defined(CONFIG_GPIO_LOWER_HALF)
  ret = beken_gpio_init();
  if (ret < 0)
    {
      gpioerr("Failed to initialize GPIO Driver: %d\n", ret);
    }
#endif

#if defined(CONFIG_PWM)
  beken_pwm_initialize("/dev/pwm0", 0);
#endif

#if defined(CONFIG_CAPTURE)
  beken_capture_initialize("/dev/capture0", 4);
#endif

#if defined(CONFIG_I2C_DRIVER)
  board_i2cdev_initialize(0);
#endif

#if defined(CONFIG_ADC)
  beken_adc_initialize("/dev/adc0", 0);
#endif

#if defined(CONFIG_BK7236N_MTD_PARTITION) && defined (CONFIG_KVDB_NVS) && defined (CONFIG_CM_KVDB_TEST)
  struct mtd_dev_s *mtd = beken_alloc_mtdpart(CONFIG_BK7236N_MTD_PARTITION_OFFSET,
                                              CONFIG_BK7236N_MTD_PARTITION_SIZE,
                                              false);
  ret = mtdconfig_register(mtd);
  if (ret < 0)
    {
       ferr("Failed to register MTD config:%d\n", ret);
    }

  struct mtd_dev_s *rammtd = rammtd_initialize(g_rammtd, RAM_MTD_SIZE);
  if (!rammtd)
    {
      ferr("Failed to create RAM MTD instance\n");
    }
  MTD_IOCTL(rammtd, MTDIOC_BULKERASE, 0);
  ret = mtdconfig_register_by_path(rammtd, CONFIG_KVDB_TEMPORARY_PATH);
  if (ret < 0)
    {
      ferr("Failed to register ram MTD: %d\n", ret);
    }

#elif defined(CONFIG_BK7236N_MTD_PARTITION)
  struct mtd_dev_s *mtd = beken_alloc_mtdpart(CONFIG_BK7236N_MTD_PARTITION_OFFSET,
                                              CONFIG_BK7236N_MTD_PARTITION_SIZE,
                                              false);
  ret = register_mtddriver(CONFIG_BK7236N_MTD_PARTITION_DEVPATH,
                                  mtd, 0755, NULL);
  if (ret < 0)
    {
       ferr("Failed to register MTD driver:%d\n", ret);
    }
#endif
  bk_err_t err = bk_pm_module_vote_cpu_freq(PM_DEV_ID_DEFAULT,PM_CPU_FRQ_240M);
  if (err)
    {
        ferr("pm freq vote failed");
    }

#ifdef CONFIG_ARCH_PERF_EVENTS
  up_perf_init((void *)PER_CLK);
#endif

  return OK;
}

/****************************************************************************
 * Name: board_early_initialize
 *
 * Description:
 *   If CONFIG_BOARD_EARLY_INITIALIZE is selected, then an additional
 *   initialization call will be performed in the boot-up sequence to a
 *   function called board_early_initialize().  board_early_initialize()
 *   will be called immediately after up_initialize() and well before
 *   board_early_initialize() is called and the initial application is
 *   started.  The context in which board_early_initialize() executes is
 *   suitable for early initialization of most, simple device drivers and
 *   is a logical, board-specific extension of up_initialize().
 *
 *   board_early_initialize() runs on the startup, initialization thread.
 *   Some initialization operations cannot be performed on the start-up,
 *   initialization thread.  That is because the initialization thread
 *   cannot wait for event.  Waiting may be required, for example, to
 *   mount a file system or or initialize a device such as an SD card.
 *   For this reason, such driver initialize must be deferred to
 *   board_late_initialize().

 ****************************************************************************/

#ifdef CONFIG_BOARD_EARLY_INITIALIZE
void board_early_initialize(void)
{
#ifdef CONFIG_WATCHDOG
  bk_wdt_driver_init();
  bk_wdt_stop();
#endif

  sys_drv_init();

  bk_gpio_driver_init();
  bk_uart_driver_init();

  reset_reason_init();

  bk_aon_rtc_driver_init();

  bk_timer_driver_init();

  bk_spi_driver_init();

#if CONFIG_BK7236N_FLASH_MTD
  beken_mtd_init();
#endif

#ifdef CONFIG_GENERAL_DMA
  bk_dma_driver_init();
#endif

#if CONFIG_OTP
  bk_otp_driver_init();
#endif

#ifdef CONFIG_SARADC
   bk_adc_driver_init();
#endif

#ifdef CONFIG_CKMN
  bk_ckmn_driver_init();
#endif

#if defined(CONFIG_TEMP_DETECT) || defined(CONFIG_VOLT_DETECT)
  bk_sensor_init();
#endif

#if defined(CONFIG_PWM) || defined(CONFIG_CAPTURE)
  bk_pwm_driver_init();
#endif

#if defined(CONFIG_I2C_DRIVER)
  bk_i2c_driver_init();
#endif

#if defined(CONFIG_DEV_RANDOM) || defined(CONFIG_DEV_URANDOM_ARCH)
  //TODO
  //bk_trng_driver_init();
#endif

#if defined(CONFIG_EFUSE)
  bk_efuse_driver_init();
#endif

  //TODO
  //extern int dubhe_driver_init(unsigned long dbh_base_addr);
  //dubhe_driver_init(SOC_SHANHAI_BASE);

#if 0
  bandgap_init();
#endif
}
#endif

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   If CONFIG_BOARD_LATE_INITIALIZE is selected, then an additional
 *   initialization call will be performed in the boot-up sequence to a
 *   function called board_late_initialize().  board_late_initialize() will
 *   be called after up_initialize() and board_early_initialize() and just
 *   before the initial application is started.  This additional
 *   initialization phase may be used, for example, to initialize board-
 *   specific device drivers for which board_early_initialize() is not
 *   suitable.
 *
 *   Waiting for events, use of I2C, SPI, etc are permissible in the context
 *   of board_late_initialize().  That is because board_late_initialize()
 *   will run on a temporary, internal kernel thread.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  /* Perform board-specific initialization */

  bk7236_evb_bringup();
}
#endif

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command BOARDIOC_INIT.
 *
 * Input Parameters:
 *   arg - The boardctl() argument is passed to the board_app_initialize()
 *         implementation without modification.  The argument has no
 *         meaning to NuttX; the meaning of the argument is a contract
 *         between the board-specific initialization logic and the
 *         matching application logic.  The value could be such things as a
 *         mode enumeration value, a set of DIP switch settings, a
 *         pointer to configuration data read from a file or serial FLASH,
 *         or whatever you would like to do with it.  Every implementation
 *         should accept zero/NULL as a default configuration.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure to indicate the nature of the failure.
 *
 ****************************************************************************/

int board_app_initialize(uintptr_t arg)
{
#ifdef CONFIG_BOARD_LATE_INITIALIZE
  /* Board initialization already performed by board_late_initialize() */

  return OK;
#else
  /* Perform board-specific initialization */

  return bk7236_evb_bringup();
#endif
}

/****************************************************************************
 * Name: board_app_finalinitialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command
 *   BOARDIOC_FINALINIT.
 *
 * Input Parameters:
 *   arg - The argument has no meaning.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure to indicate the nature of the failure.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARDCTL_FINALINIT
int board_app_finalinitialize(uintptr_t arg)
{
  return 0;
}
#endif

/****************************************************************************
 * Name: board_reset_cause
 *
 * Description:
 *   This interface may be used by application specific logic to get the
 *   cause of last reset. Support for this function is required by
 *   board-level logic if CONFIG_BOARDCTL_RESET is selected.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARDCTL_RESET_CAUSE
int board_reset_cause(FAR struct boardioc_reset_cause_s *cause)
{
  uint32_t reset = bk_misc_get_reset_reason();
  cause->cause = BOARDIOC_RESETCAUSE_NONE;

  if (reset == RESET_SOURCE_POWERON)
    {
      cause->cause = BOARDIOC_RESETCAUSE_SYS_CHIPPOR;
    }
  else if (reset == RESET_SOURCE_NMI_WDT)
    {
      cause->cause = BOARDIOC_RESETCAUSE_SYS_RWDT;
    }
  return OK;
}
#endif

/****************************************************************************
 * Name: board_reset
 *
 * Description:
 *   Reset board.  Support for this function is required by board-level
 *   logic if CONFIG_BOARDCTL_RESET is selected.
 *
 * Input Parameters:
 *   status - Status information provided with the reset event.  This
 *            meaning of this status information is board-specific.  If not
 *            used by a board, the value zero may be provided in calls to
 *            board_reset().
 *
 * Returned Value:
 *   If this function returns, then it was not possible to power-off the
 *   board due to some constraints.  The return value int this case is a
 *   board-specific reason for the failure to shutdown.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARDCTL_RESET
int board_reset(int status)
{
  bk_reboot();
  return OK;
}
#endif

