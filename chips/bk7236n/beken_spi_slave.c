/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_spi_slave.c
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

#if defined(CONFIG_SPI_SLAVE_DRIVER)

#include <assert.h>
#include <debug.h>
#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <nuttx/semaphore.h>
#include <nuttx/spi/spi.h>
#include <nuttx/spi/slave.h>

#include <arch/board/board.h>

#include <driver/gpio.h>
#include <driver/spi.h>
#include <driver/hal/hal_gpio_types.h>
#include <driver/spi_types.h>
#include <beken_spi_slave.h>

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* The max frequency that the spi supports */
#define CONFIG_SPI_MAX_BAUD_RATE          49000000
#define CONFIG_SPI_8_BITS_NUM_PER_WORD    8
#define CONFIG_SPI_16_BITS_NUM_PER_WORD   16
#define CONFIG_SPI_DEFAULT_FREQUENCY      10000000

#define SPI_SLAVE_BUFSIZE                 64

#define WORDS2BYTES(_priv, _wn)   ((_wn) * ((_priv)->config->spi_config.bit_width ? 2 : 1))
#define BYTES2WORDS(_priv, _bn)   ((_bn) / ((_priv)->config->spi_config.bit_width ? 2 : 1))

/* SPI Slave controller hardware configuration */

/* SPI Device hardware configuration */
struct beken_config_s
{
  spi_config_t                spi_config;
  spi_id_t                    spi_id;
};

struct spislave_priv_s
{
  /* Externally visible part of the SPI Slave controller interface */

  struct spi_slave_ctrlr_s    ctrlr;

  /* Reference to SPI Slave device interface */

  struct spi_slave_dev_s      *dev;

  /* Port configuration */

  struct beken_config_s       *config;
  gpio_id_t                   cs_pin_num;        /* SPI CS pin */
  int                         refs;              /* Reference count */
  uint32_t                    tx_length;         /* Location of next TX value */
  uint32_t                    tx_send_length;    /* Tx has send length */
  /* SPI Slave TX queue buffer */
  uint8_t                     tx_buffer[SPI_SLAVE_BUFSIZE];

  uint32_t                    rx_length;         /* Location of next RX value */

  /* SPI Slave RX queue buffer */
  uint8_t                     rx_buffer[SPI_SLAVE_BUFSIZE];

  /* Flag that indicates whether SPI Slave is currently processing */
  bool                        is_processing;

  /* Flag that indicates whether SPI Slave TX is currently enabled */
  bool                        is_tx_enabled;

