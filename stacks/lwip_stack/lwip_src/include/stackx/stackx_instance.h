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

#ifndef __STACKX_INSTANCE_H__
#define __STACKX_INSTANCE_H__

#include "stackx/spl_tcpip.h"
#include "netif.h"
#include "lwip/ip4_frag.h"
#include "stackx/spl_pbuf.h"
#include "arch/sys_arch.h"
#include "arch/queue.h"
#include "stackx_tx_box.h"
#include "nsfw_msg.h"
#include "stackx_app_res.h"
#include "ip_module_api.h"
#include "tcp.h"
#include "udp.h"

#define PKT_BURST 32

#define TASK_BURST 16

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define MAX_NETBUFS  1024*2     //define for C10M

#define TOTAL_MSG_QUEUE_NUM (MSG_PRIO_QUEUE_NUM+1)      /* three priority queue and one primary queue */

struct stackx_stat
{
  struct rti_queue primary_stat;        //primary box stat
  u64_t extend_member_bit;
};

struct stackx_stack
{
  struct queue primary_mbox;
  struct queue priority_mbox[MSG_PRIO_QUEUE_NUM];       //0-highest; 1-medium; 2-lowest
  //stackx_apis stackx_api;
};

struct disp_netif_list
{
  struct disp_netif_list *next;
  struct netif *netif;
};

typedef struct stackx_instance
{
  uint16_t rss_queue_id;

  mpool_handle mp_tx;
  //mring_handle mp_seg;
  mring_handle cpcb_seg;
  mring_handle lmsg_pool;

  struct stackx_stack lstack;
  struct stackx_stat lstat;     //point to p_stackx_table->lstat[i];

        /**
	 * Header of the input packet currently being processed.
	 */
  /* global variables */
  struct disp_netif_list *netif_list;
} stackx_instance;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __STACKX_INSTANCE_H__ */
