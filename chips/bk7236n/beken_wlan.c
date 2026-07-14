/****************************************************************************
 * vendor/beken/chips/bk7236n/beken_wlan.c
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

#include <debug.h>
#include <string.h>

#include <nuttx/compiler.h>
#include <nuttx/kmalloc.h>
#include <nuttx/wqueue.h>
#include <nuttx/net/net.h>
#include <nuttx/net/netdev_lowerhalf.h>
#include <nuttx/net/pkt.h>
#include <netinet/if_ether.h>

#ifdef IS_ALIGNED
#  undef IS_ALIGNED
#endif

#include "modules/wifi.h"
#include "components/system.h"
#include "components/event.h"

uint32_t bk_wifi_get_rx_buffer(uint8_t **buf, uint8_t **header);
void bk_wifi_clear_rx_buffer(uint8_t *buf);
uint32_t bk_wifi_send_tx_eth(uint8_t *buf, uint32_t len);

/****************************************************************************
 * Private Types
 ****************************************************************************/

#define WLAN_INTERFACE_NAME "wlan0"

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int beken_wlan_send(struct netdev_lowerhalf_s *dev, netpkt_t *pkt);
static netpkt_t *beken_wlan_recv(struct netdev_lowerhalf_s *dev);
static int beken_wlan_ifup(struct netdev_lowerhalf_s *dev);
static int beken_wlan_ifdown(struct netdev_lowerhalf_s *dev);

