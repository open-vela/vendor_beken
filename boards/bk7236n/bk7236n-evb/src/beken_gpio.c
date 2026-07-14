/****************************************************************************
 * vendor/beken/boards/bk7236n/bk7236n-evb/src/beken_gpio.c
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
#include <assert.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <arch/irq.h>

#include <nuttx/ioexpander/gpio.h>
#include <arch/board/board.h>

#include <common/bk_err.h>
#include <driver/gpio.h>

extern bk_err_t gpio_dev_unmap(gpio_id_t gpio_id);

#if defined(CONFIG_DEV_GPIO) && !defined(CONFIG_GPIO_LOWER_HALF)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct beken_gpio_cfg_s
{
  struct gpio_dev_s        gpio;
  gpio_id_t                gpio_id;
  gpio_config_t            gpio_config;
  gpio_driver_capacity_t   capacity;        /*Driver capacity*/
  uint32_t                 virtual_gpio_id; /*The logical serial number of GPIO */
};

struct beken_gpio_dev_s
{
  struct beken_gpio_cfg_s beken_gpio;
  pin_interrupt_t callback;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int gpio_read(struct gpio_dev_s *dev, bool *value);
static int gpio_write(struct gpio_dev_s *dev, bool value);
static int gpio_attach(struct gpio_dev_s *dev,
                            pin_interrupt_t callback);
static int gpio_enable(struct gpio_dev_s *dev, bool enable);
static int gpio_setpintype(struct gpio_dev_s *dev,
                                 enum gpio_pintype_e pintype);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct gpio_operations_s gpio_ops =
{
  .go_read       = gpio_read,
  .go_write      = gpio_write,
  .go_attach     = gpio_attach,
  .go_enable     = gpio_enable,
  .go_setpintype = gpio_setpintype,
};

static struct beken_gpio_dev_s gpio_devs[] =
{
  {
    .beken_gpio       =
    {
      .gpio           =
        {
          .gp_pintype = GPIO_INPUT_PIN,
          .gp_ops     = &gpio_ops,
        },
      .gpio_id        = GPIO_48,
      .gpio_config    = { GPIO_INPUT_ENABLE, GPIO_PULL_DOWN_EN, GPIO_SECOND_FUNC_DISABLE},
      .capacity       = 0,
    },
    .callback         = NULL,
  },

  {
    .beken_gpio       =
    {
      .gpio           =
        {
          .gp_pintype = GPIO_OUTPUT_PIN,
          .gp_ops     = &gpio_ops,
        },
      .gpio_id        = GPIO_28,
      .gpio_config    = { GPIO_OUTPUT_ENABLE, GPIO_PULL_DISABLE, GPIO_SECOND_FUNC_DISABLE},
      .capacity       = GPIO_DRIVER_CAPACITY_1,
    },
    .callback         = NULL,
  },

