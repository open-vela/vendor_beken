/****************************************************************************
 * vendor/beken/chip/bk7236n/beken_aes.c
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <debug.h>
#include <semaphore.h>

#include <nuttx/mutex.h>
#include <nuttx/crypto/crypto.h>

#include "modules/vela_te/crypto/dubhe_sca.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AES_BLK_SIZE                    (16)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_aes_inited;
static mutex_t g_aes_lock = NXMUTEX_INITIALIZER;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_aes_cbc_cypher
 *
 * Description:
 *   Process AES CBC encryption/decryption.
 *
 * Input Parameters:
 *   ctx     - AES context
 *   encrypt - True: encryption mode; False: decryption mode
 *   ivptr   - Initialization vector pointer
 *   input   - Input data pointer
 *   output  - Output buffer pointer
 *   size    - Data size in bytes
 *
 * Returned Value:
 *   OK is returned on success. Otherwise, a negated errno value is returned.
 *
 ****************************************************************************/

int beken_aes_cbc_cypher(arm_ce_sca_context_t *ctx, bool encrypt, void *ivptr,
                         const void *input, void *output, uint32_t size)
{
  int ret;

  DEBUGASSERT(ctx && input && output && ivptr);
  DEBUGASSERT(size && ((size % AES_BLK_SIZE) == 0));

  ret = nxmutex_lock(&g_aes_lock);
  if (ret < 0)
    {
      return ret;
    }

  ret = arm_ce_sca_crypt( ctx,
                          ( encrypt ?
                            ARM_CE_SCA_OPERATION_ENCRYPT :
                            ARM_CE_SCA_OPERATION_DECRYPT ),
                            ARM_CE_AES_CBC,
                            0,
                            size,
                            NULL,
                            ivptr,
                            NULL,
                            input,
                            output );
  if (ret < 0)
    {
      return ret;
    }

  ret = nxmutex_unlock(&g_aes_lock);
  if (ret < 0)
    {
      return ret;
    }

  return OK;
}

/****************************************************************************
 * Name: beken_aes_ctr_cypher
 *
 * Description:
 *   Process AES CTR encryption/decryption.
 *
 * Input Parameters:
 *   ctx      - AES context
 *   offptr   - Offset buffer pointer
 *   cntptr   - Counter buffer pointer
 *   cacheptr - Counter calculation buffer pointer
 *   input    - Input data pointer
 *   output   - Output buffer pointer
 *   size     - Data size in bytes
 *
 * Returned Value:
 *   OK is returned on success. Otherwise, a negated errno value is returned.
 *
 ****************************************************************************/

int beken_aes_ctr_cypher(arm_ce_sca_context_t *ctx, size_t *offptr,
                         void *cntptr, void *cacheptr, const void *input,
                         void *output, uint32_t size)
{
  int ret;

  DEBUGASSERT(ctx && offptr && cntptr && cacheptr && input && output);
  DEBUGASSERT(size);

  ret = nxmutex_lock(&g_aes_lock);
  if (ret < 0)
    {
      return ret;
    }
  ret = arm_ce_sca_crypt( ctx,
                          ARM_CE_SCA_OPERATION_ENCRYPT,
                          ARM_CE_AES_CTR,
                          0,
                          size,
                          offptr,
                          cntptr,
                          cacheptr,
                          input,
                          output );
  if (ret < 0)
    {
      return ret;
    }

  ret = nxmutex_unlock(&g_aes_lock);
  if (ret < 0)
    {
      return ret;
    }

  return OK;
}

/****************************************************************************
 * Name: beken_aes_setkey
 *
 * Description:
 *   Configurate AES key.
 *
 * Input Parameters:
 *   ctx     - AES context
 *   keyptr  - Key data pointer
 *   keybits - Key data bits
 *
 * Returned Value:
 *   OK is returned on success. Otherwise, a negated errno value is returned.
 *
 ****************************************************************************/

int beken_aes_setkey(arm_ce_sca_context_t *ctx, const void *keyptr,
                     uint16_t keybits)
{
  DEBUGASSERT(ctx && keyptr);

  if ((keybits != 128) && (keybits != 192) && (keybits != 256))
    {
      return -EINVAL;
    }

  int ret = 0;
  ret = arm_ce_sca_set_key(ctx, ARM_CE_SCA_EXTERNAL_KEY, keyptr, NULL, NULL, keybits, 0);

  return ret;
}

/****************************************************************************
 * Name: beken_aes_init
 *
 * Description:
 *   Initialize AES hardware.
 *
 * Input Parameters:
 *   ctx     - AES context
 *
 * Returned Value:
 *   OK is returned on success. Otherwise, a negated errno value is returned.
 *
 ****************************************************************************/

int beken_aes_init(arm_ce_sca_context_t *ctx)
{
  int ret = 0;
  if (!g_aes_inited)
    {
      ret = arm_ce_sca_init(ctx);
      if (ret < 0){
        return ret;
      }
      ctx->alg_type = ARM_CE_AES;
      g_aes_inited = true;
    }

  return OK;
}

/****************************************************************************
 * Name: aes_cypher
 *
 * Description:
 *   Process AES encryption/decryption.
 *
 * Input Parameters:
 *   out     - Output buffer pointer
 *   in      - Input data pointer
 *   size    - Data size in bytes
 *   iv      - Initialization vector pointer
 *   key     - Key data pointer
 *   keysize - Key data size in byte
 *   mode    - encryption mode, AES_MODE_CBC or AES_MODE_CTR
 *   encrypt - True: encryption mode; False: decryption mode
 *
 * Returned Value:
 *   OK is returned on success. Otherwise, a negated errno value is returned.
 *
 ****************************************************************************/

#ifdef CONFIG_CRYPTO_AES

int aes_cypher(void *out, const void *in, size_t size,
               const void *iv, const void *key, size_t keysize,
               int mode, int encrypt)
{
  int ret;
  uint8_t iv_buf[AES_BLK_SIZE];
  uint8_t cache_buf[AES_BLK_SIZE];
  size_t nc_off;
  static arm_ce_sca_context_t ctx;

  if ((size & (AES_BLK_SIZE - 1)) != 0)
    {
      return -EINVAL;
    }
  if ((keysize != 16) && (keysize != 24) && (keysize != 32))
    {
      return -EINVAL;
    }

  if ((mode != AES_MODE_CBC) &&
      (mode != AES_MODE_CTR))
    {
      return -EINVAL;
    }

  ret = beken_aes_init(&ctx);
  if (ret < 0)
    {
      return ret;
    }

  ret = beken_aes_setkey(&ctx, key, keysize * 8);
  if (ret < 0)
    {
      return ret;
    }
  switch (mode)
    {
      case AES_MODE_CBC:
        memcpy(iv_buf, iv, AES_BLK_SIZE);
        ret = beken_aes_cbc_cypher(&ctx, encrypt, iv_buf, in, out, size);
        break;
      case AES_MODE_CTR:
        nc_off = 0;
        memcpy(iv_buf, iv, AES_BLK_SIZE);
        ret = beken_aes_ctr_cypher(&ctx, &nc_off, iv_buf, cache_buf,
                                   in, out, size);
        break;
      default :
        ret = -EINVAL;
        break;
    }
  return ret;
}

#endif

