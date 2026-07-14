/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_spi.c
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

#include <assert.h>
#include <debug.h>
#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/mutex.h>
#include <nuttx/spi/spi.h>

#include <arch/board/board.h>

#include <common/bk_err.h>
#include <driver/spi.h>
#include <common/bk_include.h>
#include <driver/gpio_types.h>
#include <driver/gpio.h>

extern bk_err_t gpio_dev_unmap(gpio_id_t gpio_id);

#ifdef CONFIG_SPI_DRIVER
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The max frequency that the spi supports */
#define CONFIG_SPI_MAX_BAUD_RATE          49000000
#define CONFIG_SPI_8_BITS_NUM_PER_WORD    8
#define CONFIG_SPI_16_BITS_NUM_PER_WORD   16
#define CONFIG_SPI_DEFAULT_FREQUENCY      10000000

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* SPI Device hardware configuration */
struct beken_config_s
{
  spi_config_t          spi_config;
  spi_id_t              spi_id;
  gpio_id_t             spi_cs_pin; /* CS pin */
  mutex_t               lock;       /* Held while chip is selected for mutual exclusion */
};

struct beken_spi_priv_s
{
  /* Externally visible part of the SPI interface */

  struct spi_dev_s      spi_dev;

  /* Port configuration */
  struct beken_config_s *beken_spi_cfg;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int beken_spi_lock(struct spi_dev_s *dev, bool lock);
static void beken_spi_select(struct spi_dev_s *dev,
                                    uint32_t devid, bool selected);
static uint32_t beken_spi_setfrequency(struct spi_dev_s *dev,
                                                 uint32_t frequency);
static void beken_spi_setmode(struct spi_dev_s *dev,
                                     enum spi_mode_e mode);
static void beken_spi_setbits(struct spi_dev_s *dev, int nbits);
#ifdef CONFIG_SPI_HWFEATURES
static int beken_spi_hwfeatures(struct spi_dev_s *dev,
                                        spi_hwfeatures_t features);
#endif
static uint8_t beken_spi_status(struct spi_dev_s *dev, uint32_t devid);
static uint32_t beken_spi_send(struct spi_dev_s *dev, uint32_t wd);
static void beken_spi_exchange(struct spi_dev_s *dev,
                                      const void *txbuffer,
                                      void *rxbuffer, size_t nwords);
#ifndef CONFIG_SPI_EXCHANGE
static void beken_spi_sndblock(struct spi_dev_s *dev,
                                       const void *txbuffer,
                                       size_t nwords);
static void beken_spi_recvblock(struct spi_dev_s *dev,
                                        void *rxbuffer,
                                         size_t nwords);
#endif

#ifdef CONFIG_SPI_TRIGGER
static int beken_spi_trigger(struct spi_dev_s *dev);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct beken_config_s beken_spi_0_cfg =
{
  .spi_config =
    {
      .role          = SPI_ROLE_MASTER,
      .bit_width     = SPI_BIT_WIDTH_8BITS,
      .polarity      = SPI_POLARITY_LOW,
      .phase         = SPI_PHASE_1ST_EDGE,
      .wire_mode     = SPI_4WIRE_MODE,
      .baud_rate     = CONFIG_SPI_DEFAULT_FREQUENCY,
      .bit_order     = SPI_MSB_FIRST,
    },
  .spi_id            = SPI_ID_0,
  .spi_cs_pin        = GPIO_15,
  .lock              = NXMUTEX_INITIALIZER,
};

static struct beken_config_s beken_spi_1_cfg =
{
  .spi_config =
    {
      .role          = SPI_ROLE_MASTER,
      .bit_width     = SPI_BIT_WIDTH_8BITS,
      .polarity      = SPI_POLARITY_LOW,
      .phase         = SPI_PHASE_1ST_EDGE,
      .wire_mode     = SPI_4WIRE_MODE,
      .baud_rate     = CONFIG_SPI_DEFAULT_FREQUENCY,
      .bit_order     = SPI_MSB_FIRST,
    },
  .spi_id            = SPI_ID_1,
  .spi_cs_pin        = GPIO_3,
  .lock              = NXMUTEX_INITIALIZER,
};

static const struct spi_ops_s beken_spi_ops =
{
  .lock              = beken_spi_lock,
  .select            = beken_spi_select,

  .setfrequency      = beken_spi_setfrequency,
  .setmode           = beken_spi_setmode,
  .setbits           = beken_spi_setbits,
#ifdef CONFIG_SPI_HWFEATURES
  .hwfeatures        = beken_spi_hwfeatures,
#endif
  .status            = beken_spi_status,
  .send              = beken_spi_send,
#ifdef CONFIG_SPI_EXCHANGE
  .exchange          = beken_spi_exchange,
#else
  .sndblock          = beken_spi_sndblock,
  .recvblock         = beken_spi_recvblock,
#endif
#ifdef CONFIG_SPI_TRIGGER
  .trigger           = beken_spi_trigger,
#endif
  .registercallback  = NULL,
};

static struct beken_spi_priv_s beken_spi0_priv =
{
  .spi_dev           =
  {
    .ops             = &beken_spi_ops
  },
  .beken_spi_cfg     = &beken_spi_0_cfg,
};

static struct beken_spi_priv_s beken_spi1_priv =
{
  .spi_dev           =
  {
    .ops             = &beken_spi_ops
  },
  .beken_spi_cfg     = &beken_spi_1_cfg,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_spi_lock
 *
 * Description:
 *   Lock or unlock the SPI device.
 *
 * Input Parameters:
 *   dev    - Device-specific state data
 *   lock   - true: Lock SPI bus, false: unlock SPI bus
 *
 * Returned Value:
 *   The result of lock or unlock the SPI device.
 *
 ****************************************************************************/

static int beken_spi_lock(struct spi_dev_s *dev, bool lock)
{
  int ret;

  DEBUGASSERT(dev != NULL);
  struct beken_spi_priv_s *priv = (struct beken_spi_priv_s *)dev;

  if (lock)
    {
      ret = nxmutex_lock(&priv->beken_spi_cfg->lock);
    }
  else
    {
      ret = nxmutex_unlock(&priv->beken_spi_cfg->lock);
    }

  return ret;
}

/****************************************************************************
 * Name: beken_spi_select
 *
 * Description:
 *   Enable/disable the SPI chip select.
 *
 * Input Parameters:
 *   dev      - Device-specific state data
 *   devid    - Identifies the device to select
 *   selected - true: slave selected, false: slave de-selected
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spi_select(struct spi_dev_s *dev,
                                    uint32_t devid, bool selected)
{
  DEBUGASSERT(dev != NULL);
  struct beken_spi_priv_s *priv = (struct beken_spi_priv_s *)dev;

  if (selected)
    {
      bk_gpio_set_output_low(priv->beken_spi_cfg->spi_cs_pin);
    }
  else
    {
      bk_gpio_set_output_high(priv->beken_spi_cfg->spi_cs_pin);
    }
}

/****************************************************************************
 * Name: beken_spi_setfrequency
 *
 * Description:
 *   Set the SPI frequency.
 *
 * Input Parameters:
 *   dev       - Device-specific state data
 *   frequency - The requested SPI frequency
 *
 * Returned Value:
 *   Returns the current selected frequency.
 *
 * Attention:The maximum frequency supported by SPI is 49000000. If the frequency
 *           is greater than 49000000, the SPI operates at 49000000.
 *
 ****************************************************************************/

static uint32_t beken_spi_setfrequency(struct spi_dev_s *dev,
                                                uint32_t frequency)
{
  DEBUGASSERT(dev != NULL);
  struct beken_spi_priv_s *priv = (struct beken_spi_priv_s *)dev;

  if (CONFIG_SPI_MAX_BAUD_RATE < frequency)
    {
      /* Requested frequency is the same as the current frequency. */
      priv->beken_spi_cfg->spi_config.baud_rate = CONFIG_SPI_MAX_BAUD_RATE;
      spiwarn("The max frequency that the spi supports is %ld \n ",
              priv->beken_spi_cfg->spi_config.baud_rate);
    }
  else
    {
      priv->beken_spi_cfg->spi_config.baud_rate = frequency;
    }

  return priv->beken_spi_cfg->spi_config.baud_rate;
}

/****************************************************************************
 * Name: beken_spi_setmode
 *
 * Description:
 *   Set the SPI mode.
 *
 * Input Parameters:
 *   dev  - Device-specific state data
 *   mode - The requested SPI mode
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spi_setmode(struct spi_dev_s *dev,
                                     enum spi_mode_e mode)
{
  DEBUGASSERT(dev != NULL);
  struct beken_spi_priv_s *priv = (struct beken_spi_priv_s *)dev;

  /* Has the mode changed? */
  switch (mode)
    {
      case SPI_POL_MODE_0:
        priv->beken_spi_cfg->spi_config.polarity = SPI_POLARITY_LOW;
        priv->beken_spi_cfg->spi_config.phase    = SPI_PHASE_1ST_EDGE;
        break;
      case SPI_POL_MODE_1:
        priv->beken_spi_cfg->spi_config.polarity = SPI_POLARITY_LOW;
        priv->beken_spi_cfg->spi_config.phase    = SPI_PHASE_2ND_EDGE;
        break;
      case SPI_POL_MODE_2:
        priv->beken_spi_cfg->spi_config.polarity = SPI_POLARITY_HIGH;
        priv->beken_spi_cfg->spi_config.phase    = SPI_PHASE_1ST_EDGE;
        break;
      case SPI_POL_MODE_3:
      default:
        priv->beken_spi_cfg->spi_config.polarity = SPI_POLARITY_HIGH;
        priv->beken_spi_cfg->spi_config.phase    = SPI_PHASE_2ND_EDGE;
        break;
  }

}

/****************************************************************************
 * Name: beken_spi_setbits
 *
 * Description:
 *   Set the number of bits per word.
 *
 * Input Parameters:
 *   dev   - Device-specific state data
 *   nbits - The number of bits in an SPI word.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spi_setbits(struct spi_dev_s *dev, int nbits)
{
  DEBUGASSERT(dev != NULL);
  struct beken_spi_priv_s *priv = (struct beken_spi_priv_s *)dev;

  if (nbits == CONFIG_SPI_8_BITS_NUM_PER_WORD)
    {
      priv->beken_spi_cfg->spi_config.bit_width = SPI_BIT_WIDTH_8BITS;
    }
  else if (nbits == CONFIG_SPI_16_BITS_NUM_PER_WORD)
    {
      priv->beken_spi_cfg->spi_config.bit_width = SPI_BIT_WIDTH_16BITS;
    }
  else
    {
      spierr("The SPI not supports %d bits per word\n", nbits);
    }
}

/****************************************************************************
 * Name: beken_spi_hwfeatures
 *
 * Description:
 *   Set hardware-specific feature flags.
 *
 * Input Parameters:
 *   dev      - Device-specific state data
 *   features - H/W feature flags
 *
 * Returned Value:
 *   Zero (OK) if the selected H/W features are enabled; A negated errno
 *   value if any H/W feature is not supportable.
 *
 ****************************************************************************/

#ifdef CONFIG_SPI_HWFEATURES
static int beken_spi_hwfeatures(struct spi_dev_s *dev,
                                        spi_hwfeatures_t features)
{

#ifdef CONFIG_SPI_BITORDER
  DEBUGASSERT(dev != NULL);
  struct beken_spi_priv_s *priv = (struct beken_spi_priv_s *)dev;

  /* Transfer data LSB first */
  if ((features & HWFEAT_LSBFIRST) != 0)
    {
      priv->beken_spi_cfg->spi_config.bit_order = SPI_LSB_FIRST;
    }
  else if ((features & HWFEAT_LSBFIRST) == 0)
    {
      priv->beken_spi_cfg->spi_config.bit_order = SPI_MSB_FIRST;
    }

  features &= ~HWFEAT_LSBFIRST;
#endif

  return (features == 0) ? OK : -ENOSYS;
}
#endif

/****************************************************************************
 * Name: beken_spi_status
 *
 * Description:
 *   Get SPI/MMC status.  Optional.
 *
 * Input Parameters:
 *   dev   - Device-specific state data
 *   devid - Identifies the device to report status on
 *
 * Returned Value:
 *   Returns a bitset of status values (see SPI_STATUS_* defines)
 *
 ****************************************************************************/

static uint8_t beken_spi_status(struct spi_dev_s *dev, uint32_t devid)
{
  uint8_t status = 0;

  return status;
}

/****************************************************************************
 * Name: beken_spi_send
 *
 * Description:
 *   Send one word on SPI.
 *
 * Input Parameters:
 *   dev - Device-specific state data
 *   wd  - The word to send. The size of the data is determined by the
 *         number of bits selected for the SPI interface.
 *
 * Returned Value:
 *   Received value.
 *
 ****************************************************************************/

static uint32_t beken_spi_send(struct spi_dev_s *dev, uint32_t wd)
{
  DEBUGASSERT(dev != NULL);
  struct beken_spi_priv_s *priv = (struct beken_spi_priv_s *)dev;

  uint8_t  write_word_8bits;
  uint16_t write_word_16bits;

  if (priv->beken_spi_cfg->spi_config.bit_width == SPI_BIT_WIDTH_8BITS)
    {
      write_word_8bits = wd & 0xFF;

      bk_spi_write_bytes(priv->beken_spi_cfg->spi_id,&write_word_8bits,sizeof(write_word_8bits));

      return (uint32_t) write_word_8bits;
    }
  else if (priv->beken_spi_cfg->spi_config.bit_width == SPI_BIT_WIDTH_16BITS)
    {
      write_word_16bits = wd & 0xFFFF;

      bk_spi_write_bytes(priv->beken_spi_cfg->spi_id,&write_word_16bits,sizeof(write_word_16bits));

      return (uint32_t) write_word_16bits;
    }
  else
    {
      return 0xDEADBEEF;
    }

}

/****************************************************************************
 * Name: beken_spi_exchange
 *
 * Description:
 *   Exchange a block of data from SPI.
 *
 * Input Parameters:
 *   dev      - Device-specific state data
 *   txbuffer - A pointer to the buffer of data to be sent
 *   rxbuffer - A pointer to the buffer in which to receive data
 *   nwords   - The length of data that to be exchanged in units of words.
 *              The wordsize is determined by the number of bits-per-word
 *              selected for the SPI interface. If nbits <= 8, the data is
 *              packed into uint8_t's; if nbits >8, the data is packed into
 *              uint16_t's
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spi_exchange(struct spi_dev_s *dev,
                                       const void *txbuffer,
                                       void *rxbuffer,
                                       size_t nwords)
{
  DEBUGASSERT(dev != NULL);
  struct beken_spi_priv_s *priv = (struct beken_spi_priv_s *)dev;
  uint32_t word_to_bytes_number = 0;

  if (priv->beken_spi_cfg->spi_config.bit_width == SPI_BIT_WIDTH_8BITS)
    {
      word_to_bytes_number = nwords;
    }
  else if (priv->beken_spi_cfg->spi_config.bit_width == SPI_BIT_WIDTH_16BITS)
    {
      word_to_bytes_number = nwords * 2;
    }

  bk_spi_transmit(priv->beken_spi_cfg->spi_id, txbuffer, word_to_bytes_number,
                  rxbuffer, word_to_bytes_number);

}

#ifndef CONFIG_SPI_EXCHANGE

/****************************************************************************
 * Name: beken_spi_sndblock
 *
 * Description:
 *   Send a block of data on SPI.
 *
 * Input Parameters:
 *   dev      - Device-specific state data
 *   txbuffer - A pointer to the buffer of data to be sent
 *   nwords   - The length of data to send from the buffer in number of
 *              words. The wordsize is determined by the number of
 *              bits-per-word selected for the SPI interface.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spi_sndblock(struct spi_dev_s *dev,
                                      const void *txbuffer,
                                      size_t nwords)
{
  beken_spi_exchange(dev, txbuffer, NULL, nwords);
}

/****************************************************************************
 * Name: beken_spi_recvblock
 *
 * Description:
 *   Receive a block of data from SPI.
 *
 * Input Parameters:
 *   dev      - Device-specific state data
 *   rxbuffer - A pointer to the buffer in which to receive data
 *   nwords   - The length of data that can be received in the buffer in
 *              number of words. The wordsize is determined by the number of
 *              bits-per-word selected for the SPI interface.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void beken_spi_recvblock(struct spi_dev_s *dev,
                                        void *rxbuffer,
                                        size_t nwords)
{
  beken_spi_exchange(dev, NULL, rxbuffer, nwords);
}
#endif

/****************************************************************************
 * Name: beken_spi_trigger
 *
 * Description:
 *   Trigger a previously configured DMA transfer.
 *
 * Input Parameters:
 *   dev      - Device-specific state data
 *
 * Returned Value:
 *   OK       - Trigger was fired
 *   -ENOSYS  - Trigger not fired due to lack of DMA or low level support
 *   -EIO     - Trigger not fired because not previously primed
 *
 ****************************************************************************/

#ifdef CONFIG_SPI_TRIGGER
static int beken_spi_trigger(struct spi_dev_s *dev)
{
  return -ENOSYS;
}
#endif

/****************************************************************************
 * Name: beken_spibus_initialize
 *
 * Description:
 *   Initialize the selected SPI bus.
 *
 * Input Parameters:
 *   port     - Port number (for hardware that has multiple SPI interfaces)
 *
 * Returned Value:
 *   Valid SPI device structure reference on success; NULL on failure.
 *
 ****************************************************************************/

struct spi_dev_s *beken_spibus_initialize(spi_id_t port)
{
  struct spi_dev_s *spi_dev;
  struct beken_spi_priv_s *priv;
  int rtn_value;
  gpio_config_t cs_pin_config = {0};