  /* Flag that indicates whether SPI Slave Rx buffer is full*/
  bool                        rx_buffer_full;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* SPI Slave controller interrupt handlers */
static void  beken_rx_interrupt_callback(spi_id_t id, void *param);
static void  beken_rx_finish_interrupt_callback(spi_id_t id, void *param);
static void  beken_tx_finish_interrupt_callback(spi_id_t id, void *param);
static void beken_spislave_setmode(struct spi_slave_ctrlr_s *ctrlr,
                                             enum spi_mode_e mode);
static void beken_spislave_setbits(struct spi_slave_ctrlr_s *ctrlr, int nbits);

/* SPI Slave controller internal functions */

static void beken_spislave_evict_sent_data(struct spislave_priv_s *priv,
                                                        uint32_t sent_bytes);
static void beken_spislave_prepare_next_tx(struct spislave_priv_s *priv);
static void beken_spislave_initialize(struct spi_slave_ctrlr_s *ctrlr);
static void beken_spislave_deinitialize(struct spi_slave_ctrlr_s *ctrlr);

/* SPI Slave controller operations */

static void beken_spislave_bind(struct spi_slave_ctrlr_s *ctrlr,
                                        struct spi_slave_dev_s *dev,
                                        enum spi_slave_mode_e mode,
                                        int nbits);
static void beken_spislave_unbind(struct spi_slave_ctrlr_s *ctrlr);
static int beken_spislave_enqueue(struct spi_slave_ctrlr_s *ctrlr,
                                           const void *data,
                                           size_t nwords);
static bool beken_spislave_qfull(struct spi_slave_ctrlr_s *ctrlr);
static void beken_spislave_qflush(struct spi_slave_ctrlr_s *ctrlr);
static size_t beken_spislave_qpoll(struct spi_slave_ctrlr_s *ctrlr);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct beken_config_s beken_spi_slave0_cfg =
{
  .spi_config =
    {
      .role          = SPI_ROLE_SLAVE,
      .bit_width     = SPI_BIT_WIDTH_8BITS,
      .polarity      = SPI_POLARITY_LOW,
      .phase         = SPI_PHASE_1ST_EDGE,
      .wire_mode     = SPI_4WIRE_MODE,
      .baud_rate     = CONFIG_SPI_DEFAULT_FREQUENCY,
      .bit_order     = SPI_MSB_FIRST,
    },
  .spi_id            = SPI_ID_0,
};

static struct beken_config_s beken_spi_slave1_cfg =
{
  .spi_config =
    {
      .role          = SPI_ROLE_SLAVE,
      .bit_width     = SPI_BIT_WIDTH_8BITS,
      .polarity      = SPI_POLARITY_LOW,
      .phase         = SPI_PHASE_1ST_EDGE,
      .wire_mode     = SPI_4WIRE_MODE,
      .baud_rate     = CONFIG_SPI_DEFAULT_FREQUENCY,
      .bit_order     = SPI_MSB_FIRST,
    },
  .spi_id            = SPI_ID_1,
};

static const struct spi_slave_ctrlrops_s beken_spislave_ops =
{
  .bind             = beken_spislave_bind,
  .unbind           = beken_spislave_unbind,
  .enqueue          = beken_spislave_enqueue,
  .qfull            = beken_spislave_qfull,
  .qflush           = beken_spislave_qflush,
  .qpoll            = beken_spislave_qpoll
};

static struct spislave_priv_s beken_spi_slave0_priv =
{
  .ctrlr            =
                     {
                       .ops = &beken_spislave_ops
                     },
  .dev              = NULL,
  .config           = &beken_spi_slave0_cfg,
  .cs_pin_num       = GPIO_15,
  .refs             = 0,
  .tx_length        = 0,
  .tx_send_length   = 0,
  .tx_buffer        =
                     {
                       0
                     },
  .rx_length        = 0,
  .rx_buffer        =
                     {
                       0
                     },
  .is_processing    = false,
  .is_tx_enabled    = false,
  .rx_buffer_full   = false
};

static struct spislave_priv_s beken_spi_slave1_priv =
{
  .ctrlr            =
                     {
                       .ops = &beken_spislave_ops
                     },
  .dev              = NULL,
  .config           = &beken_spi_slave1_cfg,
  .cs_pin_num       = GPIO_3,
  .refs             = 0,
  .tx_length        = 0,
  .tx_send_length   = 0,
  .tx_buffer        =
                     {
                       0
                     },
  .rx_length        = 0,
  .rx_buffer        =
                     {
                       0
                     },
  .is_processing    = false,
  .is_tx_enabled    = false,
  .rx_buffer_full   = false
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_rx_interrupt_callback
 *
 * Description:
 *   Handler for the RX fifo interrupt.
 *
 * Input Parameters:
 *   id      - SPI Id.
 *   param   - The args of callback function.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_rx_interrupt_callback(spi_id_t id, void *param)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)param;

  /* Update the rx_length */
  priv->rx_length++;
  if(priv->rx_length == SPI_SLAVE_BUFSIZE)
    {
      priv->rx_buffer_full = true;
      bk_spi_clr_rx(priv->config->spi_id);
      spierr("The spi rx buffer has reach the rx max bytes, Please Retrieves the"
            "data in the rx buffer, and call bk_spi_write_bytes_async again. \r\n");
    }
}