  {
    .beken_gpio       =
    {
      .gpio           =
        {
          .gp_pintype = GPIO_OUTPUT_PIN,
          .gp_ops     = &gpio_ops,
        },
      .gpio_id        = GPIO_32,
      .gpio_config    = { GPIO_OUTPUT_ENABLE, GPIO_PULL_DISABLE, GPIO_SECOND_FUNC_DISABLE},
      .capacity       = GPIO_DRIVER_CAPACITY_1,
    },
    .callback         = NULL,
  }
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_gpio_int_wrapper
 *
 * Description:
 *   The isr callback function of GPIO.
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

static void beken_gpio_int_wrapper(gpio_id_t gpio_id)
{
  for (uint32_t io_number = 0;
       io_number < (sizeof(gpio_devs) / sizeof(gpio_devs[0]));
       io_number++)
    {
      if (gpio_devs[io_number].beken_gpio.gpio_id == gpio_id)
        {
          if (gpio_devs[io_number].callback != NULL)
            {
              gpio_devs[io_number].callback(
                &gpio_devs[io_number].beken_gpio.gpio, gpio_id);
            }

          break;
        }
    }
}

/****************************************************************************
 * Name: beken_set_input_int_type
 *
 * Description:
 *   Set the gpio_id as the interrupt input IO.
 *
 * Input Parameters:
 *   gpio_id      - The GPIO id of the beken device.
 *   type         - The type of interrupt source.
 *
 * Returned Value:
 *   OK           : The function execute successfuly.
 *   Other value  : Some function execute failed.
 *
 ****************************************************************************/

static int beken_set_input_int_type(gpio_id_t gpio_id, gpio_int_type_t type)
{

  BK_RETURN_ON_ERR(bk_gpio_enable_input(gpio_id));
  BK_RETURN_ON_ERR(bk_gpio_set_interrupt_type(gpio_id, type));
  BK_RETURN_ON_ERR(bk_gpio_enable_interrupt(gpio_id));

  return OK;
}

/****************************************************************************
 * Name: gpio_read
 *
 * Description:
 *   Read the value of GPIO.
 *
 * Input Parameters:
 *   dev          - The struct of GPIO device.
 *   value        - Return the readed value.
 *
 * Returned Value:
 *   OK           : The function execute successfuly.
 *   Other value  : The input args are invalid.
 *
 ****************************************************************************/

static int gpio_read(struct gpio_dev_s *dev, bool *value)
{
  struct beken_gpio_dev_s *beken_io_dev = (struct beken_gpio_dev_s *)dev;

  DEBUGASSERT(beken_io_dev != NULL && value != NULL);
  gpioinfo("Reading pin %d...\n",beken_io_dev->beken_gpio.gpio_id);

  if (beken_io_dev->beken_gpio.gpio.gp_pintype == GPIO_OUTPUT_PIN  ||
     beken_io_dev->beken_gpio.gpio.gp_pintype == GPIO_OUTPUT_PIN_OPENDRAIN)
    {
      *value = bk_gpio_get_output(beken_io_dev->beken_gpio.gpio_id);
    }
  else
    {
      *value = bk_gpio_get_input(beken_io_dev->beken_gpio.gpio_id);
    }

  return OK;
}

/****************************************************************************
 * Name: gpio_write
 *
 * Description:
 *   Write the value of GPIO.
 *
 * Input Parameters:
 *   dev          - The struct of GPIO device.
 *   value        - The value to be written.
 *
 * Returned Value:
 *   OK           : The function execute successfuly.
 *   Other value  : The input args are invalid or some function execute failed.
 *
 ****************************************************************************/

static int gpio_write(struct gpio_dev_s *dev, bool value)
{
  struct beken_gpio_dev_s *beken_io_dev =
    (struct beken_gpio_dev_s *)dev;

  DEBUGASSERT(beken_io_dev != NULL);
  gpioinfo("Writing %d\n",(int)value);

  if (beken_io_dev->beken_gpio.gpio.gp_pintype == GPIO_OUTPUT_PIN  ||
     beken_io_dev->beken_gpio.gpio.gp_pintype == GPIO_OUTPUT_PIN_OPENDRAIN)
    {
      if (value)
        {
          BK_RETURN_ON_ERR(bk_gpio_set_output_high(beken_io_dev->beken_gpio.gpio_id));
        }
      else
        {
          BK_RETURN_ON_ERR(bk_gpio_set_output_low(beken_io_dev->beken_gpio.gpio_id));
        }
    }

  return OK;
}

/****************************************************************************
 * Name: gpio_attach
 *
 * Description:
 *   Attach the callback function to the corresponding GPIO.
 *
 * Input Parameters:
 *   dev          - The struct of GPIO device.
 *   callback     - The callback function to be attached.
 *
 * Returned Value:
 *   OK           : The function execute successfuly.
 *   -EACCES      : The gp_pintype is invalid.
 *   Other value  : Some function execute failed.
 *
 ****************************************************************************/

static int gpio_attach(struct gpio_dev_s *dev,
                        pin_interrupt_t callback)
{
  struct beken_gpio_dev_s *beken_io_dev =
    (struct beken_gpio_dev_s *)dev;

  gpioinfo("Attaching the callback virtual_id %ld \n",beken_io_dev->beken_gpio.virtual_gpio_id);

  if (beken_io_dev->beken_gpio.gpio.gp_pintype >= GPIO_INTERRUPT_PIN )
    {
      /* Make sure the interrupt is disabled */
      BK_RETURN_ON_ERR(bk_gpio_disable_interrupt(beken_io_dev->beken_gpio.gpio_id));
      BK_RETURN_ON_ERR(bk_gpio_register_isr(beken_io_dev->beken_gpio.gpio_id, beken_gpio_int_wrapper));

      gpio_devs[beken_io_dev->beken_gpio.virtual_gpio_id].callback = callback;
    }
  else
    {
      gpioerr( "ERROR: The gp_pintype %d is not be supported.\n", beken_io_dev->beken_gpio.gpio.gp_pintype);
      return -EACCES;
    }

  return OK;
}

/****************************************************************************
 * Name: gpio_enable
 *
 * Description:
 *   Enable or disable the interrupt of GPIO.
 *
 * Input Parameters:
 *   dev          - The struct of GPIO device.
 *   enable       - The flag of enable or disable interrupt.
 *
 * Returned Value:
 *   OK           : The function execute successfuly.
 *   -EACCES      : The gp_pintype is invalid.
 *   Other value  : Some function execute failed.
 *
 ****************************************************************************/

static int gpio_enable(struct gpio_dev_s *dev, bool enable)
{
  struct beken_gpio_dev_s *beken_io_dev = (struct beken_gpio_dev_s *)dev;

  if (beken_io_dev->beken_gpio.gpio.gp_pintype >= GPIO_INTERRUPT_PIN )
    {
      if (enable)
        {
          if (beken_io_dev->callback != NULL)
            {
              gpioinfo("Enabling the interrupt\n");
              /* Configure the interrupt */
              BK_RETURN_ON_ERR(bk_gpio_enable_interrupt(beken_io_dev->beken_gpio.gpio_id));
            }
        }
      else
        {
          gpioinfo("Disable the interrupt\n");
          BK_RETURN_ON_ERR(bk_gpio_disable_interrupt(beken_io_dev->beken_gpio.gpio_id));
        }
    }
  else
    {
      gpioerr( "ERROR: The gp_pintype %d is not be supported.\n", beken_io_dev->beken_gpio.gpio.gp_pintype);
      return -EACCES;
    }

  return OK;
}

/****************************************************************************
 * Name: gpio_setpintype
 *
 * Description:
 *   According to the pintype to config the GPIO.
 *
 * Input Parameters:
 *   dev          - The struct of GPIO device.
 *   pintype      - Pintype to be configured.
 *
 * Returned Value:
 *   OK           : The function execute successfuly.
 *   ERROR        : The pintype is invalid.
 *   Other value  : Some function execute failed.
 *
 ****************************************************************************/

static int gpio_setpintype(struct gpio_dev_s *dev,
                            enum gpio_pintype_e pintype)
{
  struct beken_gpio_dev_s *beken_io_dev = (struct beken_gpio_dev_s *)dev;

