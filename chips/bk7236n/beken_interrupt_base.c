/****************************************************************************
 * vendor/beken/chip/bk7236n/beken_interrupt_base.c
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

#include <inttypes.h>
#include <nuttx/irq.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>
#include <sys/types.h>

#include "beken_irq.h"
#include "driver/int_types.h"
#include "beken_interrupt_base.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define ICU_RETURN_ON_INVALID_DEVS(dev) do {\
                if ((dev) >= INT_SRC_NONE) {\
                    return BK_ERR_INT_DEVICE_NONE;\
                }\
            } while(0)

#define ICU_INTERRUPT_MAP_FROM_CMSIS_TO_ARMV8(cmsis_num,armv8_num) do {\
                armv8_num = cmsis_num + BK7236N_IRQ_FIRST;\
                } while(0)


extern const icu_int_map_t icu_int_map_table[];
static isr_func_t isr_list[INT_SRC_NONE] = {0};

static int bk_int_src_from_irq(int irq)
{
  int src;

  for (src = 0; src < INT_SRC_NONE; src++)
    {
      if ((icu_int_map_table[src].int_bit + BK7236N_IRQ_FIRST) == irq)
        {
          return src;
        }
    }

  return -EINVAL;
}

static int bk_int_isr_wrap(int irq, void *context, void *arg)
{
  int src = bk_int_src_from_irq(irq);
  isr_func_t isr;

  UNUSED(context);
  UNUSED(arg);

  if (src < 0)
    {
      return src;
    }

  isr = isr_list[src];
  if (isr == NULL)
    {
      return -EINVAL;
    }

  isr();
  return 0;
}

/****************************************************************************
* Name: bk_int_isr_register
*
* Description:
*   Register the isr
*
* Input Parameters:
*   src:The interrupt source id.
*   isr_callback:The callback function of interrupt src.
*   arg:The arguments of the callback function .
*
* Returned Value:
*   Zero (OK) on success; a negated errno on failure
*
****************************************************************************/

int bk_int_isr_register(icu_int_src_t src, isr_func_t isr_callback, void *arg)
{
  ICU_RETURN_ON_INVALID_DEVS(src);

  const icu_int_map_t *icu_int_map = &icu_int_map_table[src];
  uint8_t int_num = 0;
  int_num = icu_int_map->int_bit + BK7236N_IRQ_FIRST;
  up_disable_irq(int_num);
  isr_list[src] = isr_callback;
  irq_attach(int_num, bk_int_isr_wrap, arg);
#ifdef CONFIG_ARCH_IRQPRIO
  up_prioritize_irq(int_num, icu_int_map->int_prio);
#endif
  up_enable_irq(int_num);

  return OK;
}

/****************************************************************************
* Name: bk_int_set_priority
*
* Description:
*   Set the priority of the intterupt.

* Input Parameters:
*   src:The interrupt source id.
*   isr_callback:The callback function of interrupt src.
*   arg:The arguments of the callback function .
*
* Returned Value:
*   Zero (OK) on success; a negated errno on failure
*
****************************************************************************/
int bk_int_set_priority(icu_int_src_t int_src, uint32_t int_priority)
{
#ifdef CONFIG_ARCH_IRQPRIO
  const icu_int_map_t *icu_int_map = &icu_int_map_table[int_src];
  uint8_t int_num;

  int_num = icu_int_map->int_bit + BK7236N_IRQ_FIRST;

  up_prioritize_irq(int_num, icu_int_map->int_prio);
#endif
  return OK;
}

/****************************************************************************
* Name: bk_int_isr_unregister
*
* Description:
*   Unregister the isr
*
* Input Parameters:
*   src:The interrupt source id.
*
* Returned Value:
*   Zero (OK) on success; a negated errno on failure
*
****************************************************************************/
int bk_int_isr_unregister(icu_int_src_t src)
{
  ICU_RETURN_ON_INVALID_DEVS(src);

  const icu_int_map_t *icu_int_map = &icu_int_map_table[src];
  uint8_t int_num;

  int_num = icu_int_map->int_bit + BK7236N_IRQ_FIRST;

  up_disable_irq(int_num);
  irq_detach(int_num);
  isr_list[src] = NULL;

  return OK;
}

/****************************************************************************
* Name: bk_get_int_number
*
* Description:
*   Get the priority number of intterrupt
*
* Input Parameters:
*   src:The interrupt source id.
*
* Returned Value:
*   The priority number
*
****************************************************************************/
int32_t bk_get_int_number(icu_int_src_t src)
{
  ICU_RETURN_ON_INVALID_DEVS(src);

  return (int32_t) icu_int_map_table[src].int_bit;
}


