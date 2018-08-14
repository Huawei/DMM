/*
*
* Copyright (c) 2018 Huawei Technologies Co.,Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at:
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef _COMMON_H_
#define _COMMON_H_
#include <stddef.h>             /* for size_t */
#include "stackx_common.h"
#include "netif.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

//yyq

/* define common names for structures shared between server and client */

#define MP_STACKX_INSTANCE_POLL_NAME "VppTCP_instance_poll"
#define MP_STACKX_PORT_ZONE "VppTCP_stackx_port_zone"
#define MP_STACKX_PORT_INFO "VppTCP_stackx_port_info"
#define MP_MEMPOLL_RX_NAME "VppTCP_MBUF_%u_%u_RX"
#define MP_MEMPOLL_RXMSG_NAME "VppTCP_MSG_%u_%u_RX"
#define MP_MEMPOLL_TX_NAME "VppTCP_MBUF_TX"
#define MP_MEMPOLL_SEG_NAME "VppTCP_MBUF_SEG"
#define MP_STACKX_MSG_NAME "VppTCP_msg"
#define MP_STACKX_RING_NAME "VppTCP_%u_ring"
#define MP_STACKX_LRING_NAME "VppTCP_%u_lring"

#define MP_STACKX_BIT_SET_NAME "%s_bit_set"

#define MP_STACKX_SOCKET_FREE_LIST_NAME "VppTCP_socket_list"

#define MP_NETIF_LIST_NAME "VppTCP_Netif_list"

/*
 move sharemem create from App to nstackMain
advice rename app_tx_mbuf to VppTCP_APP_TXBUF_POOL
*/
#define MP_STACKX_APP_TXBUF_POOL  "app_tx_mbuf"

#define MP_MEMPOLL_TCP_BUFF_NAME "VppTCP_MBUF_%u_TCP_BUFF"

extern int spl_snprintf (char *buffer, int buflen, const char *format, ...);

/*According to the number of network cards, the establishment of recv lring,
 *each separate, each proc_id each NIC queue independent lring
 */
static inline const char *
get_mempoll_rx_name (unsigned proc_id, unsigned nic_id)
{
  /* buffer for return value. Size calculated by %u being replaced
   * by maximum 3 digits (plus an extra byte for safety)
   * the id may reach 65535, need add more space*/
  static char buffer[sizeof (MP_MEMPOLL_RX_NAME) + 32];

  int retVal = spl_snprintf (buffer, sizeof (buffer) - 1, MP_MEMPOLL_RX_NAME, proc_id, nic_id); //???????????buffer??
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

static inline const char *
get_mempoll_rxmsg_name (unsigned proc_id, unsigned nic_id)
{
  /* buffer for return value. Size calculated by %u being replaced
   * by maximum 3 digits (plus an extra byte for safety)
   * the id may reach 65535, need add more space*/
  static char buffer[sizeof (MP_MEMPOLL_RXMSG_NAME) + 32];

  int retVal = spl_snprintf (buffer, sizeof (buffer) - 1, MP_MEMPOLL_RXMSG_NAME, proc_id, nic_id);      //???????????buffer??
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

static inline const char *
get_mempoll_ring_name (unsigned proc_id)
{
  static char buffer[sizeof (MP_STACKX_RING_NAME) + 16];

  int retVal =
    spl_snprintf (buffer, sizeof (buffer) - 1, MP_STACKX_RING_NAME, proc_id);
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

static inline const char *
get_mempoll_lring_name (unsigned proc_id)
{
  static char buffer[sizeof (MP_STACKX_LRING_NAME) + 16];

  int retVal =
    spl_snprintf (buffer, sizeof (buffer) - 1, MP_STACKX_LRING_NAME, proc_id);
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

static inline const char *
get_mempoll_msg_name ()
{
  static char buffer[sizeof (MP_STACKX_MSG_NAME) + 16];

  int retVal = spl_snprintf (buffer, sizeof (buffer) - 1, MP_STACKX_MSG_NAME);
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

static inline const char *
get_mempoll_tx_name ()
{
  /* buffer for return value. Size calculated by %u being replaced
   * by maximum 3 digits (plus an extra byte for safety) */
  static char buffer[sizeof (MP_MEMPOLL_TX_NAME) + 16];

  int retVal = spl_snprintf (buffer, sizeof (buffer) - 1, MP_MEMPOLL_TX_NAME);  //???????????buffer??
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

static inline const char *
get_mempoll_seg_name ()
{
  /* buffer for return value. Size calculated by %u being replaced
   * by maximum 3 digits (plus an extra byte for safety) */
  static char buffer[sizeof (MP_MEMPOLL_SEG_NAME) + 16];

  int retVal = spl_snprintf (buffer, sizeof (buffer) - 1, MP_MEMPOLL_SEG_NAME); //???????????buffer??
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

static inline const char *
get_memStatusBitSet_name (const char *name, unsigned int num)
{
  static char buffer[64];

  int retVal =
    spl_snprintf (buffer, sizeof (buffer) - 1, MP_STACKX_BIT_SET_NAME "%d",
                  name, num);
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

static inline const char *
get_memNameWithProc (const char *name)
{
  static char buffer[64];

  int retVal = spl_snprintf (buffer, sizeof (buffer) - 1, "%s", name);
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

static inline const char *
get_memInstancePoll_name (unsigned int type)
{
  static char buffer[sizeof (MP_STACKX_INSTANCE_POLL_NAME) + 32];
  int retVal = spl_snprintf (buffer, sizeof (buffer) - 1, "%s" "%d",
                             MP_STACKX_INSTANCE_POLL_NAME, type);
  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return buffer;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