  DEBUGASSERT(beken_io_dev != NULL);

  beken_io_dev->beken_gpio.gpio.gp_pintype = pintype;
  BK_RETURN_ON_ERR(gpio_dev_unmap(beken_io_dev->beken_gpio.gpio_id));

  switch (pintype)
  {
  case GPIO_INPUT_PIN:
    BK_RETURN_ON_ERR(bk_gpio_enable_input(beken_io_dev->beken_gpio.gpio_id));
    break;
  case GPIO_INPUT_PIN_PULLUP:
    BK_RETURN_ON_ERR(bk_gpio_enable_input(beken_io_dev->beken_gpio.gpio_id));
    BK_RETURN_ON_ERR(bk_gpio_pull_up(beken_io_dev->beken_gpio.gpio_id));
    break;
  case GPIO_INPUT_PIN_PULLDOWN:
    BK_RETURN_ON_ERR(bk_gpio_enable_input(beken_io_dev->beken_gpio.gpio_id));
    BK_RETURN_ON_ERR(bk_gpio_pull_down(beken_io_dev->beken_gpio.gpio_id));
    break;
  case GPIO_OUTPUT_PIN:
    BK_RETURN_ON_ERR(bk_gpio_enable_output(beken_io_dev->beken_gpio.gpio_id));
    break;
  case GPIO_OUTPUT_PIN_OPENDRAIN:
    BK_RETURN_ON_ERR(bk_gpio_enable_output(beken_io_dev->beken_gpio.gpio_id));
    break;
  case GPIO_INTERRUPT_HIGH_PIN:
    BK_RETURN_ON_ERR(beken_set_input_int_type(beken_io_dev->beken_gpio.gpio_id, GPIO_INT_TYPE_HIGH_LEVEL));
    break;
  case GPIO_INTERRUPT_LOW_PIN:
    BK_RETURN_ON_ERR(beken_set_input_int_type(beken_io_dev->beken_gpio.gpio_id, GPIO_INT_TYPE_LOW_LEVEL));
    break;
   case GPIO_INTERRUPT_RISING_PIN:
    BK_RETURN_ON_ERR(beken_set_input_int_type(beken_io_dev->beken_gpio.gpio_id, GPIO_INT_TYPE_RISING_EDGE));
    break;
   case GPIO_INTERRUPT_FALLING_PIN:
    BK_RETURN_ON_ERR(beken_set_input_int_type(beken_io_dev->beken_gpio.gpio_id, GPIO_INT_TYPE_FALLING_EDGE));
    break;
  default:
    return ERROR;
    break;
  }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_gpio_init
 *
 * Description:
 *   Register the gpio.
 *
 * Input Parameters:
 *   NONE.
 *
 * Returned Value:
 *   OK           : The function execute successfuly.
 *   Other value  : Failed to initial or register gpio.
 *
 ****************************************************************************/

int beken_gpio_init(void)
{
  int rtn_value;

  for (uint32_t io_number = 0; io_number < (sizeof(gpio_devs)/sizeof(gpio_devs[0])); io_number++)
    {
      BK_RETURN_ON_ERR(gpio_dev_unmap(
        gpio_devs[io_number].beken_gpio.gpio_id));
      gpio_devs[io_number].beken_gpio.virtual_gpio_id = io_number;

      BK_RETURN_ON_ERR(bk_gpio_set_config(gpio_devs[io_number].beken_gpio.gpio_id,&(gpio_devs[io_number].beken_gpio.gpio_config)));
      rtn_value = gpio_pin_register(&gpio_devs[io_number].beken_gpio.gpio, gpio_devs[io_number].beken_gpio.gpio_id);

      if (rtn_value != OK)
        {
          gpioerr("Failed to register gpio %d  \n",gpio_devs[io_number].beken_gpio.gpio_id);
          return rtn_value;
        }

      gpioinfo("Succeed to register gpio %d \n",gpio_devs[io_number].beken_gpio.gpio_id);
    }

  return OK;
}

#endif /* CONFIG_DEV_GPIO && !CONFIG_GPIO_LOWER_HALF */
