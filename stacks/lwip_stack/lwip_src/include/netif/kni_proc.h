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

#ifndef _KNI_PROC_H_
#define _KNI_PROC_H_
#include <rte_kni.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define KNI_MAX_KTHREAD 32

/* the kni is runned on the lcore from below on */
#define DEFAULT_KNI_LCORE_BASE 24

/* Structure of kni para */
struct disp_kni_para
{
  u8_t queue_id;
  u8_t port_id;
};

/*
 * Structure of port parameters
 */
struct kni_port_params
{
  uint8_t port_id;              /* Port ID */
  unsigned lcore_rx;            /* lcore ID for RX */
  unsigned lcore_tx;            /* lcore ID for TX */
  uint32_t nb_lcore_k;          /* Number of lcores for KNI multi kernel threads */
  uint32_t nb_kni;              /* Number of KNI devices to be created */
  unsigned lcore_k[KNI_MAX_KTHREAD];    /* lcore ID list for kthreads */
  struct rte_kni *kni[KNI_MAX_KTHREAD]; /* KNI context pointers */
  struct rte_mempool *kni_pktmbuf_pool;
  u8_t ip_reconfigured;
} __rte_cache_aligned;

/* Structure type for storing kni interface specific stats */
struct kni_interface_stats
{
  /* number of pkts received from NIC, and sent to KNI */
  uint64_t rx_packets;

  /* number of pkts received from NIC, but failed to send to KNI */
  uint64_t rx_dropped;

  /* number of pkts received from KNI, and sent to NIC */
  uint64_t tx_packets;

  /* number of pkts received from KNI, but failed to send to NIC */
  uint64_t tx_dropped;
};

/* External interface for initilizing KNI subsystem */
int kni_proc_init (enum rte_proc_type_t proc_type, int proc_id, u32_t pmask,
                   struct kni_port_params **kni_para);

int kni_proc_init_secondary (int proc_id, int port_id);
/* External interface for destroy KNI subsystem */
void kni_proc_free (void);

/* External interface for kni tx thread entry */
void kni_tx_thread_cycle (void *arg);

int kni_config_net (void);
void kni_handler_eth_operate_request (int port);

/* External interface for commiting packet to kni device */
void kni_dispatch_to_kernel (uint8_t port_id,
                             struct rte_mbuf *pkts_burst[], uint8_t nb_rx);

/* the lcore kni tx/rx thread run on */
unsigned kni_get_tx_lcore (uint8_t port_id);
unsigned kni_get_rx_lcore (uint8_t port_id);

void init_kni (void);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /*#ifndef _KNI_PROC_H_ */