static int beken_wlan_sta_connect(FAR struct netdev_lowerhalf_s *dev);
static int beken_wlan_sta_disconnect(FAR struct netdev_lowerhalf_s *dev);
static int beken_wlan_sta_essid(FAR struct netdev_lowerhalf_s *dev,
                            FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_bssid(FAR struct netdev_lowerhalf_s *dev,
                            FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_passwd(FAR struct netdev_lowerhalf_s *dev,
                             FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_mode(FAR struct netdev_lowerhalf_s *dev,
                           FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_auth(FAR struct netdev_lowerhalf_s *dev,
                           FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_freq(FAR struct netdev_lowerhalf_s *dev,
                           FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_bitrate(FAR struct netdev_lowerhalf_s *dev,
                              FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_txpower(FAR struct netdev_lowerhalf_s *dev,
                              FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_country(FAR struct netdev_lowerhalf_s *dev,
                              FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_sensitivity(FAR struct netdev_lowerhalf_s *dev,
                                  FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_scan(FAR struct netdev_lowerhalf_s *dev,
                           FAR struct iwreq *iwr, bool set);
static int beken_wlan_sta_range(FAR struct netdev_lowerhalf_s *dev,
                            FAR struct iwreq *iwr);

static int g_if_state = 0;
/****************************************************************************
 * Private Data
 ****************************************************************************/

struct netdev_lowerhalf_s g_sta_dev;

static const struct netdev_ops_s g_ops =
{
  beken_wlan_ifup,   /* ifup */
  beken_wlan_ifdown, /* ifdown */
  beken_wlan_send,   /* transmit */
  beken_wlan_recv    /* receive */
};

static const struct wireless_ops_s g_sta_iw_ops =
{
  beken_wlan_sta_connect,
  beken_wlan_sta_disconnect,
  beken_wlan_sta_essid,
  beken_wlan_sta_bssid,
  beken_wlan_sta_passwd,
  beken_wlan_sta_mode,
  beken_wlan_sta_auth,
  beken_wlan_sta_freq,
  beken_wlan_sta_bitrate,
  beken_wlan_sta_txpower,
  beken_wlan_sta_country,
  beken_wlan_sta_sensitivity,
  beken_wlan_sta_scan,
  beken_wlan_sta_range
};

static wifi_sta_config_t g_sta_config;
static bool g_is_sta_connected;

struct iw_scan_result_s
{
  int      total_len;
  int       cur_len;
  FAR char *buf;                    /* iwr->u.data.pointer */
};

static volatile bool g_scan_done = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int32_t wifi_errno_trans(int ret)
{
  if (ret == BK_OK)
    {
      return OK;
    }
  else
    {
      return ERROR;
    }
}

/****************************************************************************
 * Name: beken_wlan_ifup
 *
 * Description:
 *   Bring up the Wlan interface when an IP address is provided.
 *
 * Input Parameters:
 *   dev - Reference to the NuttX driver state structure
 *
 * Returned Value:
 *   OK on success.Negated errno returned on failure.
 *
 ****************************************************************************/

static int beken_wlan_ifup(struct netdev_lowerhalf_s *dev)
{
  if (g_if_state == 0)
    {
      int ret = bk_wifi_sta_start();
      if (ret < 0 )
        {
          wlerr("bk_wlan_sta_start failed. err:%d", ret);
          return wifi_errno_trans(ret);
        }
      g_if_state = 1;
      netdev_lower_carrier_on(dev);
    }
  return OK;
}

/****************************************************************************
 * Name: beken_wlan_ifdown
 *
 * Description:
 *   Stop the Wlan interface.
 *
 * Input Parameters:
 *   dev - Reference to the NuttX driver state structure
 *
 * Returned Value:
 *   OK on success.Negated errno returned on failure.
 *
 ****************************************************************************/

static int beken_wlan_ifdown(struct netdev_lowerhalf_s *dev)
{
  netdev_lower_carrier_off(dev);
  /* Nothing to do */
  return OK;
}

/****************************************************************************
 * Name: beken_wlan_send
 *
 * Description:
 *   Try to send all TX packets in TX ready queue to Wi-Fi driver.
 *
 * Input Parameters:
 *   dev - Reference to the NuttX driver state structure
 *   pkt - The packets to transmit
 *
 * Returned Value:
 *   OK on success.Negated errno value for failure.
 *
 ****************************************************************************/

static int beken_wlan_send(struct netdev_lowerhalf_s *dev, netpkt_t *pkt)
{
  static uint8_t tx_buf[2048];
  unsigned int len  = netpkt_getdatalen(dev, pkt);
  int rtn = 0;

  if (netpkt_is_fragmented(pkt))
    {
      netpkt_copyout(dev, tx_buf, pkt, len, 0);
      rtn = bk_wifi_send_tx_eth(tx_buf, len);
    }
  else
    {
      rtn = bk_wifi_send_tx_eth(netpkt_getdata(dev, pkt), len);
    }
  if (rtn > 0)
    {
      netpkt_free(dev, pkt, NETPKT_TX);
      return OK;
    }
  else
    {
      return -ENOMEM;
    }
}

void netdriver_txdone(void)
{
  netdev_lower_txdone(&g_sta_dev);
}


/****************************************************************************
 * Name: netdriver_rxready
 *
 * Description:
 *   The MAC layer notify the RX packets are ready.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void netdriver_rxready(void)
{
  netdev_lower_rxready(&g_sta_dev);
}

/****************************************************************************
 * Name: beken_wlan_recv
 *
 * Description:
 *   Receive/Poll the packets from MAC layer.
 *
 * Input Parameters:
 *   dev - Reference to the NuttX driver state structure
 *
 * Returned Value:
 *   The received packets
 *
 ****************************************************************************/

static netpkt_t *beken_wlan_recv(struct netdev_lowerhalf_s *dev)
{
  netpkt_t *pkt = NULL;
  uint32_t len;
  uint8_t* buf;
  uint8_t* header;

  len = bk_wifi_get_rx_buffer(&buf, &header);
  if (len > 0)
    {
      pkt = netpkt_alloc(dev, NETPKT_RX);
      if (pkt == NULL)
        {
          wlerr("netpkt_alloc failed");
          return pkt;
        }
      netpkt_copyin(dev, pkt, buf, len, 0);
      bk_wifi_clear_rx_buffer(header);
    }
  return pkt;
}

/****************************************************************************
 * Name: beken_wlan_sta_connect
 *
 * Description:
 *   Trigger Wi-Fi station connection action
 *
 * Input Parameters:
 *   dev - Reference to the NuttX driver state structure
 *
 * Returned Value:
 *   OK on success.Negated errno returned on failure.
 *
 ****************************************************************************/

static int beken_wlan_sta_connect(FAR struct netdev_lowerhalf_s *dev)
{
  bk_err_t ret = bk_wifi_sta_connect();
  if (ret != BK_OK)
    {
      wlerr("bk_wifi_sta_connect failed, err：%d", ret);
    }
  return wifi_errno_trans(ret);
}

/****************************************************************************
 * Name: beken_wlan_sta_disconnect
 *
 * Description:
 *   Trigger Wi-Fi station disconnection action
 *
 * Input Parameters:
 *   dev - Reference to the NuttX driver state structure
 *
 * Returned Value:
 *   OK on success.Negated errno returned on failure.
 *
 ****************************************************************************/

static int beken_wlan_sta_disconnect(FAR struct netdev_lowerhalf_s *dev)
{
  bk_err_t ret = bk_wifi_sta_disconnect();
  if (ret != BK_OK)
    {
      wlerr("bk_wifi_sta_disconnect failed, err：%d", ret);
    }
  memset(&g_sta_config, 0x0, sizeof(g_sta_config));
  return wifi_errno_trans(ret);
}

/****************************************************************************
 * Name: beken_wlan_sta_essid
 *
 * Description:
 *   Set/Get Wi-Fi station ESSID
 *
 * Input Parameters:
 *   dev - Reference to the NuttX driver state structure
 *   iwr - The argument of the ioctl cmd
 *   set - true: set data; false: get data
 *
 * Returned Value:
 *   OK on success.Negated errno returned on failure.
 *
 ****************************************************************************/

static int beken_wlan_sta_essid(FAR struct netdev_lowerhalf_s *dev,
                            FAR struct iwreq *iwr, bool set)
{
  int ret;
  struct iw_point *essid = &iwr->u.essid;
  uint8_t *pdata;
  uint8_t len;
#ifdef CONFIG_DEBUG_WIRELESS_INFO
  char buf[WIFI_SSID_STR_LEN];
#endif

  DEBUGASSERT(essid != NULL);

  pdata = essid->pointer;
  len   = essid->length;

  if (set && len > WIFI_SSID_STR_LEN)
    {
      return -EINVAL;
    }

  if (set)
    {
      memset(g_sta_config.ssid, 0x0, WIFI_SSID_STR_LEN);
      memcpy(g_sta_config.ssid, pdata, len);

      ret = bk_wifi_sta_set_config(&g_sta_config);
      if (ret != BK_OK)
        {
          wlerr("ERROR: Failed to set Wi-Fi config data ret=%d\n", ret);
          return wifi_errno_trans(ret);
        }
    }
  else
    {
      wifi_sta_config_t sta_config;
      memset(&sta_config, 0x0, sizeof(wifi_sta_config_t));
      ret = bk_wifi_sta_get_config(&sta_config);
      if (ret != BK_OK)
        {
          wlerr("ERROR: Failed to ger Wi-Fi config data ret=%d\n", ret);
          return wifi_errno_trans(ret);
        }
      len = strlen(sta_config.ssid);
      memcpy(pdata, sta_config.ssid, len);
      if (g_is_sta_connected)
        {
          essid->flags = IW_ESSID_ON;
        }
      else
        {
          essid->flags = IW_ESSID_OFF;
        }
    }

#ifdef CONFIG_DEBUG_WIRELESS_INFO
  memcpy(buf, pdata, len);
  buf[len] = 0;
  wlinfo("\nINFO: Wi-Fi station ssid=%s len=%d\n", buf, len);
#endif

  return OK;
}

/****************************************************************************
 * Name: beken_wlan_sta_bssid
 *
 * Description:
 *   Set/Get Wi-Fi station BSSID
 *
 * Input Parameters:
 *   dev - Reference to the NuttX driver state structure
 *   iwr - The argument of the ioctl cmd
 *   set   - true: set data; false: get data
 *
 * Returned Value:
 *   OK on success (Not implemented. ToDo)
 *
 ****************************************************************************/

static int beken_wlan_sta_bssid(FAR struct netdev_lowerhalf_s *dev,
                            FAR struct iwreq *iwr, bool set)
{
  wifi_link_status_t link_status;
  bk_err_t rtn;
  struct sockaddr *sockaddr;
  char *pdata;

  if (set)
    {
      return -ENOSYS;
    }

  sockaddr = &iwr->u.ap_addr;
  pdata    = sockaddr->sa_data;

  memset(&link_status, 0x0, sizeof(link_status));
  rtn = bk_wifi_sta_get_link_status(&link_status);
  if (rtn == BK_OK)
    {
      memcpy(pdata, link_status.bssid, WIFI_BSSID_LEN);
    }
  else
    {
      wlerr("ERROR: Failed to get link status=%d\n", rtn);
    }

  return OK;
}

/****************************************************************************
* Name: beken_wlan_sta_passwd
*
* Description:
*   Set/Get Wi-Fi station password
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*   set   - true: set data; false: get data
*
* Returned Value:
*   OK on success.Negated errno returned on failure.
*
****************************************************************************/

static int beken_wlan_sta_passwd(FAR struct netdev_lowerhalf_s *dev,
                             FAR struct iwreq *iwr, bool set)
{
  int ret;
  struct iw_encode_ext *ext = iwr->u.encoding.pointer;
  uint8_t *pdata;
  uint8_t len;
#ifdef CONFIG_DEBUG_WIRELESS_INFO
  char buf[WIFI_PASSWORD_LEN];
#endif

  DEBUGASSERT(ext != NULL);

  pdata = ext->key;
  len   = ext->key_len;
  if (set)
    {
      if (len > WIFI_PASSWORD_LEN)
        {
          return -EINVAL;
        }

      memset(g_sta_config.password, 0x0, WIFI_PASSWORD_LEN);
      memcpy(g_sta_config.password, pdata, len);
      if (g_sta_config.ssid[0] != '\0')
        {
          ret = bk_wifi_sta_set_config(&g_sta_config);
          if (ret)
            {
              wlerr("ERROR: Failed to set Wi-Fi config data ret=%d\n", ret);
              return wifi_errno_trans(ret);
            }
        }
    }
  else
    {
      wifi_sta_config_t sta_config;
      memset(&sta_config, 0x0, sizeof(wifi_sta_config_t));
      ret = bk_wifi_sta_get_config(&sta_config);
      if (ret != BK_OK)
        {
          wlerr("ERROR: Failed to ger Wi-Fi config data ret=%d\n", ret);
          return wifi_errno_trans(ret);
        }
      ext->key_len = strlen(sta_config.password);
      memcpy(pdata, sta_config.password, ext->key_len);
      if (g_is_sta_connected)
        {
          wifi_link_status_t link_status;
          bk_err_t rtn;
          memset(&link_status, 0x0, sizeof(link_status));
          rtn = bk_wifi_sta_get_link_status(&link_status);
          if (rtn == BK_OK)
            {
              switch (link_status.security)
              {
              case WIFI_SECURITY_NONE:
                ext->alg = IW_ENCODE_ALG_NONE;
                break;
              case WIFI_SECURITY_WEP:
                ext->alg = IW_ENCODE_ALG_WEP;
                break;
              case WIFI_SECURITY_WPA_TKIP:
              case WIFI_SECURITY_WPA2_TKIP:
                ext->alg = IW_ENCODE_ALG_TKIP;
                break;
              case WIFI_SECURITY_WPA_AES:
              case WIFI_SECURITY_WPA2_AES:
                ext->alg = IW_ENCODE_ALG_CCMP;
                break;
              default:
                break;
            }
          }
          else
            {
              wlerr("ERROR: Failed to get link status=%d\n", rtn);
            }
        }
    }

#ifdef CONFIG_DEBUG_WIRELESS_INFO
  memcpy(buf, pdata, len);
  buf[len] = 0;
  wlinfo("INFO: Wi-Fi station password=%s len=%d\n", buf, len);
#endif

  return OK;
}

/****************************************************************************
* Name: beken_wlan_sta_mode
*
* Description:
*   Set/Get Wi-Fi Station mode code.
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*   set - true: set data; false: get data
*
* Returned Value:
*   OK on success.
*
****************************************************************************/

static int beken_wlan_sta_mode(FAR struct netdev_lowerhalf_s *dev,
                           FAR struct iwreq *iwr, bool set)
{
  if (set == false)
   {
     iwr->u.mode = IW_MODE_INFRA;
   }
  memset(&g_sta_config, 0x0, sizeof(wifi_sta_config_t));
  return OK;
}

/****************************************************************************
* Name: beken_wlan_sta_auth
*
* Description:
*   Set/Get station authentication mode params.
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*   set - true: set data; false: get data
*
* Returned Value:
*   OK on success.
*
****************************************************************************/

static int beken_wlan_sta_auth(FAR struct netdev_lowerhalf_s *dev,
                           FAR struct iwreq *iwr, bool set)
{
  wlinfo("%s", __func__);
  return OK;
}

/****************************************************************************
* Name: beken_wlan_sta_freq
*
* Description:
*   Set/Get station frequency.
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*   set - true: set data; false: get data
*
* Returned Value:
*   OK on success
*
****************************************************************************/

static int beken_wlan_sta_freq(FAR struct netdev_lowerhalf_s *dev,
                           FAR struct iwreq *iwr, bool set)
{
  wifi_link_status_t link_status;
  bk_err_t rtn;

  if (set)
    {
      return -ENOSYS;
    }

  memset(&link_status, 0x0, sizeof(link_status));
  rtn = bk_wifi_sta_get_link_status(&link_status);
  if (rtn == BK_OK)
    {
      iwr->u.freq.flags = IW_FREQ_FIXED;
      iwr->u.freq.e     = 0;
      iwr->u.freq.m     = 2412 + 5 * (link_status.channel - 1);
    }
  else
    {
      wlerr("ERROR: Failed to get link status=%d\n", rtn);
    }

  return OK;
}

/****************************************************************************
* Name: beken_wlan_sta_bitrate
*
* Description:
*   Set/Get default bit rate (Mbps).
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*   set - true: set data; false: get data
*
* Returned Value:
*   OK on success (Not implemented.ToDo)
*
****************************************************************************/

static int beken_wlan_sta_bitrate(FAR struct netdev_lowerhalf_s *dev,
                              FAR struct iwreq *iwr, bool set)
{
  wlinfo("%s", __func__);
  return OK;
}

/****************************************************************************
* Name: beken_wlan_sta_txpower
*
* Description:
*   Set/Get  transmit power (dBm).
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*   set - true: set data; false: get data
*
* Returned Value:
*   OK on success (Not implemented.ToDo)
*
****************************************************************************/

static int beken_wlan_sta_txpower(FAR struct netdev_lowerhalf_s *dev,
                              FAR struct iwreq *iwr, bool set)
{
  wlinfo("%s", __func__);
  return OK;
}

/****************************************************************************
* Name: beken_wlan_sta_country
*
* Description:
*   Configure country info.
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*   set - true: set data; false: get data
*
* Returned Value:
*   OK on success.Negated errno returned on failure.
*
****************************************************************************/

static int beken_wlan_sta_country(FAR struct netdev_lowerhalf_s *dev,
                              FAR struct iwreq *iwr, bool set)
{
  int ret;
  char *country_code;
  wifi_country_t country;

  if (set)
    {
      memset(&country, 0x00, sizeof(wifi_country_t));
      country.schan  = 1;
      country.policy = WIFI_COUNTRY_POLICY_MANUAL;

      country_code = (char *)iwr->u.data.pointer;
      if (strlen(country_code) != 2)
        {
          wlerr("ERROR: Invalid input arguments\n");
          return -EINVAL;
        }

      if (strncmp(country_code, "US", 3) == 0 ||
          strncmp(country_code, "CA", 3) == 0 ||
          strncmp(country_code, "CO", 3) == 0 ||
          strncmp(country_code, "DO", 3) == 0 ||
          strncmp(country_code, "GT", 3) == 0 ||
          strncmp(country_code, "MX", 3) == 0 ||
          strncmp(country_code, "PA", 3) == 0 ||
          strncmp(country_code, "PR", 3) == 0 ||
          strncmp(country_code, "TW", 3) == 0 ||
          strncmp(country_code, "UZ", 3) == 0)
        {
          country.nchan  = 11;
        }
      else if(strncmp(country_code, "JP", 3) == 0)
        {
          country.nchan  = 14;
        }
      else
        {
          country.nchan  = 13;
        }

      memcpy(country.cc, country_code, 2);
      ret = bk_wifi_set_country(&country);
      if (ret)
        {
          wlerr("ERROR: Failed to  Configure country ret=%d\n", ret);
          return wifi_errno_trans(ret);
        }
    }
  else
    {
      memset(&country, 0x00, sizeof(wifi_country_t));
      bk_wifi_get_country(&country);
      iwr->u.data.length = strlen(country.cc);
      memcpy(iwr->u.data.pointer, country.cc, iwr->u.data.length);
    }

  return OK;
}

/****************************************************************************
* Name: beken_wlan_sta_sensitivity
*
* Description:
*   Get Wi-Fi sensitivity (dBm).
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*   set - true: set data; false: get data
*
* Returned Value:
*   OK on success (positive non-zero values are cmd-specific)
*   Negated errno returned on failure.
*
****************************************************************************/

static int beken_wlan_sta_sensitivity(FAR struct netdev_lowerhalf_s *dev,
                                  FAR struct iwreq *iwr, bool set)
{
  wifi_link_status_t link_status;
  bk_err_t rtn;

  if (set)
    {
      return -ENOSYS;
    }

  memset(&link_status, 0x0, sizeof(link_status));
  rtn = bk_wifi_sta_get_link_status(&link_status);
  if (rtn == BK_OK)
    {
      iwr->u.sens.value = -(link_status.rssi);
    }
  else
    {
      wlerr("ERROR: Failed to get link status=%d\n", rtn);
    }

  return OK;
}

/****************************************************************************
* Name: build_scan_results
*
* Description:
*   Format the scan data to respond to wapi command
*
* Input Parameters:
*   scan_req - The private structure to keep the data buffer
*   info     - The scan AP data
*
* Returned Value:
*   OK on success.Negated errno returned on failure.
*
****************************************************************************/

static int build_scan_results(FAR struct iw_scan_result_s *scan_req,
                             wifi_scan_ap_info_t *info)
{
  int need_len;
  FAR char *pointer;
  FAR struct iw_event *iwe;


  need_len = IW_EV_LEN(ap_addr) + IW_EV_LEN(qual) +
             IW_EV_LEN(freq) + IW_EV_LEN(data) + IW_EV_LEN(essid);

  if (scan_req->cur_len + need_len > scan_req->total_len)
    {
      scan_req->cur_len += need_len;
      return -E2BIG;
    }

  /* Copy scan result */

  pointer = scan_req->buf + scan_req->cur_len;

  /* 1.Copy BSSID */

  iwe                      = (FAR struct iw_event *)pointer;
  iwe->cmd                 = SIOCGIWAP;
  iwe->u.ap_addr.sa_family = ARPHRD_ETHER;
  memcpy(&iwe->u.ap_addr.sa_data, info->bssid, IFHWADDRLEN);
  iwe->len                 = IW_EV_LEN(ap_addr);
  pointer                 += iwe->len;

  /* 2.Copy ESSID */

  iwe                  = (FAR struct iw_event *)pointer;
  iwe->cmd             = SIOCGIWESSID;
  iwe->u.essid.flags   = 0;
  iwe->u.essid.length  = MIN(strlen(info->ssid), 32);
  iwe->u.essid.pointer = (FAR void *)sizeof(iwe->u.essid);
  memcpy(&iwe->u.essid + 1, info->ssid, iwe->u.essid.length);
  iwe->len             = IW_EV_LEN(essid) + ((iwe->u.essid.length + 3) & ~3);
  pointer             += iwe->len;

  /* 3.Copy link quality info */

  iwe                  = (FAR struct iw_event *)pointer;
  iwe->cmd             = IWEVQUAL;
  iwe->u.qual.qual     = 0;
  iwe->u.qual.level    = info->rssi;
  iwe->u.qual.noise    = 0;
  iwe->u.qual.updated  = IW_QUAL_DBM;
  iwe->len             = IW_EV_LEN(qual);
  pointer             += iwe->len;

  /* 4.Copy AP control channel */

  iwe                  = (FAR struct iw_event *)pointer;
  iwe->cmd             = SIOCGIWFREQ;
  iwe->u.freq.e        = 0;
  iwe->u.freq.m        = info->channel;
  iwe->len             = IW_EV_LEN(freq);
  pointer             += iwe->len;

  /* 5.Copy AP encryption mode */

  iwe                  = (FAR struct iw_event *)pointer;
  iwe->cmd             = SIOCGIWENCODE;
  iwe->u.data.flags    = info->security != WIFI_SECURITY_NONE?
                         IW_ENCODE_ENABLED :
                         IW_ENCODE_DISABLED;
  iwe->u.data.length   = 0;
  iwe->u.essid.pointer = NULL;
  iwe->len             = IW_EV_LEN(data);
  pointer             += iwe->len;

  scan_req->cur_len    = pointer - scan_req->buf;
  return OK;
}


/****************************************************************************
* Name: wlan_scan_done_cb
*
* Description:
*   The callback will be called when scan done
*
* Input Parameters:
*   arg          - The event context
*   event_module - The module registered in event
*   event_id     - The Event ID
*   event_data   - The privete data

* Returned Value:
*   OK on success.
*
****************************************************************************/

static int wlan_scan_done_cb(void *arg, event_module_t event_module,
                                   int event_id, void *event_data)
{
  g_scan_done = true;
  return BK_OK;
}

/****************************************************************************
* Name: beken_wlan_sta_scan
*
* Description:
*   Trigger Wi-Fi station scan action
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*   set - true: set data; false: get data

* Returned Value:
*   OK on success.Negated errno returned on failure.
*
****************************************************************************/

static int beken_wlan_sta_scan(FAR struct netdev_lowerhalf_s *dev,
                           FAR struct iwreq *iwr, bool set)
{
  int ret = OK;
  if (set)
    {
      struct iw_scan_req* req = (struct iw_scan_req*)iwr->u.data.pointer;
      bk_event_register_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE, wlan_scan_done_cb, NULL);
      if (req->essid_len > 0)
        {
          wifi_scan_config_t scan_config;
          memset(&scan_config, 0x0, sizeof(wifi_scan_config_t));
          scan_config.scan_type = req->scan_type;
          strncpy(scan_config.ssid, (char *)req->essid, WIFI_SSID_STR_LEN);
          bk_wifi_scan_start(&scan_config);
        }
      else
        {
          bk_wifi_scan_start(NULL);
        }
        g_scan_done = false;
    }
  else
    {
      wifi_scan_result_t scan_result = {0};
      struct iw_scan_result_s scan_req;
      if (g_scan_done == false)
        {
          return -EAGAIN;
        }
      else if (iwr->u.data.length == 0)
        {
          return -E2BIG;
        }
      bk_wifi_scan_get_result(&scan_result);

      scan_req.buf       = iwr->u.data.pointer;
      scan_req.cur_len   = 0;
      scan_req.total_len = iwr->u.data.length;
      for (int i = 0; i < scan_result.ap_num; i++)
        {
          ret = build_scan_results(&scan_req, &scan_result.aps[i]);
          if (OK != ret)
            {
              wlerr("build_scan_results faild\n");
            }
        }
      iwr->u.data.length = scan_req.cur_len;
      bk_wifi_scan_free_result(&scan_result);
      g_scan_done = false;
    }

  return ret;
}

/****************************************************************************
* Name: beken_wlan_sta_range
*
* Description:
*   Get Wi-Fi station frequency range.
*
* Input Parameters:
*   dev - Reference to the NuttX driver state structure
*   iwr - The argument of the ioctl cmd
*
* Returned Value:
*   OK on success.
*
****************************************************************************/

static int beken_wlan_sta_range(FAR struct netdev_lowerhalf_s *dev,
                            FAR struct iwreq *iwr)
{
  int k;
  struct iw_range *range = (struct iw_range *)iwr->u.data.pointer;

  /* default in china. */

  range->num_frequency = 13;
  for (k = 1; k <= range->num_frequency; k++)
   {
     range->freq[k - 1].i = k;
     range->freq[k - 1].e = 0;
     range->freq[k - 1].m = 2407 + 5 * k;
   }

  return OK;
}

/****************************************************************************
* Name: wlan_event_cb
*
* Description:
*   The callback will be called when Wi-Fi event happens
*
* Input Parameters:
*   arg          - The event context
*   event_module - The module registered in event
*   event_id     - The Event ID
*   event_data   - The privete data
*
* Returned Value:
*   OK on success.
*****************************************************************************/

static bk_err_t wlan_event_cb(void *arg, event_module_t event_module,
                                int event_id, void *event_data)
{
  if (event_module != EVENT_MOD_WIFI)
    {
      return BK_OK;
    }

  switch (event_id)
    {
    case EVENT_WIFI_STA_CONNECTED:
      wlinfo("sta connected\n");
      g_is_sta_connected = true;
      break;
    case EVENT_WIFI_STA_DISCONNECTED:
      wlinfo("sta disconnected\n");
      g_is_sta_connected = false;
      break;
    default:
      break;
    }
   return BK_OK;
}
/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: beken_netdriver_init
 *
 * Description:
 *   Initialize the Beken WLAN station netcard driver
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   OK on success
 *
 ****************************************************************************/

int beken_netdriver_init(void)
{
  uint8_t mac[BK_MAC_ADDR_LEN];

  g_sta_dev.quota[NETPKT_TX] = 1;
  g_sta_dev.quota[NETPKT_RX] = 1;
  g_sta_dev.ops              = &g_ops;

  /* Bind the wireless ops interfaces. */

  g_sta_dev.iw_ops = &g_sta_iw_ops;

  bk_wifi_sta_get_mac(mac);

  bk_event_register_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, wlan_event_cb, NULL);
  memcpy(g_sta_dev.netdev.d_mac.ether.ether_addr_octet, mac, BK_MAC_ADDR_LEN);
  memcpy(g_sta_dev.netdev.d_ifname, WLAN_INTERFACE_NAME, strlen(WLAN_INTERFACE_NAME));
  g_sta_dev.netdev.d_pktsize = CONFIG_NET_ETH_PKTSIZE;
  netdev_lower_register(&g_sta_dev, NET_LL_IEEE80211);

  return OK;
}