/****************************************************************************
 * Name: beken_rx_finish_interrupt_callback
 *
 * Description:
 *   Handler for the RX finish interrupt.
 *
 * Input Parameters:
 *   id      - SPI Id.
 *   param   - The args of callback function.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_rx_finish_interrupt_callback(spi_id_t id, void *param)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)param;

  /* If the rx buffer is not full, continue to receive datas*/
  if(priv->rx_length != SPI_SLAVE_BUFSIZE)
    {
      bk_spi_read_bytes_async(priv->config->spi_id, priv->rx_buffer, SPI_SLAVE_BUFSIZE);
    }

  spierr("The spi rx finish interrupt is trigger, Please check if the rx buffer has valid buffer,"
           "if not ,you need call bk_spi_write_bytes_async again. \r\n");
}

/****************************************************************************
 * Name: beken_tx_finish_interrupt_callback
 *
 * Description:
 *   Handler for the TX finish interrupt.
 *
 * Input Parameters:
 *   id      - SPI Id.
 *   param   - The args of callback function.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_tx_finish_interrupt_callback(spi_id_t id, void *param)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)param;

  if (!priv->is_processing)
    {
      SPIS_DEV_SELECT(priv->dev, true);
      priv->is_processing = true;
    }

  /* TX process */

  if (priv->is_tx_enabled)
    {
      /* Update the tx_length.If has the new datas,move new datas from back to front */
      beken_spislave_evict_sent_data(priv, priv->tx_send_length);
    }

  /* Send the new data  */
  beken_spislave_prepare_next_tx(priv);

  if (priv->is_processing)
    {
      priv->is_processing = false;
      SPIS_DEV_SELECT(priv->dev, false);
    }
}

/****************************************************************************
 * Name: beken_spislave_setmode
 *
 * Description:
 *   Set the SPI slave mode.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance.
 *   mode - The requested SPI mode
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spislave_setmode(struct spi_slave_ctrlr_s *ctrlr,
                                             enum spi_mode_e mode)
{
  DEBUGASSERT(ctrlr != NULL);
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;

  /* Has the mode changed? */
  switch (mode)
    {
      case SPI_POL_MODE_0:
        priv->config->spi_config.polarity = SPI_POLARITY_LOW;
        priv->config->spi_config.phase    = SPI_PHASE_1ST_EDGE;
        break;
      case SPI_POL_MODE_1:
        priv->config->spi_config.polarity = SPI_POLARITY_LOW;
        priv->config->spi_config.phase    = SPI_PHASE_2ND_EDGE;
        break;
      case SPI_POL_MODE_2:
        priv->config->spi_config.polarity = SPI_POLARITY_HIGH;
        priv->config->spi_config.phase    = SPI_PHASE_1ST_EDGE;
        break;
      case SPI_POL_MODE_3:
      default:
        priv->config->spi_config.polarity = SPI_POLARITY_HIGH;
        priv->config->spi_config.phase    = SPI_PHASE_2ND_EDGE;
        break;
  }

}

