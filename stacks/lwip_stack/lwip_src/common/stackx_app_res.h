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

#ifndef STACKX_APP_RES_H
#define STACKX_APP_RES_H
#include "types.h"
#include "nsfw_mem_api.h"
#include "stackxopts.h"
#include "stackx_spl_share.h"
#include "nsfw_mt_config.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define SPL_RES_GROUP_ARRAY "spl_res_group_array"

#define SPL_APP_RES "spl_app_res"
#define SPL_APP_RES_GROUP_NAME "spl_app_res_group"
#define SPL_TX_POOL_NAME "spl_tx_pool"
#define SPL_CONN_POOL_NAME "spl_conn_pool"
#define SPL_CONN_ARRAY_NAME "spl_conn_array"
#define SPL_DFX_ARRAY_NAME "spl_dfx_array"
#define SPL_MSG_POOL_NAME "spl_msg_pool"
#define SPL_RECYCLE_GROUP "spl_recycle_group"
#define SPL_RECV_RING_POOL_NAME "spl_recv_ring_pool"
#define SPL_RECV_RING_ARRAY_NAME "spl_recv_array"

#define SBR_TX_POOL_NUM APP_POOL_NUM
#define SBR_TX_POOL_MBUF_NUM TX_MBUF_POOL_SIZE
#define SBR_TX_MSG_NUM (SBR_TX_POOL_NUM * SBR_TX_POOL_MBUF_NUM)

#define SBR_TX_POOL_ARRAY_NAME "sbr_tx_pool_array"
#define SBR_TX_POOL_NAME "sbr_tx_pool"
#define SBR_TX_MSG_ARRAY_NAME "sbr_tx_msg_array"

#define SPL_MAX_MSG_NUM (MBOX_RING_SIZE*8*MAX_THREAD_NUM)

typedef struct
{
  PRIMARY_ADDR mring_handle msg_pool;
  PRIMARY_ADDR mring_handle conn_pool;
  PRIMARY_ADDR spl_netconn_t **conn_array;
  PRIMARY_ADDR mring_handle recv_ring_pool;
  PRIMARY_ADDR mpool_handle tx_pool_array[SBR_TX_POOL_NUM];
  PRIMARY_ADDR mring_handle mbox_array[SPL_MSG_BOX_NUM];
  i64 extend_member_bit;
} spl_app_res_group;

typedef struct
{
  u32 pid_array[SBR_TX_POOL_NUM];
  PRIMARY_ADDR spl_app_res_group *res_group[MAX_THREAD_NUM];
  u16 thread_num;
} spl_app_res_group_array;

extern spl_app_res_group *g_spl_app_res_group;
extern spl_app_res_group_array *g_res_group_array;

/* call it main thread */
int spl_init_group_array ();

/* call these in tcpip thread */
int spl_init_app_res ();
spl_netconn_t **spl_get_conn_array (u32 pid);
mring_handle spl_get_conn_pool ();
mring_handle spl_get_msg_pool ();
void spl_recycle_msg_pool (u32 pid);
u32 spl_get_conn_num ();
void spl_free_tx_pool (u32 pid);
int spl_add_mbox (mring_handle mbox_array[], u32 array_size);
int spl_add_instance_res_group (u32 thread_index, spl_app_res_group * group);

/* call these in app */
int sbr_malloc_tx_pool (u32 pid, mpool_handle pool[], u32 pool_num);
int sbr_attach_group_array ();
mring_handle sbr_get_instance_conn_pool (u32 thread_index);

/* call these in app and spl */
mring_handle *ss_get_instance_msg_box (u16 thread_index, u16 index);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
