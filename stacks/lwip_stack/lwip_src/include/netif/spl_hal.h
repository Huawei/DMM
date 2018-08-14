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

#ifndef SPL_HAL_H_
#define SPL_HAL_H_

#include "hal_api.h"
#include "netif.h"
#include "nsfw_maintain_api.h"
#include "stackx_spl_share.h"
#include "stackx_pbuf_comm.h"
#include "netifapi.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

int spl_hal_init (int argc, char *argv[]);
int spl_hal_port_init ();

int spl_hal_stats_display (struct netif *pnetif, char *str, u32_t len,
                           char *json, u32_t json_len);

err_t spl_hal_output (struct netif *netif, struct pbuf *buf);
void spl_hal_input (struct netif *netif, struct spl_pbuf **buf);

inline u16_t spl_hal_recv (struct netif *netif, u8_t id);

int spl_hal_tx_ip_cksum_enable ();
int spl_hal_tx_udp_cksum_enable ();
int spl_hal_tx_tcp_cksum_enable ();

u32 spl_hal_is_nic_exist (const char *name);

int spl_hal_is_bond_netif (struct netif *pnetif);

static inline void
spl_do_dump (struct spl_pbuf *p, u16 direction)
{
  struct spl_pbuf *q = p;
  while (q)
    {
      ntcpdump (q->payload, q->len, direction);
      q = q->next;
    }
}

/* information of bond*/
#define MAX_BOND_PORT_NUM 4

/* information of one bond port */
struct bond_set
{
  char bond_port_name[HAL_MAX_NIC_NAME_LEN];
  char slave_ports[HAL_MAX_SLAVES_PER_BOND][HAL_MAX_NIC_NAME_LEN];
  u8_t slave_port_cnt;
};

#define NETIF_ETH_ADDR_LEN  6

/* information of all bond ports */
struct bond_ports_info
{
  u8_t cnt;
  struct bond_set ports[MAX_BOND_PORT_NUM];
};

struct ether_addr
{
  u8_t addr_bytes[NETIF_ETH_ADDR_LEN];

};

struct netif *get_netif_by_ip (unsigned int ip);
struct netif *netif_check_broadcast_addr (spl_ip_addr_t * addr);
struct netifExt *getNetifExt (u16_t id);
int netifExt_add (struct netif *netif);

int add_netif_ip (char *netif_name, unsigned int ip, unsigned int mask);
int del_netif_ip (char *netif_name, unsigned int ip);

err_t spl_netifapi_netif_add (struct netif *pnetif,
                              spl_ip_addr_t * ipaddr,
                              spl_ip_addr_t * netmask,
                              spl_ip_addr_t * gw,
                              void *state,
                              netif_init_fn init,
                              netif_input_fn input,
                              netifapi_void_fn voidfunc);
struct netif *find_netif_by_if_name (char *if_name);
void ethernetif_packets_input (struct netif *pstnetif);
err_t ethernetif_init (struct netif *pnetif);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#endif /* SPL_HAL_H_ */
