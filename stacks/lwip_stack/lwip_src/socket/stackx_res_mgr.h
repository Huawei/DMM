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

#ifndef STACKX_RES_MGR_H
#define STACKX_RES_MGR_H
#include "stackx_socket.h"
#include "stackx_ip_tos.h"
#include "stackx_tx_box.h"
#include "stackx_app_res.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef struct
{
  mring_handle conn_pool;
  mpool_handle tx_pool;
} sbr_share_group;

extern sbr_share_group g_share_group;

static inline mpool_handle
sbr_get_tx_pool ()
{
  return g_share_group.tx_pool;
}

static inline mring_handle
sbr_get_conn_pool ()
{
  return (mring_handle) ADDR_LTOSH (g_share_group.conn_pool);
}

static inline mring_handle
sbr_get_spl_msg_box (sbr_socket_t * sk, u8 tos)
{
  return
    ss_get_instance_msg_box (ss_get_bind_thread_index (sbr_get_conn (sk)),
                             stackx_get_prio (tos));
}

int sbr_init_stackx ();
int sbr_fork_stackx ();
int sbr_malloc_conn_for_sk (sbr_socket_t * sk, spl_netconn_type_t type);
int sbr_init_conn_for_accept (sbr_socket_t * sk, spl_netconn_t * conn);
void sbr_free_conn_from_sk (sbr_socket_t * sk);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
