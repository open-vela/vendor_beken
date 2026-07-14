/****************************************************************************
 * vendor/beken/chip/bk7236n/beken_uart.c
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
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/serial/serial.h>
#include <nuttx/power/pm.h>
#include <arch/board/board.h>

#include "arm_internal.h"
#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include "beken_lowputc.h"
#include "beken_uart.h"
#include "driver/uart.h"
#include "beken_interrupt_base.h"

/****************************************************************************
 * External Defined Functions
 ****************************************************************************/

extern void uart0_init(unsigned long ulbaudrate);

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct beken_serial_cfg
{
  uart_config_t  dev;            /* Generic UART device */
  uart_id_t uart_id;
  uint8_t irq;            /* IRQ associated with this uart */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_PM
static void up_pm_notify(struct pm_callback_s *cb, int dowmin,
                              enum pm_state_e pmstate);
static int  up_pm_prepare(struct pm_callback_s *cb, int domain,
                                enum pm_state_e pmstate);
#endif

static struct uart_dev_s *g_uart_isr_dev;


static int  beken_serial_setup(struct uart_dev_s *dev);
static void beken_serial_shutdown(struct uart_dev_s *dev);
static int  beken_serial_attach(struct uart_dev_s *dev);
static void beken_serial_detach(struct uart_dev_s *dev);
static int  beken_serial_ioctl(struct file *filep, int cmd,
                                 unsigned long arg);
static int  beken_serial_receive(struct uart_dev_s *dev,
                                   unsigned int *status);
static void beken_serial_rxint(struct uart_dev_s *dev, bool enable);
static bool beken_serial_rxavailable(struct uart_dev_s *dev);
#ifdef CONFIG_SERIAL_IFLOWCONTROL
static bool beken_serial_rxflowcontrol(struct uart_dev_s *dev,
#endif
static void beken_serial_send(struct uart_dev_s *dev, int ch);
static void beken_serial_txint(struct uart_dev_s *dev, bool enable);
static bool beken_serial_txready(struct uart_dev_s *dev);
static bool beken_serial_txempty(struct uart_dev_s *dev);

/****************************************************************************
 * Private Variables
 ****************************************************************************/

static struct uart_ops_s g_uart_ops =
{
  .setup         = beken_serial_setup,
  .shutdown      = beken_serial_shutdown,
  .attach        = beken_serial_attach,
  .detach        = beken_serial_detach,
  .ioctl         = beken_serial_ioctl,
  .receive       = beken_serial_receive,
  .rxint         = beken_serial_rxint,
  .rxavailable   = beken_serial_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
  .rxflowcontrol = beken_serial_rxflowcontrol,
#endif
  .send          = beken_serial_send,
  .txint         = beken_serial_txint,
  .txready       = beken_serial_txready,
  .txempty       = beken_serial_txempty,
};

static char g_uart0_rxbuffer[CONFIG_UART0_RXBUFSIZE];
static char g_uart0_txbuffer[CONFIG_UART0_TXBUFSIZE];

struct beken_serial_cfg  g_uart0_dev =
{
  .dev         =
  {
    .baud_rate = UART_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_NONE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_FLOWCTRL_DISABLE,
    .src_clk   = UART_SCLK_XTAL_26M
  },
  .uart_id = UART_ID_0,
  .irq     = INT_SRC_UART0,
};

/* Fill only the requested fields */

static uart_dev_t g_uart0_priv =
{
  .isconsole = true,
  .recv      =
  {
    .size    = CONFIG_UART0_RXBUFSIZE,
    .buffer  = g_uart0_rxbuffer,
  },
  .xmit      =
  {
    .size    = CONFIG_UART0_TXBUFSIZE,
    .buffer  = g_uart0_txbuffer,
  },
  .ops       = &g_uart_ops,
  .priv      = &g_uart0_dev,
};

#ifdef CONFIG_PM
static  struct pm_callback_s g_serialcb =
{
  .notify  = up_pm_notify,
  .prepare = up_pm_prepare,
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_pm_notify
 *
 * Description:
 *   Notify the driver of new power state. This callback is  called after
 *   all drivers have had the opportunity to prepare for the new power state.
 *
 * Input Parameters:
 *
 *    cb - Returned to the driver. The driver version of the callback
 *         structure may include additional, driver-specific state data at
 *         the end of the structure.
 *
 *    pmstate - Identifies the new PM state
 *
 * Returned Value:
 *   None - The driver already agreed to transition to the low power
 *   consumption state when when it returned OK to the prepare() call.
 *
 *
 ****************************************************************************/

#ifdef CONFIG_PM
static void up_pm_notify(struct pm_callback_s *cb, int domain,
                              enum pm_state_e pmstate)
{
  switch (pmstate)
    {
      case (PM_NORMAL):
        {
          /* Logic for PM_NORMAL goes here */
          sinfo("Switch to PM_NORMAL \r\n");
        }
        break;

      case (PM_IDLE):
        {
          /* Logic for PM_IDLE goes here */
          sinfo("Switch to PM_IDLE \r\n");
        }
        break;

      case (PM_STANDBY):
        {
          /* Logic for PM_STANDBY goes here */
          sinfo("Switch to PM_STANDBY \r\n");
        }
        break;

      case (PM_SLEEP):
        {
          /* Logic for PM_SLEEP goes here */
          sinfo("Switch to PM_SLEEP \r\n");
        }
        break;

      default:
        {
          /* Should not get here */
        }
        break;
    }
}
#endif

/****************************************************************************
 * Name: up_pm_prepare
 *
 * Description:
 *   Request the driver to prepare for a new power state. This is a warning
 *   that the system is about to enter into a new power state. The driver
 *   should begin whatever operations that may be required to enter power
 *   state. The driver may abort the state change mode by returning a
 *   non-zero value from the callback function.
 *
 * Input Parameters:
 *
 *    cb - Returned to the driver. The driver version of the callback
 *         structure may include additional, driver-specific state data at
 *         the end of the structure.
 *
 *    pmstate - Identifies the new PM state
 *
 * Returned Value:
 *   Zero - (OK) means the event was successfully processed and that the
 *          driver is prepared for the PM state change.
 *
 *   Non-zero - means that the driver is not prepared to perform the tasks
 *              needed achieve this power setting and will cause the state
 *              change to be aborted. NOTE: The prepare() method will also
 *              be called when reverting from lower back to higher power
 *              consumption modes (say because another driver refused a
 *              lower power state change). Drivers are not permitted to
 *              return non-zero values when reverting back to higher power
 *              consumption modes!
 *
 *
 ****************************************************************************/

#ifdef CONFIG_PM
static int up_pm_prepare(struct pm_callback_s *cb, int domain,
                               enum pm_state_e pmstate)
{
  /* Logic to prepare for a reduced power state goes here. */

  return OK;
}
#endif

/****************************************************************************
 * Name: beken_uart_rx_isr
 *
 * Description:
 *   This is the UART rx interrupt handler.  It will be invoked when an
 *   interrupt is received on the 'irq'.  It should call
 *   uart_recvchars to perform the appropriate data transfers.  The
 *   interrupt handling logic must be able to map the 'irq' number into the
 *   appropriate uart_dev_s structure in order to call these functions.
 *
 ****************************************************************************/

static void beken_uart_rx_isr(uart_id_t id, void *param)
{
    struct uart_dev_s *dev = (struct uart_dev_s *)param;
    uart_recvchars(dev);
}

/****************************************************************************
 * Name: beken_uart_tx_isr
 *
 * Description:
 *   This is the UART tx interrupt handler.  It will be invoked when an
 *   interrupt is received on the 'irq'.  It should call uart_xmitchars
 *   to perform the appropriate data transfers.  The
 *   interrupt handling logic must be able to map the 'irq' number into the
 *   appropriate uart_dev_s structure in order to call these functions.
 *
 ****************************************************************************/

static void beken_uart_tx_isr(uart_id_t id, void *param)
{
    struct uart_dev_s *dev = (struct uart_dev_s *)param;
    uart_xmitchars(dev);
}

/****************************************************************************
 * Name: beken_serial_setup
 *
 * Description:
 *      Configure the UART baud, bits, parity, fifos, etc. This method is
 *      called the first time that the serial port is opened.
 *      For the serial console, this will occur very early in initialization,
 *      for other serial ports this will occur when the port is first opened.
 *      This setup does not include attaching or enabling interrupts.
 *      That portion of the UART setup is performed when the attach() method
 *      is called.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *
 * Returned Values:
 *   Zero (OK) is returned.
 *
 ****************************************************************************/

static int beken_serial_setup(struct uart_dev_s *dev)
{
  /*configured by internal driver libs*/
  return OK;
}

/****************************************************************************
 * Name: beken_serial_shutdown
 *
 * Description:
 * Disable the UART.  This method is called when the serial port is closed.
 * This method reverses the operation the setup method.  NOTE that the serial
 * console is never shutdown.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *
 ****************************************************************************/

static void beken_serial_shutdown(struct uart_dev_s *dev)
{
  DEBUGASSERT(dev != NULL);

  struct  beken_serial_cfg *priv = ( struct beken_serial_cfg *)dev->priv;

  bk_uart_deinit(priv->uart_id);

}

/****************************************************************************
 * Name: beken_serial_attach
 *
 * Description:
 *   Configure the UART to operation in interrupt driven mode.  This method
 *   is called when the serial port is opened.  Normally, this is just after
 *   the the setup() method is called, however, the serial console may
 *   operate in a non-interrupt driven mode during the boot phase.
 *
 *   RX and TX interrupts are not enabled when by the attach method (unless
 *   the hardware supports multiple levels of interrupt enabling).  The RX
 *   and TX interrupts are not enabled until the txint() and rxint() methods
 *   are called.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *
 * Returned Values:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/

static int beken_serial_attach(struct uart_dev_s *dev)
{
  DEBUGASSERT(dev != NULL);

  struct beken_serial_cfg *priv = (struct beken_serial_cfg *)dev->priv;
  int ret;

  g_uart_isr_dev = dev;

  /* Attach and enable the IRQ */
  ret = bk_uart_register_rx_isr(priv->uart_id, beken_uart_rx_isr, (void *)dev);
  if (ret != OK)
    {
      return ERROR;
    }
  ret = bk_uart_register_tx_isr(priv->uart_id, beken_uart_tx_isr, (void *)dev);
  if (ret != OK)
    {
      return ERROR;
    }

  return OK;
}

/****************************************************************************
 * Name: beken_serial_detach
 *
 * Description:
 *   Detach UART interrupts.  This method is called when the serial port is
 *   closed normally just before the shutdown method is called.  The
 *   exception is the serial console which is never shutdown.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *
 ****************************************************************************/

static void beken_serial_detach(struct uart_dev_s *dev)
{
  DEBUGASSERT(dev != NULL);

  struct beken_serial_cfg *priv = (struct beken_serial_cfg *)dev->priv;

  bk_int_isr_unregister(priv->irq);
}

/****************************************************************************
 * Name: beken_serial_ioctl
 *
 * Description:
 *   All ioctl calls will be routed through this method.
 *   Here it's employed to implement the TERMIOS ioctls and TIOCSERGSTRUCT.
 *
 * Parameters:
 *   filep    Pointer to a file structure instance.
 *   cmd      The ioctl command.
 *   arg      The argument of the ioctl cmd.
 *
 * Returned Value:
 *   Returns a non-negative number on success;  A negated errno value is
 *   returned on any failure (see comments ioctl() for a list of appropriate
 *   errno values).
 *
 ****************************************************************************/

static int beken_serial_ioctl(struct file *filep, int cmd,
                                      unsigned long arg)
{
  int ret = OK;
  return ret;
}

/****************************************************************************
 * Name: beken_serial_receive
 *
 * Description:
 *   Called (usually) from the interrupt level to receive one
 *   character from the UART.  Error bits associated with the
 *   receipt are provided in the return 'status'.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *   status     -  Pointer to a variable to store eventual error bits.
 *
 * Returned Values:
 *   Return the byte read from the RX FIFO.
 *
 ****************************************************************************/

static int beken_serial_receive(struct uart_dev_s *dev,
                                        unsigned int *status)
{
  DEBUGASSERT(dev != NULL);
  uint32_t rx_fifo;
  uint8_t rx_data;
  struct beken_serial_cfg *priv =(struct beken_serial_cfg *) dev->priv;

  bk_uart_read_bytes(priv->uart_id, &rx_data, 1, 0);
  rx_fifo = rx_data & 0xFF;

  /* Since we don't have error bits associated with receipt, we set zero */

  *status = 0;

  return  (int) rx_fifo;
}

/****************************************************************************
 * Name: beken_serial_rxint
 *
 * Description:
 *   Enable or disable RX interrupts.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *   enable     -  If true enables the RX interrupt, if false disables it.
 *
 ****************************************************************************/

static void beken_serial_rxint(struct uart_dev_s *dev, bool enable)
{
  irqstate_t flags;
  DEBUGASSERT(dev != NULL);
  flags = enter_critical_section();
  struct beken_serial_cfg *priv =(struct beken_serial_cfg *) dev->priv;

  if (enable)
    {
      bk_uart_enable_rx_interrupt(priv->uart_id);
    }
  else
    {
      bk_uart_disable_rx_interrupt(priv->uart_id);
    }

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: beken_serial_rxavailable
 *
 * Description:
 *   Check if there is any data available to be read.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *
 * Returned Values:
 *   Return true if the RX FIFO is not empty and false if RX FIFO is empty.
 *
 ****************************************************************************/

static bool beken_serial_rxavailable(struct uart_dev_s *dev)
{
  DEBUGASSERT(dev != NULL);

  struct beken_serial_cfg *priv =(struct beken_serial_cfg *) dev->priv;

  return bk_uart_read_fifo_is_read(priv->uart_id);
}

/****************************************************************************
 * Name: beken_serial_rxflowcontrol
 *
 * Description:
 *   Called when Rx buffer is full (or exceeds configured watermark levels
 *   if CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS is defined).
 *   Return true if UART activated RX flow control to block more incoming
 *   data
 *
 * Input Parameters:
 *   dev       - UART device instance
 *   nbuffered - the number of characters currently buffered
 *               (if CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS is
 *               not defined the value will be 0 for an empty buffer or the
 *               defined buffer size for a full buffer)
 *   upper     - true indicates the upper watermark was crossed where
 *               false indicates the lower watermark has been crossed
 *
 * Returned Value:
 *   true if RX flow control activated.
 *
 ****************************************************************************/

#ifdef CONFIG_SERIAL_IFLOWCONTROL
static bool beken_serial_rxflowcontrol(struct uart_dev_s *dev,
                                                  unsigned int nbuffered, bool upper)
{
  return false;
}
#endif

/****************************************************************************
 * Name: beken_serial_send
 *
 * Description:
 *    Send a unique character
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *   ch         -  Byte to be sent.
 *
 ****************************************************************************/

static void beken_serial_send(struct uart_dev_s *dev, int ch)
{
  DEBUGASSERT(dev != NULL);

  struct beken_serial_cfg *priv =(struct beken_serial_cfg *) dev->priv;

  bk_uart_write_bytes(priv->uart_id, (const void *) (&ch), 1);

}

/****************************************************************************
 * Name: beken_serial_txint
 *
 * Description:
 *    Enable or disable TX interrupts.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *   enable     -  If true enables the TX interrupt, if false disables it.
 *
 ****************************************************************************/

static void beken_serial_txint(struct uart_dev_s *dev, bool enable)
{
  irqstate_t flags;
  DEBUGASSERT(dev != NULL);
  flags = enter_critical_section();
  struct beken_serial_cfg *priv =(struct beken_serial_cfg *) dev->priv;

  if (enable)
    {
      bk_uart_enable_tx_interrupt(priv->uart_id);
    }
  else
    {
      bk_uart_disable_tx_interrupt(priv->uart_id);
    }

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: beken_serial_txready
 *
 * Description:
 *    Check if the transmit hardware is ready to send another byte.
 *    This is used to determine if send() method can be called.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *
 * Returned Values:
 *   Return true if the transmit hardware is ready to send another byte,
 *   false otherwise.
 *
 ****************************************************************************/

static bool beken_serial_txready(struct uart_dev_s *dev)
{
  DEBUGASSERT(dev != NULL);
  struct beken_serial_cfg *priv =(struct beken_serial_cfg *) dev->priv;

  return bk_uart_is_tx_ready(priv->uart_id);
}

/****************************************************************************
 * Name: beken_serial_txempty
 *
 * Description:
 *    Verify if all characters have been sent. If for example, the UART
 *    hardware implements FIFOs, then this would mean the transmit FIFO is
 *    empty. This method is called when the driver needs to make sure that
 *    all characters are "drained" from the TX hardware.
 *
 * Parameters:
 *   dev        -  Pointer to the serial driver struct.
 *
 * Returned Values:
 *   Return true if the TX FIFO is empty, false if it is not.
 *
 ****************************************************************************/

static bool beken_serial_txempty(struct uart_dev_s *dev)
{
  DEBUGASSERT(dev != NULL);
  struct beken_serial_cfg *priv =(struct beken_serial_cfg *) dev->priv;

  return bk_uart_is_tx_fifo_empty(priv->uart_id);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_flush_uart_fifo
 *
 * Description:
 *  Flush fifo of uart.
 *
 ****************************************************************************/

void beken_flush_uart_fifo(void)
{
  struct beken_serial_cfg *priv =(struct beken_serial_cfg *) g_uart0_priv.priv;

  bk_uart_hal_flush_fifo(priv->uart_id);

}

/****************************************************************************
 * Name: arm_earlyserialinit
 *
 * Description:
 *   Performs the low level USART initialization early in debug so that the
 *   serial console will be available during bootup.  This must be called
 *   before arm_serialinit.
 *
 ****************************************************************************/

void arm_earlyserialinit(void)
{
    uart0_init(115200);
}

/****************************************************************************
 * Name: arm_serialinit
 *
 * Description:
 *   Register serial console and serial ports.  This assumes
 *   that arm_earlyserialinit was called previously.
 *
 ****************************************************************************/

void arm_serialinit(void)
{
  /* Register the console */
#ifdef CONFIG_PM
    int ret;

    ret = pm_register(&g_serialcb);
    DEBUGASSERT(ret == OK);
    UNUSED(ret);
#endif

    uart_register("/dev/console", &g_uart0_priv);
}

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug writes
 *
 ****************************************************************************/

void up_putc(int ch)
{
  /* Check for LF */
  if (ch == '\n')
    {
      /* Add CR */

      arm_lowputc('\r');
    }

  arm_lowputc(ch);
  //return ch;
}
