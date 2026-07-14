# ##############################################################################
# vendor/beken/chips/bk7236n/CMakeLists.txt
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

set(BEKEN_CHIP_DEFS
    CONFIG_DEBUG_FIRMWARE=0
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

set(BEKEN_CHIP_INCLUDES
    ${NUTTX_CHIP_ABS_DIR}/include
    ${NUTTX_CHIP_ABS_DIR}/bk_idk/armino_as_lib/include
    ${NUTTX_CHIP_ABS_DIR}/bk_idk/armino_as_lib/bk7236n/config)

if(TARGET nuttx)
  target_link_options(
    nuttx
    PRIVATE
      "LINKER:--build-id=none"
      "LINKER:--entry=Reset_Handler")
endif()

set(SRCS
    beken_start.c
    beken_irq.c
    beken_lowputc.c
    beken_uart.c
    beken_allocateheap.c
    beken_timerisr.c
    beken_os_adapt.c
    beken_interrupt_base.c
    beken_idle.c
    )

if(CONFIG_PM)
  list(APPEND SRCS beken_pminitialize.c)
endif()

if(CONFIG_RTC_DRIVER)
  list(APPEND SRCS beken_rtc_lowerhalf.c)
endif()

if(CONFIG_BK7236N_FLASH_MTD)
  list(APPEND SRCS beken_flash.c)
endif()

if(CONFIG_TIMER)
  list(APPEND SRCS beken_tim_lowerhalf.c)
endif()

if(CONFIG_ONESHOT)
  list(APPEND SRCS beken_oneshot_lowerhalf.c)
endif()

if(CONFIG_SPI_DRIVER)
  list(APPEND SRCS beken_spi.c)
endif()

if(CONFIG_SPI_SLAVE_DRIVER)
  list(APPEND SRCS beken_spi_slave.c)
endif()

if(CONFIG_WATCHDOG)
  list(APPEND SRCS beken_wdt_lowerhalf.c)
endif()

if(CONFIG_PWM OR CONFIG_CAPTURE)
  list(APPEND SRCS beken_pwm_lowerhalf.c)
endif()

if(CONFIG_ADC)
  list(APPEND SRCS beken_adc_lowerhalf.c)
endif()

if(CONFIG_I2C_DRIVER)
  list(APPEND SRCS beken_i2c_lowerhalf.c)
endif()

if(CONFIG_DEV_RANDOM OR CONFIG_DEV_URANDOM_ARCH)
  list(APPEND SRCS beken_trng_lowerhalf.c)
endif()

if(CONFIG_NET AND CONFIG_BK7236N_WIFI)
  list(APPEND SRCS beken_wlan.c)
endif()

if(CONFIG_NET AND NOT CONFIG_NETDEV_LATEINIT)
  list(APPEND SRCS beken_netinit.c)
endif()

if(CONFIG_CRYPTO)
  list(APPEND SRCS beken_crypto.c beken_aes.c)
endif()

# Compile chip sources as OBJECT files and link them directly into nuttx.
# This ensures symbols such as bk_int_isr_register (strong) are always
# present and override the weak stubs in armino_as_lib/libdriver.a.
add_library(beken_chip OBJECT ${SRCS})
add_dependencies(beken_chip nuttx_context)
nuttx_add_library_internal(beken_chip)

target_compile_options(
  beken_chip
  PRIVATE $<GENEX_EVAL:$<TARGET_PROPERTY:nuttx,NUTTX_KERNEL_COMPILE_OPTIONS>>)
target_compile_definitions(
  beken_chip
  PRIVATE $<GENEX_EVAL:$<TARGET_PROPERTY:nuttx,NUTTX_KERNEL_DEFINITIONS>>)
target_include_directories(
  beken_chip
  PRIVATE $<GENEX_EVAL:$<TARGET_PROPERTY:nuttx,NUTTX_KERNEL_INCLUDE_DIRECTORIES>>)

target_include_directories(beken_chip PRIVATE ${BEKEN_CHIP_INCLUDES})
target_include_directories(beken_chip PRIVATE ${CMAKE_SOURCE_DIR}/sched)
target_compile_definitions(beken_chip PRIVATE ${BEKEN_CHIP_DEFS})
target_compile_options(beken_chip PRIVATE -Wno-error=strict-prototypes)

# Reuse arch header paths (ARM common, chip, etc.).
target_link_libraries(beken_chip PRIVATE arch)

if(TARGET nuttx)
  add_dependencies(nuttx beken_chip)
  target_link_libraries(nuttx PRIVATE $<TARGET_OBJECTS:beken_chip>)
endif()

set_property(
  TARGET nuttx_target_interface
  APPEND
  PROPERTY ALL_TARGETS beken_chip)
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/targets "beken_chip\n")
