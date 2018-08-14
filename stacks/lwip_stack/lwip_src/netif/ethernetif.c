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

#include "spl_def.h"
#include "mem.h"
#include "stackx/spl_pbuf.h"
//#include <stackx/stats.h>
//#include "sockets.h"
#include <netinet/in.h>

#include "stackx_spl_share.h"
#include "stackx/spl_api.h"
#include "lwip/etharp.h"

#include <sc_dpdk.h>
#include "sc_dpdk.h"

#include "cpuid.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "spl_hal.h"
#include "hal_api.h"

#include "sys.h"

#define IFNAME0 'e'
#define IFNAME1 'n'

struct ethernetif
{
  struct eth_addr *ethaddr;

};

#if (DPDK_MODULE != 1)
NSTACK_STATIC void
low_level_init (struct netif *pnetif)
{
  struct ether_addr eth_addr;

  struct netifExt *pnetifExt = NULL;
  NSPOL_LOGINF (NETIF_DEBUG, "low_level_init \n");

  pnetifExt = getNetifExt (pnetif->num);
  if (NULL == pnetifExt)
    return;

  hal_get_macaddr (pnetifExt->hdl, &eth_addr);
  NSPOL_LOGINF (SC_DPDK_INFO,
                "low_level_init: Port %s, MAC : %02X:%02X:%02X:%02X:%02X:%02X",
                pnetifExt->if_name, eth_addr.addr_bytes[0],
                eth_addr.addr_bytes[1], eth_addr.addr_bytes[2],
                eth_addr.addr_bytes[3], eth_addr.addr_bytes[4],
                eth_addr.addr_bytes[5]);

  pnetif->hwaddr_len = 6;
  pnetif->hwaddr[0] = eth_addr.addr_bytes[0];   //0x00;
  pnetif->hwaddr[1] = eth_addr.addr_bytes[1];   //0x1b;
  pnetif->hwaddr[2] = eth_addr.addr_bytes[2];   //0x21;
  pnetif->hwaddr[3] = eth_addr.addr_bytes[3];   //0x6b;
  pnetif->hwaddr[4] = eth_addr.addr_bytes[4];   //0x24;
  pnetif->hwaddr[5] = eth_addr.addr_bytes[5];   //0x40;
  pnetif->mtu = SPL_FRAME_MTU;

  /* don't set SPL_NETIF_FLAG_ETHARP if this device is not an ethernet one */
  pnetif->flags =
    NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
  pnetif->flags |= NETIF_FLAG_IGMP;

}
#endif

#if (DPDK_MODULE != 1)

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to spl_netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if privatedata couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init (struct netif * pnetif)
{
  struct ethernetif *eth_netif;

  if (NULL == pnetif)
    {
      NSPOL_LOGERR ("netif=NULL");
      return ERR_VAL;
    }
  NSPOL_LOGINF (NETIF_DEBUG, "ethernetif_init \n");

  eth_netif = (struct ethernetif *) malloc (sizeof (struct ethernetif));
  if (eth_netif == NULL)
    {
      NSPOL_LOGERR ("ethernetif_init: out of memory");
      return ERR_MEM;
    }

  pnetif->state = eth_netif;
  pnetif->name[0] = IFNAME0;
  pnetif->name[1] = IFNAME1;

  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  pnetif->output = etharp_output;
  pnetif->linkoutput = spl_hal_output;

  eth_netif->ethaddr = (struct eth_addr *) &(pnetif->hwaddr[0]);

  // Add extra netif information here
  if (0 != netifExt_add (pnetif))
    {
      return ERR_VAL;
    }

  /* initialize the hardware */
  low_level_init (pnetif);
  NSPOL_LOGINF (NETIF_DEBUG,
                "ethernetif_init complete ifname [%c][%c][%d] \n",
                pnetif->name[0], pnetif->name[1], pnetif->num);

  return ERR_OK;
}

void
ethernetif_packets_input (struct netif *pstnetif)
{
  struct spl_pbuf *p = NULL;
  spl_hal_input (pstnetif, &p);

  /* no packet could be read, silently ignore this */
  if (p != NULL
      && pstnetif->input (spl_convert_spl_pbuf_to_pbuf (p),
                          pstnetif) != ERR_OK)
    {
      NSPOL_LOGERR ("netif->input failed]p=%p, netif=%p", p, pstnetif);
    }

  /* Free the spl pbuf */
  spl_pbuf_free (p);
}
#endif
//#endif /* 0 */
