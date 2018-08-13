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

#include "spl_opt.h"

#if STACKX_NETIF_API

#include "nsfw_msg_api.h"
#include "netifapi.h"
#include <sys/socket.h>
//#include <netinet/in.h>

#include "stackx_spl_share.h"
#include "spl_sbr.h"
#include "stackx/spl_api.h"
#include "tcpip.h"
#include "sc_dpdk.h"
#include "spl_instance.h"
#include "spl_hal.h"
#include "spl_def.h"
#include <inet.h>

struct netifExt *gNetifExt = NULL;

/**
 * common operation for sbr message.
 *
 * @param msg the api_msg_msg describing the connection type
 */
int
do_halmsg (data_com_msg * m)
{
  NSPOL_LOGDBG (TESTSOCKET_DEBUG | STACKX_DBG_TRACE,
                "the msg is from HAL module, minor(%u)", m->param.minor_type);
  return 0;
}

static int
_do_add_netif (data_com_msg * m)
{
  NSPOL_LOGINF (NETIF_DEBUG, "_do_add_netif\n");

  m->param.err = ERR_OK;
  msg_add_netif *_m = (msg_add_netif *) m->buffer;
  _m->function (_m);
  SYNC_MSG_ACK (m);
  return 0;
}

struct netif *
find_netif_by_if_name (char *if_name)
{

  struct netifExt *netifEx = gNetifExt;
  struct netif *netif = NULL;

  while (netifEx != NULL)
    {
      if (!(strcmp (netifEx->if_name, if_name)))
        {
          for (netif = netif_list; netif != NULL; netif = netif->next)
            {
              if (netifEx->id == netif->num)
                return netif;
            }

        }
      netifEx = netifEx->next;
    }

  return NULL;
}

/*@TODO: May be moved to some other file ? Like HAL*/
struct netif *
get_netif_by_ip (unsigned int ip)
{
  struct netif *netif;

  if (ip == 0)
    {
      return NULL;
    }

  for (netif = netif_list; netif != NULL; netif = netif->next)
    {
      if (ip == netif->ip_addr.addr)
        {
          NSPOL_LOGINF (NETIF_DEBUG, "netif_find: found %x %c %c", ip,
                        netif->name[0], netif->name[1]);
          return netif;
        }
    }
  NSPOL_LOGINF (NETIF_DEBUG, "netif_find: Not found %x", ip);
  return NULL;
}

/*
@TODO:
*/
struct netif *
netif_check_broadcast_addr (spl_ip_addr_t * addr)
{
  return NULL;
}

struct netifExt *
getNetifExt (u16_t id)
{

  struct netifExt *netifEx;
  netifEx = gNetifExt;

  while (netifEx != NULL)
    {
      if (netifEx->id == id)
        {
          return netifEx;
        }
      netifEx = netifEx->next;
    }

  return NULL;
}

int
netifExt_add (struct netif *netif)
{

  /* If Netif Ext already created for it then just return sucess */
  if (getNetifExt (netif->num))
    return 0;

  struct netifExt *netifEx = malloc (sizeof (struct netifExt));
  if (!netifEx)
    {
      return -1;
    }

  if (memset (netifEx, 0, sizeof (struct netifExt)) < 0)
    {
      return -1;
    }

  NSPOL_LOGINF (NETIF_DEBUG, "netifExt_added \n");

  netifEx->id = netif->num;

  /* add this netif to the list */
  netifEx->next = gNetifExt;
  gNetifExt = netifEx;
  return 0;
}

void
do_netifapi_netif_add (msg_add_netif * pmsg)
{
  struct netif *netif = NULL;
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;

  data_com_msg *m = MSG_ENTRY (pmsg, data_com_msg, buffer);
  ipaddr.addr = pmsg->ipaddr->addr;
  netmask.addr = pmsg->netmask->addr;
  gw.addr = pmsg->gw->addr;

  NSPOL_LOGINF (NETIF_DEBUG, "do_netifapi_netif_add \n");
  netif = netif_add (pmsg->netif,
                     &ipaddr,
                     &netmask, &gw, pmsg->state, pmsg->init, pmsg->input);

  if (NULL == netif)
    {
      SET_MSG_ERR (m, ERR_IF);
    }
  else
    {

      SET_MSG_ERR (m, ERR_OK);
      NSPOL_LOGINF (NETIF_DEBUG, "netif created name %c%c%d\n",
                    netif->name[0], netif->name[1], netif->num);
      pmsg->voidfunc (pmsg->netif);
      add_disp_netif (pmsg->netif);
    }

}

err_t
spl_netifapi_netif_add (struct netif *pnetif,
                        spl_ip_addr_t * ipaddr,
                        spl_ip_addr_t * netmask,
                        spl_ip_addr_t * gw,
                        void *state,
                        netif_init_fn init,
                        netif_input_fn input, netifapi_void_fn voidfunc)
{
  msg_add_netif stmsg;

  stmsg.function = do_netifapi_netif_add;
  stmsg.netif = pnetif;
  stmsg.ipaddr = ipaddr;
  stmsg.netmask = netmask;
  stmsg.gw = gw;
  stmsg.state = state;
  stmsg.init = init;
  stmsg.input = input;
  stmsg.voidfunc = voidfunc;
  return tcpip_netif_add (&stmsg);
}

int
add_netif_ip (char *netif_name, unsigned int ip, unsigned int mask)
{
  struct netif *pnetif;
  int retval;

  if (get_netif_by_ip (ip))
    {
      NSPOL_LOGERR ("ip is exist]IP=%s", inet_ntoa (ip));
      return -1;
    }
  NSPOL_LOGINF (NETIF_DEBUG, "add_netif_ip] IP=%s", inet_ntoa (ip));

  pnetif = find_netif_by_if_name (netif_name);
  if (pnetif == NULL)
    {
      return -1;
    }

  pnetif->ip_addr.addr = ip;
  pnetif->netmask.addr = mask;

  if (ip)
    {
      retval = etharp_request (pnetif, &pnetif->ip_addr);

      if (ERR_OK != retval)
        {
          NSPOL_LOGERR ("etharp_request failed]retval=%d,netif=%p,ip=%u", retval, pnetif, pnetif->ip_addr.addr);        //inet_ntoa is not thread-safe, print u32 instead.
        }
    }

  NSPOL_LOGINF (NETIF_DEBUG, "add_netif_ip]netif_name=%s,IP=%s,mask=0x%08x",
                netif_name, inet_ntoa (ip), spl_htonl (mask));
  return 0;
}

/*lint -e438*/
int
del_netif_ip (char *netif_name, unsigned int ip)
{
  struct netif *pnetif;
  //struct netif* vnetif = NULL;
  //struct netif** ref;

  pnetif = find_netif_by_if_name (netif_name);
  if (NULL == pnetif)
    {
      return -1;
    }

  NSPOL_LOGINF (NETIF_DEBUG, "del_netif_ip] IP=%s", inet_ntoa (ip));

  pnetif->ip_addr.addr = 0;
  pnetif->netmask.addr = 0;
  return 0;
}

REGIST_MSG_MODULE_MAJOR_FUN (MSG_MODULE_HAL, SPL_TCPIP_MSG_NETIFAPI,
                             do_halmsg);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_HAL, SPL_TCPIP_MSG_NETIFAPI,
                                   NETIF_DO_ADD, _do_add_netif);

#endif /* STACKX_NETIF_API */
