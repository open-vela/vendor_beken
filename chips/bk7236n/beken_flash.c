/****************************************************************************
 * vendor/beken/chip/bk7236n/beken_flash.c
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

#include <debug.h>
#include <nuttx/config.h>

#include <nuttx/mutex.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <nuttx/fs/ioctl.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/fs/fs.h>

#include "arm_internal.h"
#include "driver/flash.h"

#include <stdio.h>
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define MTD_ERASED_STATE  CONFIG_BK7236N_FLASH_ERASE_STATE
#define FLASH_SECTOR_SIZE CONFIG_BK7236N_FLASH_SECTOR_SIZE
#define FLASH_SIZE        CONFIG_BK7236N_FLASH_SIZE
#define MTD_BLK_SIZE      CONFIG_BK7236N_FLASH_MTD_BLKSIZE
#define MTD_ERASE_SIZE    FLASH_SECTOR_SIZE

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This type represents the state of the MTD device.  The struct mtd_dev_s
 * must appear at the beginning of the definition so that you can freely
 * cast between pointers to struct mtd_dev_s and struct beken_dev_s.
 */

struct beken_dev_s
{
  struct mtd_dev_s mtd;

  /* Other implementation specific data may follow here */
  bool initialized;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* MTD driver methods */

static int     beken_erase(FAR struct mtd_dev_s *dev, off_t startblock,
                 size_t nblocks);
static ssize_t beken_bread(FAR struct mtd_dev_s *dev, off_t startblock,
                 size_t nblocks, FAR uint8_t *buf);
static ssize_t beken_bwrite(FAR struct mtd_dev_s *dev, off_t startblock,
                 size_t nblocks, FAR const uint8_t *buf);
static ssize_t beken_read(FAR struct mtd_dev_s *dev, off_t offset,
                 size_t nbytes, FAR uint8_t *buffer);
#ifdef CONFIG_MTD_BYTE_WRITE
static ssize_t beken_write(FAR struct mtd_dev_s *dev, off_t offset,
                 size_t nbytes, FAR const uint8_t *buffer);
#endif
static int     beken_ioctl(FAR struct mtd_dev_s *dev, int cmd,
                 unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* This structure holds the state of the MTD driver */

static struct beken_dev_s g_mtddev =
{
  {
    beken_erase,
    beken_bread,
    beken_bwrite,
    beken_read,
#ifdef CONFIG_MTD_BYTE_WRITE
    beken_write,   /* Should be NULL if the byte write method is not supported */
#endif
    beken_ioctl
  },

  /* Initialization of any other implementation specific data goes here */
  false,
};

static mutex_t g_lock = NXMUTEX_INITIALIZER;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_erase
 *
 * Description:
 *   Erase several blocks, each of the size previously reported.
 *
 ****************************************************************************/

static int beken_erase(FAR struct mtd_dev_s *dev, off_t startblock,
                      size_t nblocks)
{
  FAR struct beken_dev_s *priv = (FAR struct beken_dev_s *)dev;
  uint32_t offset = startblock * MTD_ERASE_SIZE;

  /* The interface definition assumes that all erase blocks are the same
   * size. If that is not true for this particular device, then transform
   * the start block and nblocks as necessary.
   */

  /* Erase the specified blocks and return status (OK or a negated errno) */
  if (!priv->initialized)
    {
      return -EINVAL;
    }
  nxmutex_lock(&g_lock);

  bk_flash_set_protect_type(FLASH_PROTECT_NONE);

  for (int i=0; i<nblocks; i++)
    {
      bk_flash_erase_sector(offset + i * MTD_ERASE_SIZE);
    }

  bk_flash_set_protect_type(FLASH_UNPROTECT_LAST_BLOCK);

  nxmutex_unlock(&g_lock);

  return nblocks;
}

/****************************************************************************
 * Name: beken_bread
 *
 * Description:
 *   Read the specified number of blocks into the user provided buffer.
 *
 ****************************************************************************/

static ssize_t beken_bread(FAR struct mtd_dev_s *dev, off_t startblock,
                          size_t nblocks, FAR uint8_t *buf)
{
  ssize_t ret;
  FAR struct beken_dev_s *priv = (FAR struct beken_dev_s *)dev;
  uint32_t offset = startblock * MTD_BLK_SIZE;
  uint32_t nbytes = nblocks * MTD_BLK_SIZE;

  /* The interface definition assumes that all read/write blocks are the
   * same size.  If that is not true for this particular device, then
   * transform the start block and nblocks as necessary.
   */

  /* Read the specified blocks into the provided user buffer and return
   * status (The positive, number of blocks actually read or a negated
   * errno).
   */
  if (!priv->initialized)
    {
      return -EINVAL;
    }
  nxmutex_lock(&g_lock);

  ret = bk_flash_read_bytes(offset, buf, nbytes);
  if (ret == OK)
    {
      ret = nblocks;
    }
  nxmutex_unlock(&g_lock);

  return ret;
}

/****************************************************************************
 * Name: beken_bwrite
 *
 * Description:
 *   Write the specified number of blocks from the user provided buffer.
 *
 ****************************************************************************/

static ssize_t beken_bwrite(FAR struct mtd_dev_s *dev, off_t startblock,
                           size_t nblocks, FAR const uint8_t *buf)
{
  ssize_t ret;
  FAR struct beken_dev_s *priv = (FAR struct beken_dev_s *)dev;
  uint32_t offset = startblock * MTD_BLK_SIZE;
  uint32_t nbytes = nblocks * MTD_BLK_SIZE;

  /* The interface definition assumes that all read/write blocks are the
   * same size.  If that is not true for this particular device, then
   * transform the start block and nblocks as necessary.
   */

  /* Write the specified blocks from the provided user buffer and return
   * status (The positive, number of blocks actually written or a negated
   * errno)
   */
  if (!priv->initialized)
    {
      return -EINVAL;
    }
  nxmutex_lock(&g_lock);
  bk_flash_set_protect_type(FLASH_PROTECT_NONE);
  ret = bk_flash_write_bytes(offset, buf, nbytes);
  bk_flash_set_protect_type(FLASH_UNPROTECT_LAST_BLOCK);
  if (ret == OK)
    {
      ret = nblocks;
    }
  nxmutex_unlock(&g_lock);

  return ret;
}

/****************************************************************************
 * Name: beken_read
 *
 * Description:
 *   Read the specified number of bytes to the user provided buffer.
 *
 ****************************************************************************/

static ssize_t beken_read(FAR struct mtd_dev_s *dev, off_t offset,
                         size_t nbytes, FAR uint8_t *buffer)
{
  ssize_t ret;
  FAR struct beken_dev_s *priv = (FAR struct beken_dev_s *)dev;

  /* Some devices may support byte oriented read (optional).  Byte-oriented
   * writing is inherently block oriented on most MTD devices and is not
   * supported.  It is recommended that low-level drivers not support read()
   * if it requires buffering -- let the higher level logic handle that.  If
   * the read method is not implemented, just set the method pointer to NULL
   * in the struct mtd_dev_s instance.
   */

  /* The interface definition assumes that all read/write blocks are the
   * same size.  If that is not true for this particular device, then
   * transform the start block and nblocks as necessary.
   */

  /* Read the specified bytes into the provided user buffer and return
   * status (The positive, number of bytes actually read or a negated
   * errno)
   */
  if (!priv->initialized)
    {
      return -EINVAL;
    }
  nxmutex_lock(&g_lock);

  ret = bk_flash_read_bytes(offset, buffer, nbytes);
  if (ret == OK)
    {
      ret = nbytes;
    }
  nxmutex_unlock(&g_lock);

  return ret;
}

/****************************************************************************
 * Name: beken_write
 *
 * Description:
 *   Some FLASH parts have the ability to write an arbitrary number of
 *   bytes to an arbitrary offset on the device.  This method should be
 *   implement only for devices that support such access.
 *
 ****************************************************************************/

#ifdef CONFIG_MTD_BYTE_WRITE
static ssize_t beken_write(FAR struct mtd_dev_s *dev, off_t offset,
                          size_t nbytes, FAR const uint8_t *buffer)
{
  ssize_t ret;
  FAR struct beken_dev_s *priv = (FAR struct beken_dev_s *)dev;
  if (!priv->initialized)
    {
      return -EINVAL;
    }
  nxmutex_lock(&g_lock);

  bk_flash_set_protect_type(FLASH_PROTECT_NONE);
  ret = bk_flash_write_bytes(offset, buffer, nbytes);
  if (ret == OK)
    {
      ret = nbytes;
    }
  bk_flash_set_protect_type(FLASH_UNPROTECT_LAST_BLOCK);
  nxmutex_unlock(&g_lock);

  return ret;
}
#endif

/****************************************************************************
 * Name: beken_ioctl
 ****************************************************************************/

static int beken_ioctl(FAR struct mtd_dev_s *dev, int cmd, unsigned long arg)
{
  FAR struct beken_dev_s *priv = (FAR struct beken_dev_s *)dev;
  int ret = -EINVAL; /* Assume good command with bad parameters */

  if (!priv->initialized)
    {
      return -EINVAL;
    }

  switch (cmd)
    {
      case MTDIOC_GEOMETRY:
        {
          FAR struct mtd_geometry_s *geo = (FAR struct mtd_geometry_s *)arg;
          if (geo)
            {
              memset(geo, 0, sizeof(*geo));

              /* Populate the geometry structure with information needed to
               * know the capacity and how to access the device.
               *
               * NOTE:
               * that the device is treated as though it where just an array
               * of fixed size blocks. That is most likely not true, but the
               * client will expect the device logic to do whatever is
               * necessary to make it appear so.
               */

              geo->blocksize    = MTD_BLK_SIZE;              /* Size of one read/write block */
              geo->erasesize    = MTD_ERASE_SIZE;            /* Size of one erase block */
              geo->neraseblocks = FLASH_SIZE/MTD_ERASE_SIZE; /* Number of erase blocks */
              ret               = OK;
          }
        }
        break;

      case BIOC_PARTINFO:
        {
          FAR struct partition_info_s *info =
            (FAR struct partition_info_s *)arg;
          if (info != NULL)
            {
              info->numsectors  = FLASH_SIZE/MTD_BLK_SIZE;
              info->sectorsize  = MTD_BLK_SIZE;
              info->startsector = 0;
              info->parent[0]   = '\0';
              ret               = OK;
            }
        }
        break;

      case BIOC_XIPBASE:
        {
          /* Since the code blocks have additional crc section which is also
           * taken into account of calculating hash value, so it is not
           * appropriate to access it directly by the base address,
           * and simply return -ENOTTY here.
           */
          ret = -ENOTTY;
        }
        break;

      case MTDIOC_BULKERASE:
        {
          /* Erase the entire device */

          ret = OK;
        }
        break;

      case MTDIOC_ERASESTATE:
        {
          uint8_t *result = (uint8_t *)arg;
          *result = MTD_ERASED_STATE;

          ret = OK;
        }
        break;

      default:
        ret = -ENOTTY; /* Bad command */
        break;
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
/****************************************************************************
 * Name: beken_alloc_mtdpart
 *
 * Description:
 *   Allocate mtd partition.
 *
 ****************************************************************************/

struct mtd_dev_s *beken_alloc_mtdpart(uint32_t mtd_offset, uint32_t mtd_size, bool encrypted)
{
  struct mtd_dev_s *mtd_part;

  uint32_t startblock = mtd_offset/MTD_BLK_SIZE;
  uint32_t blocks = mtd_size/MTD_BLK_SIZE;

  mtd_part = mtd_partition((struct mtd_dev_s *)&g_mtddev, startblock, blocks);
  if (!mtd_part)
  {
    return NULL;
  }

  return mtd_part;
}

/****************************************************************************
 * Name: beken_mtd_init
 *
 * Description:
 *   Create and initialize an MTD device instance.  MTD devices are not
 *   registered in the file system, but are created as instances that can
 *   be bound to other functions (such as a block or character driver front
 *   end).
 *
 ****************************************************************************/

FAR struct mtd_dev_s *beken_mtd_init(void)
{
  /* Allocate an instance of the private data structure -- OR, if there can
   * only be a single instance of the driver, then use a shared, global
   * device structure.
   */
  if (!g_mtddev.initialized)
    {
      if (OK != bk_flash_driver_init())
        {
          return NULL;
        }
      g_mtddev.initialized = true;
    }

  /* Return the implementation-specific state structure as the MTD device */

  return (FAR struct mtd_dev_s *)&g_mtddev;
}