/****************************************************************************
 * Name: beken_spislave_setbits
 *
 * Description:
 *   Set the number of bits per word.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance.
 *   nbits - The number of bits in an SPI word.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spislave_setbits(struct spi_slave_ctrlr_s *ctrlr, int nbits)
{
  DEBUGASSERT(ctrlr != NULL);
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;

  if (nbits == CONFIG_SPI_8_BITS_NUM_PER_WORD)
    {
      priv->config->spi_config.bit_width = SPI_BIT_WIDTH_8BITS;
    }
  else if (nbits == CONFIG_SPI_16_BITS_NUM_PER_WORD)
    {
      priv->config->spi_config.bit_width = SPI_BIT_WIDTH_16BITS;
    }
  else
    {
      spierr("The SPI not supports %d bits per word\n", nbits);
    }
}

/****************************************************************************
 * Name: beken_spislave_evict_sent_data
 *
 * Description:
 *   Evict from the TX buffer data sent on the latest transaction and update
 *   the length. This is a post transaction operation.
 *
 * Input Parameters:
 *   priv       - Private SPI Slave controller structure
 *   sent_bytes - Number of transmitted bytes
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spislave_evict_sent_data(struct spislave_priv_s *priv,
                                                       uint32_t sent_bytes)
{
  if (sent_bytes < priv->tx_length)
    {
      priv->tx_length -= sent_bytes;

      memmove(priv->tx_buffer, priv->tx_buffer + sent_bytes,
              priv->tx_length);

      memset(priv->tx_buffer + priv->tx_length, 0, sent_bytes);
    }
  else
    {
      priv->tx_length = 0;
    }
}

/****************************************************************************
 * Name: beken_spislave_prepare_next_tx
 *
 * Description:
 *   Prepare the SPI Slave controller for transmitting data on the next
 *   transaction.
 *
 * Input Parameters:
 *   priv   - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spislave_prepare_next_tx(struct spislave_priv_s *priv)
{
  if (priv->tx_length != 0)
    {
      bk_spi_write_bytes_async(priv->config->spi_id, priv->tx_buffer, priv->tx_length);
      priv->is_tx_enabled = true;
      priv->tx_send_length = priv->tx_length;
    }
  else
    {
      spiwarn("TX buffer empty! Disabling TX for next transaction\n");

      priv->is_tx_enabled = false;
    }
}

/****************************************************************************
 * Name: beken_spislave_initialize
 *
 * Description:
 *   Initialize Beken SPI Slave hardware interface.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spislave_initialize(struct spi_slave_ctrlr_s *ctrlr)
{
  DEBUGASSERT(ctrlr != NULL);

  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  const struct beken_config_s *config = priv->config;

  spiinfo("ctrlr=%p\n", ctrlr);

  if (bk_spi_init(config->spi_id,&(config->spi_config)) != OK)
    {
        spierr("bk_spi_init failed \n");
    }
}

/****************************************************************************
 * Name: beken_spislave_deinitialize
 *
 * Description:
 *   Deinitialize Beken SPI Slave hardware interface.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spislave_deinitialize(struct spi_slave_ctrlr_s *ctrlr)
{
  DEBUGASSERT(ctrlr != NULL);

  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;

  if (bk_spi_deinit(priv->config->spi_id) != OK)
    {
        spierr("bk_spi_deinit failed \n");
    }
}

/****************************************************************************
 * Name: beken_spislave_bind
 *
 * Description:
 *   Bind the SPI Slave device interface to the SPI Slave controller
 *   interface and configure the SPI interface. Upon return, the SPI
 *   slave controller driver is fully operational and ready to perform
 *   transfers.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *   dev   - SPI Slave device interface instance
 *   mode  - The SPI mode requested
 *   nbits - The number of bits requests.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spislave_bind(struct spi_slave_ctrlr_s *ctrlr,
                                        struct spi_slave_dev_s *dev,
                                        enum spi_slave_mode_e mode,
                                        int nbits)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  const void *data = NULL;
  irqstate_t flags;
  size_t num_words;

  spiinfo("ctrlr=%p dev=%p mode=%d nbits=%d\n", ctrlr, dev, mode, nbits);

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev == NULL);
  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(nbits > 0);

  flags = enter_critical_section();

  priv->dev            = dev;
  priv->tx_length      = 0;
  priv->tx_send_length = 0;
  priv->rx_length      = 0;

  memset(priv->tx_buffer, 0, SPI_SLAVE_BUFSIZE);
  memset(priv->rx_buffer, 0, SPI_SLAVE_BUFSIZE);

  priv->is_processing  = false;
  priv->is_tx_enabled  = false;
  priv->rx_buffer_full = false;

  beken_spislave_setbits((struct spi_slave_ctrlr_s *)priv, nbits);
  beken_spislave_setmode((struct spi_slave_ctrlr_s *)priv, mode);

  beken_spislave_initialize((struct spi_slave_ctrlr_s *)priv);

  /*Set the rx byte number that trigger interrupt */
  bk_spi_read_bytes_async(priv->config->spi_id, priv->rx_buffer, SPI_SLAVE_BUFSIZE);

  /* Attach IRQ for RX interrupt */
  bk_spi_register_rx_isr(priv->config->spi_id, beken_rx_interrupt_callback, (void *)priv);
  bk_spi_register_rx_finish_isr(priv->config->spi_id, beken_rx_finish_interrupt_callback, (void *)priv);
  bk_spi_register_tx_finish_isr(priv->config->spi_id, beken_tx_finish_interrupt_callback, (void *)priv);

  SPIS_DEV_SELECT(dev, false);

  SPIS_DEV_CMDDATA(dev, false);

  num_words = SPIS_DEV_GETDATA(dev, &data);

  if (data != NULL && num_words > 0)
    {
      size_t num_bytes = WORDS2BYTES(priv, num_words);
      memcpy(priv->tx_buffer, data, num_bytes);
      priv->tx_length += num_bytes;
    }

  leave_critical_section(flags);

}

