# ##############################################################################
# vendor/beken/boards/bk7236n/bk7236n-evb/src/CMakeLists.txt
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

set(SRCS beken_ap.c)

if(CONFIG_SPI_DRIVER)
  list(APPEND SRCS beken_board_spidev.c)
endif()

if(CONFIG_SPI_SLAVE_DRIVER)
  list(APPEND SRCS beken_board_spislavedev.c)
endif()

if(CONFIG_I2C_DRIVER)
  list(APPEND SRCS beken_board_i2cdev.c)
endif()

if(CONFIG_DEV_GPIO AND NOT CONFIG_GPIO_LOWER_HALF)
  list(APPEND SRCS beken_gpio.c)
endif()

target_sources(board PRIVATE ${SRCS})

target_include_directories(
  board
  PRIVATE ${NUTTX_CHIP_ABS_DIR}/bk_idk/armino_as_lib/include
          ${NUTTX_CHIP_ABS_DIR}/bk_idk/armino_as_lib/bk7236n/config
          ${NUTTX_CHIP_ABS_DIR}/../bk7236n)

target_compile_definitions(
  board
  PRIVATE CONFIG_DEBUG_FIRMWARE=0
          CONFIG_SOC_BK7234=0
          CONFIG_BK_WHILE=0
          CONFIG_MEM_DEBUG=0
          CONFIG_GPIO_RETENTION_SUPPORT=0
          CONFIG_ARCH_RISCV=0
          CONFIG_AON_RTC_DEBUG=0
          SARADC_AUTOTEST=0
          CONFIG_FREERTOS_SMP_TEMP=0
          CONFIG_MALLOC_STATIS=0
          CONFIG_INCLUDE_WPA2_ENTERPRISE=0
          CONFIG_STDIO_PRINTF=0
          CONFIG_DUMP_ENABLE=0
          CONFIG_SOC_BK7231N=0
          CONFIG_FREERTOS=0
          CONFIG_SUPPORT_LOG_LEVEL_CFG=0
          CONFIG_GPIO_DEFAULT_SET_SUPPORT=0
          CONFIG_GPIO_ANA_WAKEUP_SUPPORT=0
          CONFIG_CAN=0
          CONFIG_LCD=0
          CONFIG_AUDIO=0
          CONFIG_LIN=0
          CONFIG_SYS_CPU1=0
          CONFIG_SOC_BK7236=0
          CONFIG_WIFI_BAND_POLICY=0
          CONFIG_WIFI_STA_SAME_SSID_BAND_PREF=0
          CONFIG_WIFI_AP_HW_MODE=0
          CONFIG_WIFI_WIRELESS_MODE_SUPPORT=0
          CONFIG_WIFI_CSI_EN=0
          CONFIG_FLASH_V1P1=0
          CONFIG_SUPPORT_XMIT_READY_CALLBACK=0
          CONFIG_FLASH_OTP=0
          CONFIG_AON_RTC_64BIT=0
          CONFIG_AON_RTC=0
          CONFIG_RTC_ANA_WAKEUP_SUPPORT=0
          CONFIG_DEEP_LV=0
          CONFIG_TOUCH=0
          CONFIG_JPEGENC_HW=0
          CONFIG_JPEGDEC_HW=0
          CONFIG_SPI_BYTE_INTERVAL=0)

target_compile_options(board PRIVATE -Wno-error=strict-prototypes)

nuttx_add_romfs(
  NAME
  etc
  MOUNTPOINT
  etc
  RCSRCS
  etc/init.d/rcS
  etc/init.d/rc.sysinit
  RCRAWS
  etc/group
  etc/build.prop
  PATH
  ${CMAKE_CURRENT_BINARY_DIR}/etc)

target_link_libraries(board PRIVATE romfs_etc)
