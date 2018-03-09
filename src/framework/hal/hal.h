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

#ifndef _HAL_H_
#define _HAL_H_

#include <stdint.h>
#include "hal_api.h"
#include "nstack_log.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define HAL_DRV_MAX   32

#define HAL_IO_REGISTER(name, ops) \
    static __attribute__((__constructor__)) void __hal_register##name(void)  \
    {\
        hal_io_adpt_register(ops); \
    } \


#define HAL_MAX_PCI_ADDR_LEN 16

#define HAL_MAX_DRIVER_NAME_LEN 128

#define HAL_MAX_PATH_LEN        4096    //max path length on linux is 4096

#define HAL_SCRIPT_LENGTH       256

#define HAL_HDL_TO_ID(hdl) (hdl.id)

/* IO using DPDK interface */
typedef struct dpdk_if
{
  uint8_t port_id;              /**< DPDK port identifier */
  uint8_t slave_num;
  uint8_t slave_port[HAL_MAX_SLAVES_PER_BOND];

  uint32_t hw_vlan_filter:1;
  uint32_t hw_vlan_strip:1;
  uint32_t rsv30:30;

  uint32_t rx_queue_num;
  uint32_t rx_ring_size[HAL_ETH_MAX_QUEUE_NUM];
  struct rte_mempool *rx_pool[HAL_ETH_MAX_QUEUE_NUM];

  uint32_t tx_queue_num;
  uint32_t tx_ring_size[HAL_ETH_MAX_QUEUE_NUM];

  char pci_addr[HAL_MAX_PCI_ADDR_LEN];
  char nic_name[HAL_MAX_NIC_NAME_LEN];
  char driver_name[HAL_MAX_DRIVER_NAME_LEN];
} dpdk_if_t;

typedef struct netif_inst
{
  enum
  {
    NETIF_STATE_FREE = 0,
    NETIF_STATE_ACTIVE
  } state;

  hal_hdl_t hdl;

  const struct netif_ops *ops;          /**< Implementation specific methods */

  union
  {
    dpdk_if_t dpdk_if;              /**< using DPDK for IO */
  } data;

} netif_inst_t;

typedef struct netif_ops
{
  const char *name;
  int (*init_global) (int argc, char **argv);
  int (*init_local) (void);
  int (*open) (netif_inst_t * inst, const char *name);
  int (*close) (netif_inst_t * inst);
  int (*start) (netif_inst_t * inst);
  int (*stop) (netif_inst_t * inst);
  int (*bond) (netif_inst_t * inst, const char *bond_name,
               uint8_t slave_num, netif_inst_t * slave[]);
    uint32_t (*mtu) (netif_inst_t * inst);
  int (*macaddr) (netif_inst_t * inst, void *mac_addr);
  int (*capability) (netif_inst_t * inst, hal_netif_capa_t * info);
    uint16_t (*recv) (netif_inst_t * inst, uint16_t queue_id,
                      struct common_mem_mbuf ** rx_pkts, uint16_t nb_pkts);
    uint16_t (*send) (netif_inst_t * inst, uint16_t queue_id,
                      struct common_mem_mbuf ** tx_pkts, uint16_t nb_pkts);
    uint32_t (*link_status) (netif_inst_t * inst);
  int (*stats) (netif_inst_t * inst, hal_netif_stats_t * stats);
  int (*stats_reset) (netif_inst_t * inst);
  int (*config) (netif_inst_t * inst, hal_netif_config_t * conf);
  int (*mcastaddr) (netif_inst_t * inst, void *mc_addr_set,
                    void *mc_addr, uint32_t nb_mc_addr);
  int (*add_mac) (netif_inst_t * inst, void *mc_addr);
  int (*rmv_mac) (netif_inst_t * inst, void *mc_addr);
  int (*allmcast) (netif_inst_t * inst, uint8_t enable);
} netif_ops_t;

extern netif_inst_t netif_tbl[HAL_MAX_NIC_NUM];

static inline netif_inst_t *
alloc_netif_inst ()
{
  int i;
  netif_inst_t *inst;

  for (i = 0; i < HAL_MAX_NIC_NUM; ++i)
    {
      inst = &netif_tbl[i];

      if (NETIF_STATE_FREE == inst->state)
        {
          inst->state = NETIF_STATE_ACTIVE;

          inst->hdl.id = i;

          return inst;
        }
    }

  return NULL;

}

static inline netif_inst_t *
get_netif_inst (hal_hdl_t hdl)
{
  netif_inst_t *inst;

  if (unlikely (!hal_is_valid (hdl)))
    {
      NSHAL_LOGERR ("inst id is not valid]inst=%i, HAL_MAX_NIC_NUM=%d",
                    HAL_HDL_TO_ID (hdl), HAL_MAX_NIC_NUM);

      return NULL;
    }

  inst = &netif_tbl[HAL_HDL_TO_ID (hdl)];

  if (unlikely ((NETIF_STATE_ACTIVE != inst->state) || (NULL == inst->ops)))
    {
      NSHAL_LOGERR ("netif is not active]inst=%i", HAL_HDL_TO_ID (hdl));

      return NULL;
    }

  return inst;
}

int hal_snprintf (char *buffer, size_t buflen, const char *format, ...);
int hal_is_script_valid (const char *cmd);
int hal_run_script (const char *cmd, char *result_buf, size_t max_result_len);
void hal_io_adpt_register (const netif_ops_t * ops);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
