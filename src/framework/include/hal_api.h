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

#ifndef _HAL_API_H_
#define _HAL_API_H_

#include "common_mem_mbuf.h"
#include "common_mem_mempool.h"
#include "common_func.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define HAL_ETH_MAX_QUEUE_NUM    4

#define HAL_ETH_QUEUE_STAT_CNTRS 16

#define HAL_MAX_NIC_NUM 4096
COMPAT_PROTECT (HAL_MAX_NIC_NUM, 4096);

#define HAL_MAX_SLAVES_PER_BOND 2

#define HAL_MAX_NIC_NAME_LEN 256

/**
 * TX offload capabilities of a device.
 */
#define HAL_ETH_TX_OFFLOAD_IPV4_CKSUM  0x00000002
#define HAL_ETH_TX_OFFLOAD_UDP_CKSUM   0x00000004
#define HAL_ETH_TX_OFFLOAD_TCP_CKSUM   0x00000008

/**
 * Hal Instance Handler
 */
typedef struct hal_hdl
{
  int id;
} hal_hdl_t;

/**
 * Ethernet device capability
 */
typedef struct hal_netif_capa
{
  uint32_t tx_offload_capa;   /**< Device TX offload capabilities. */
} hal_netif_capa_t;

/**
 * A structure used to retrieve statistics for an Ethernet port.
 */
typedef struct hal_netif_stats
{
  uint64_t ipackets;    /**< Total no.of packets that are successfully received . */
  uint64_t opackets;    /**< Total no.of packets that are successfully transmitted .*/
  uint64_t ibytes;    /**< Total no.of bytes that are successfully received . */
  uint64_t obytes;    /**< Total no.of bytes that are successfully transmitted . */
  uint64_t imissed;    /**< Total no.of RX packets that are dropped by the HW. */
  uint64_t ierrors;    /**< Total no.of packets that are received as erroneous. */
  uint64_t oerrors;    /**< Total no.of failed transmitted packets. */
  uint64_t rx_nombuf;    /**< Total no.of RX mbuf allocation failures. */

  uint64_t q_ipackets[HAL_ETH_QUEUE_STAT_CNTRS];   /**< Total no.of queue RX packets. */
  uint64_t q_opackets[HAL_ETH_QUEUE_STAT_CNTRS];   /**< Total no.of queue TX packets. */
  uint64_t q_ibytes[HAL_ETH_QUEUE_STAT_CNTRS];       /**< Total no.of successfully received queue bytes. */
  uint64_t q_obytes[HAL_ETH_QUEUE_STAT_CNTRS];       /**< Total no.of successfully transmitted queue bytes. */
  uint64_t q_errors[HAL_ETH_QUEUE_STAT_CNTRS];       /**< Total no.of queue packets received that are dropped. */
} hal_netif_stats_t;

/**
 * Ethernet device config
 */
typedef struct hal_netif_config
{
  struct
  {
    uint32_t hw_vlan_filter:1;
    uint32_t hw_vlan_strip:1;
    uint32_t rsv30:30;
  } bit;

  struct
  {
    uint32_t queue_num;
    uint32_t ring_size[HAL_ETH_MAX_QUEUE_NUM];
    struct common_mem_mempool *ring_pool[HAL_ETH_MAX_QUEUE_NUM];
  } rx;

  struct
  {
    uint32_t queue_num;
    uint32_t ring_size[HAL_ETH_MAX_QUEUE_NUM];
  } tx;

} hal_netif_config_t;

int hal_init_global (int argc, char **argv);
int hal_init_local ();
hal_hdl_t hal_create (const char *name, hal_netif_config_t * conf);
hal_hdl_t hal_bond (const char *bond_name, uint8_t slave_num,
                    hal_hdl_t slave_hdl[]);

#define hal_is_valid(hdl) ((hdl.id >= 0) && (hdl.id < HAL_MAX_NIC_NUM))

#define hal_is_equal(hdl_left, hdl_right) (hdl_left.id == hdl_right.id)

int hal_close (hal_hdl_t hdl);
int hal_stop (hal_hdl_t hdl);
uint32_t hal_get_mtu (hal_hdl_t hdl);
void hal_get_macaddr (hal_hdl_t hdl, void *mac_addr);
void hal_get_capability (hal_hdl_t hdl, hal_netif_capa_t * info);
uint16_t hal_recv_packet (hal_hdl_t hdl, uint16_t queue_id,
                          struct common_mem_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t hal_send_packet (hal_hdl_t hdl, uint16_t queue_id,
                          struct common_mem_mbuf **tx_pkts, uint16_t nb_pkts);
uint32_t hal_link_status (hal_hdl_t hdl);
int hal_stats (hal_hdl_t hdl, hal_netif_stats_t * stats);
void hal_stats_reset (hal_hdl_t hdl);
int hal_add_mcastaddr (hal_hdl_t hdl, void *mc_addr_set,
                       void *mc_addr, uint32_t nb_mc_addr);
int hal_del_mcastaddr (hal_hdl_t hdl, void *mc_addr_set,
                       void *mc_addr, uint32_t nb_mc_addr);
void hal_set_allmulti_mode (hal_hdl_t hdl, uint8_t enable);
uint32_t hal_is_nic_exist (const char *name);
hal_hdl_t hal_get_invalid_hdl ();

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
