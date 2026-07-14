/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_uart.h
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

#ifndef __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_UART_H
#define __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_UART_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

/****************************************************************************
 * Pre-processor Prototypes
 ****************************************************************************/

#define CONFIG_UART0_RXBUFSIZE   256
#define CONFIG_UART0_TXBUFSIZE   256

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: beken_flush_uart_fifo
 *
 * Description:
 *  Flush fifo of uart.
 *
 ****************************************************************************/

void beken_flush_uart_fifo(void);

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif


#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __VENDOR_BEKEN_CHIPS_BK7236N_BEKEN_UART_H */