/****************************************************************************
 * Name: beken_spislave_unbind
 *
 * Description:
 *   Un-bind the SPI Slave device interface from the SPI Slave controller
 *   interface. Reset the SPI interface and restore the SPI Slave
 *   controller driver to its initial state.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spislave_unbind(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  spiinfo("Unbinding %p\n", priv->dev);

  flags = enter_critical_section();

  priv->dev = NULL;

  bk_spi_clr_rx(priv->config->spi_id);
  bk_spi_clr_tx(priv->config->spi_id);

  beken_spislave_deinitialize(ctrlr);

  leave_critical_section(flags);

  bk_spi_unregister_rx_isr(priv->config->spi_id);
  bk_spi_unregister_rx_finish_isr(priv->config->spi_id);
  bk_spi_unregister_tx_finish_isr(priv->config->spi_id);

}

/****************************************************************************
 * Name: beken_spislave_enqueue
 *
 * Description:
 *   Enqueue the next value to be shifted out from the interface. This adds
 *   the word to the controller driver for a subsequent transfer but has no
 *   effect on any in-process or currently "committed" transfers.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *   data  - Pointer to the command/data mode data to be shifted out.
 *           The data width must be aligned to the nbits parameter which was
 *           previously provided to the bind() method.
 *   len   - Number of units of "nbits" wide to enqueue,
 *           "nbits" being the data width previously provided to the bind()
 *           method.
 *
 * Returned Value:
 *   Number of data items successfully queued, or a negated errno:
 *         - "len" if all the data was successfully queued
 *         - "0..len-1" if queue is full
 *         - "-errno" in any other error
 *
 ****************************************************************************/

static int beken_spislave_enqueue(struct spi_slave_ctrlr_s *ctrlr,
                            const void *data,
                            size_t len)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  size_t num_bytes = WORDS2BYTES(priv, len);
  size_t bufsize;
  irqstate_t flags;
  int enqueued_words;

  spiinfo("ctrlr=%p, data=%p, num_bytes=%zu\n", ctrlr, data, num_bytes);

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  flags = enter_critical_section();

  bufsize = SPI_SLAVE_BUFSIZE - priv->tx_length;
  if (bufsize == 0)
    {
      leave_critical_section(flags);
      return -ENOSPC;
    }

  num_bytes = MIN(num_bytes, bufsize);
  memcpy(priv->tx_buffer + priv->tx_length, data, num_bytes);
  priv->tx_length += num_bytes;

  enqueued_words = BYTES2WORDS(priv, num_bytes);

  if (!priv->is_processing)
    {
      beken_spislave_prepare_next_tx(priv);
    }

  leave_critical_section(flags);

  return enqueued_words;
}

/****************************************************************************
 * Name: beken_spislave_qfull
 *
 * Description:
 *   Return true if the queue is full or false if there is space to add an
 *   additional word to the queue.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   true if the output queue is full, false otherwise.
 *
 ****************************************************************************/

