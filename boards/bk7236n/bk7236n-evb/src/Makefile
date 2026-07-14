#############################################################################
# vendor/beken/boards/bk7236n-evb/src/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
#############################################################################

include $(TOPDIR)/Make.defs

CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)/$(CONFIG_ARCH_CHIP_CUSTOM_DIR)/bk_idk/armino_as_lib/include
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)/$(CONFIG_ARCH_CHIP_CUSTOM_DIR)/bk_idk/armino_as_lib/bk7236n/config
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)/$(CONFIG_ARCH_CHIP_CUSTOM_DIR)/../bk7236n

CFLAGS += -DCONFIG_DEBUG_FIRMWARE=0 -DCONFIG_SOC_BK7234=0 -DCONFIG_BK_WHILE=0 -DCONFIG_MEM_DEBUG=0 -DCONFIG_GPIO_RETENTION_SUPPORT=0 -DCONFIG_ARCH_RISCV=0 -DCONFIG_AON_RTC_DEBUG=0 -DSARADC_AUTOTEST=0
CFLAGS += -DCONFIG_FREERTOS_SMP_TEMP=0 -DCONFIG_MALLOC_STATIS=0 -DCONFIG_INCLUDE_WPA2_ENTERPRISE=0 -DCONFIG_STDIO_PRINTF=0 -DCONFIG_DUMP_ENABLE=0 -DCONFIG_SOC_BK7231N=0
CFLAGS += -DCONFIG_FREERTOS=0 -DCONFIG_SUPPORT_LOG_LEVEL_CFG=0 -DCONFIG_GPIO_DEFAULT_SET_SUPPORT=0 -DCONFIG_GPIO_ANA_WAKEUP_SUPPORT=0
CFLAGS += -DCONFIG_CAN=0 -DCONFIG_LCD=0 -DCONFIG_AUDIO=0 -DCONFIG_LIN=0 -DCONFIG_SYS_CPU1=0 -DCONFIG_SOC_BK7236=0
CFLAGS += -DCONFIG_WIFI_BAND_POLICY=0 -DCONFIG_WIFI_STA_SAME_SSID_BAND_PREF=0 -DCONFIG_WIFI_AP_HW_MODE=0 -DCONFIG_WIFI_WIRELESS_MODE_SUPPORT=0 -DCONFIG_WIFI_CSI_EN=0
CFLAGS += -DCONFIG_FLASH_V1P1=0 -DCONFIG_SUPPORT_XMIT_READY_CALLBACK=0 -DCONFIG_FLASH_OTP=0
CFLAGS += -DCONFIG_AON_RTC_64BIT=0 -DCONFIG_AON_RTC=0 -DCONFIG_RTC_ANA_WAKEUP_SUPPORT=0
CFLAGS += -DCONFIG_DEEP_LV=0 -DCONFIG_TOUCH=0 -DCONFIG_JPEGENC_HW=0 -DCONFIG_JPEGDEC_HW=0 -DCONFIG_SPI_BYTE_INTERVAL=0
CFLAGS += -Wno-error=strict-prototypes

CSRCS = beken_ap.c

ifeq ($(CONFIG_SPI_DRIVER),y)
CSRCS += beken_board_spidev.c
endif

ifeq ($(CONFIG_SPI_SLAVE_DRIVER),y)
CSRCS += beken_board_spislavedev.c
endif

ifeq ($(CONFIG_I2C_DRIVER),y)
CSRCS += beken_board_i2cdev.c
endif

ifeq ($(CONFIG_DEV_GPIO),y)
ifneq ($(CONFIG_GPIO_LOWER_HALF),y)
CSRCS += beken_gpio.c
endif
endif

RCSRCS = etc/init.d/rc.sysinit etc/init.d/rcS
RCRAWS = etc/group
RCRAWS = etc/build.prop

include $(TOPDIR)/boards/Board.mk