  switch (port)
    {
      case SPI_ID_0:
        priv = &beken_spi0_priv;
        break;
      case SPI_ID_1:
        priv = &beken_spi1_priv;
        break;
      default:
        return NULL;
    }

  spi_dev = (struct spi_dev_s *)priv;

  nxmutex_lock(&priv->beken_spi_cfg->lock);

  rtn_value = bk_spi_init(priv->beken_spi_cfg->spi_id,&priv->beken_spi_cfg->spi_config);

  if (rtn_value != OK)
    {
      spi_dev = NULL;
      nxmutex_unlock(&priv->beken_spi_cfg->lock);

      return spi_dev;
    }

  cs_pin_config.func_mode = GPIO_SECOND_FUNC_DISABLE;
  cs_pin_config.io_mode   = GPIO_OUTPUT_ENABLE;
  cs_pin_config.pull_mode = GPIO_PULL_DISABLE;

  if ((gpio_dev_unmap(priv->beken_spi_cfg->spi_cs_pin) != OK) ||
      (bk_gpio_set_config(priv->beken_spi_cfg->spi_cs_pin,
                          &cs_pin_config) != OK))
    {
      spi_dev = NULL;
      bk_spi_deinit(priv->beken_spi_cfg->spi_id);
    }

  nxmutex_unlock(&priv->beken_spi_cfg->lock);

  return spi_dev;
}

/****************************************************************************
 * Name: beken_spibus_uninitialize
 *
 * Description:
 *   Uninitialize an SPI bus.
 *
 * Input Parameters:
 *   dev      - Device-specific state data
 *
 * Returned Value:
 *   Zero (OK) is returned on success. Otherwise -1 (ERROR).
 *
 ****************************************************************************/

int beken_spibus_uninitialize(struct spi_dev_s *dev)
{
  struct beken_spi_priv_s *priv = (struct beken_spi_priv_s *)dev;
  int rtn_value;

  nxmutex_lock(&priv->beken_spi_cfg->lock);

  rtn_value = bk_spi_deinit(priv->beken_spi_cfg->spi_id);

  nxmutex_unlock(&priv->beken_spi_cfg->lock);

  return rtn_value;
}

#endif