static bool beken_spislave_qfull(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;
  bool is_full = false;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  spiinfo("ctrlr=%p\n", ctrlr);

  flags = enter_critical_section();
  is_full = priv->tx_length == SPI_SLAVE_BUFSIZE;
  leave_critical_section(flags);

  return is_full;
}

/****************************************************************************
 * Name: beken_spislave_qflush
 *
 * Description:
 *   Discard all saved values in the output queue. On return from this
 *   function the output queue will be empty.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spislave_qflush(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  spiinfo("ctrlr=%p\n", ctrlr);

  flags = enter_critical_section();

  priv->tx_length      = 0;
  priv->is_tx_enabled  = false;
  priv->tx_send_length = 0;

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: beken_spislave_qpoll
 *
 * Description:
 *   Tell the controller to output all the receive queue data.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   Number of units of width "nbits" left in the RX queue. If the device
 *   accepted all the data, the return value will be 0.
 *
 ****************************************************************************/

static size_t beken_spislave_qpoll(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;
  uint32_t tmp;
  uint32_t recv_n;
  size_t remaining_words;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  spiinfo("ctrlr=%p\n", ctrlr);

  flags = enter_critical_section();

  tmp = SPIS_DEV_RECEIVE(priv->dev, priv->rx_buffer,
                         BYTES2WORDS(priv, priv->rx_length));
  recv_n = WORDS2BYTES(priv, tmp);

  if (recv_n < priv->rx_length)
    {
      /* If the upper layer does not receive all of the data from the receive
       * buffer, move the remaining data to the head of the buffer.
       */

      priv->rx_length -= recv_n;
      memmove(priv->rx_buffer, priv->rx_buffer + recv_n, priv->rx_length);
    }
  else
    {
      priv->rx_length = 0;
    }

  remaining_words = BYTES2WORDS(priv, priv->rx_length);

  if ((priv->rx_buffer_full == true) && (priv->rx_length == 0))
    {
      bk_spi_read_bytes_async(priv->config->spi_id, priv->rx_buffer, SPI_SLAVE_BUFSIZE);
      priv->rx_buffer_full = false;
    }

  leave_critical_section(flags);

  return remaining_words;
}

/****************************************************************************
 * Name: beken_spislave_ctrlr_initialize
 *
 * Description:
 *   Initialize the selected SPI Slave bus.
 *
 * Input Parameters:
 *   port - Port number (for hardware that has multiple SPI Slave interfaces)
 *
 * Returned Value:
 *   Valid SPI Slave controller structure reference on success;
 *   NULL on failure.
 *
 ****************************************************************************/

struct spi_slave_ctrlr_s *beken_spislave_ctrlr_initialize(int port)
{
  struct spi_slave_ctrlr_s *spislave_dev;
  struct spislave_priv_s *priv;
  irqstate_t flags;

  switch (port)
    {
      case SPI_ID_0:
        priv = &beken_spi_slave0_priv;
        break;
      case SPI_ID_1:
        priv = &beken_spi_slave1_priv;
        break;
      default:
        return NULL;
    }

  spislave_dev = (struct spi_slave_ctrlr_s *)priv;

  flags = enter_critical_section();

  if ((volatile int)priv->refs != 0)
    {
      leave_critical_section(flags);
      return spislave_dev;
    }

  priv->refs++;

  leave_critical_section(flags);

  return spislave_dev;
}

/****************************************************************************
 * Name: beken_spislave_ctrlr_uninitialize
 *
 * Description:
 *   Uninitialize an SPI Slave bus.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   Zero (OK) is returned on success. Otherwise -1 (ERROR).
 *
 ****************************************************************************/

int beken_spislave_ctrlr_uninitialize(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;

  DEBUGASSERT(ctrlr != NULL);

  if (priv->refs == 0)
    {
      return ERROR;
    }

  flags = enter_critical_section();

  if (--priv->refs)
    {
      leave_critical_section(flags);
      return OK;
    }

  leave_critical_section(flags);

  return OK;
}

#endif /* defined (CONFIG_SPI_SLAVE) */
