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

#include "stackx_res_mgr.h"
#include "stackx_common.h"
#include "nstack_securec.h"
#include "nsfw_msg.h"
#include "stackx_common.h"
#include "nsfw_mgr_com_api.h"
#include "stackx_cfg.h"
#include "nsfw_maintain_api.h"
//#include "stackx_dfx_api.h"
#include "stackx_app_res.h"

sbr_share_group g_share_group = { 0 };

#define SLOW_SLEEP_TIME 500000

NSTACK_STATIC inline void
sbr_reset_fd_share (sbr_fd_share * fd_share)
{
  common_mem_spinlock_init (&fd_share->recv_lock);
  common_mem_spinlock_init (&fd_share->common_lock);
  fd_share->err = 0;
  fd_share->lastoffset = 0;
  fd_share->lastdata = NULL;
  fd_share->recoder.head = NULL;
  fd_share->recoder.tail = NULL;
  fd_share->recoder.totalLen = 0;
  fd_share->recv_timeout = 0;
  fd_share->send_timeout = 0;
  fd_share->rcvlowat = 1;
  fd_share->block_polling_time = SLOW_SLEEP_TIME;
}

/*****************************************************************************
*   Prototype    : sbr_init_tx_pool
*   Description  : get tx buf pool
*   Input        : None
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
NSTACK_STATIC int
sbr_init_tx_pool ()
{
  mpool_handle pool[1];
  pool[0] = NULL;

  (void) sbr_malloc_tx_pool (get_sys_pid (), pool, 1);
  if (pool[0])
    {
      g_share_group.tx_pool = pool[0];
      return 0;
    }

  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_init_app_res
*   Description  : get msg, conn pool
*   Input        : None
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
NSTACK_STATIC int
sbr_init_app_res ()
{
  g_share_group.conn_pool = sbr_get_instance_conn_pool (0);
  if (!g_share_group.conn_pool)
    {
      return -1;
    }

  return 0;
}

/*=========== get share config for app =============*/
NSTACK_STATIC inline int
get_share_config ()
{
  static nsfw_mem_name g_cfg_mem_info =
    { NSFW_SHMEM, NSFW_PROC_MAIN, NSTACK_SHARE_CONFIG };

  mzone_handle base_cfg_mem = nsfw_mem_zone_lookup (&g_cfg_mem_info);
  if (NULL == base_cfg_mem)
    {
      NSSOC_LOGERR ("get config share mem failed.");
      return -1;
    }

  if (get_share_cfg_from_mem (base_cfg_mem) < 0)
    {
      NSSOC_LOGERR ("get share config failed.");
      return -1;
    }

  NSSOC_LOGDBG ("get share config success.");
  return 0;
}

int
nstack_set_share_config ()
{
  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_init_stackx
*   Description  : init stackx res
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_init_stackx ()
{
  sbr_init_cfg ();
  if (get_share_config () < 0)
    {
      NSSBR_LOGERR ("get_share_config  failed");
      return -1;
    }

  if (sbr_attach_group_array () != 0)
    {
      NSSBR_LOGERR ("sbr_attach_group_array failed");
      return -1;
    }

  NSSBR_LOGDBG ("sbr_attach_group_array ok");

  if (sbr_init_tx_pool () != 0)
    {
      NSSBR_LOGERR ("init tx pool failed");
      return -1;
    }

  NSSBR_LOGDBG ("init tx pool ok");

  if (sbr_init_app_res () != 0)
    {
      NSSBR_LOGERR ("sbr_init_app_res failed");
      return -1;
    }

  NSSBR_LOGDBG ("sbr_init_app_res ok");
  NSSBR_LOGDBG ("sbr_init_stackx ok");
  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_fork_stackx
*   Description  : init stackx res
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_fork_stackx ()
{

  if (sbr_attach_group_array () != 0)
    {
      NSSBR_LOGERR ("sbr_attach_group_array failed");
      return -1;
    }

  NSSBR_LOGDBG ("sbr_attach_group_array ok");

  if (sbr_init_tx_pool () != 0)
    {
      NSSBR_LOGERR ("init tx pool failed");
      return -1;
    }

  NSSBR_LOGDBG ("init tx pool ok");
  NSSBR_LOGDBG ("sbr_fork_stackx ok");
  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_malloc_conn_for_sk
*   Description  : malloc netconn for sk,need add pid
*   Input        : sbr_socket_t* sk
*                  netconn_type_t type
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_malloc_conn_for_sk (sbr_socket_t * sk, spl_netconn_type_t type)
{
  spl_netconn_t *conn = ss_malloc_conn_app (g_share_group.conn_pool, type);

  if (!conn)
    {
      NSSBR_LOGERR ("malloc conn failed]fd=%d", sk->fd);
      sbr_set_errno (ENOBUFS);
      return -1;
    }

  NSSBR_LOGINF ("malloc conn ok]fd=%d,conn=%p", sk->fd, conn);

  u16 thread_index = 0;
  ss_set_bind_thread_index (conn, thread_index);
  ss_set_msg_box (conn, ss_get_instance_msg_box (thread_index, 0));

  sbr_fd_share *fd_share = (sbr_fd_share *) ((char *) conn + SS_NETCONN_SIZE);
  sbr_reset_fd_share (fd_share);

  sk->stack_obj = (void *) conn;
  sk->sk_obj = (void *) fd_share;
  return 0;
}

int
sbr_init_conn_for_accept (sbr_socket_t * sk, spl_netconn_t * conn)
{
  if (!conn)
    {
      sbr_set_sk_errno (sk, ENOBUFS);
      return -1;
    }

  NSSBR_LOGINF ("accept conn ok]fd=%d,conn=%p,private_data=%p", sk->fd, conn,
                conn->private_data);

  if (ss_add_pid (conn, get_sys_pid ()) < 0)
    {
      NSSBR_LOGERR ("ss_add_pid failed]fd=%d", sk->fd);
    }

  ss_set_accept_from (conn, NULL);      /* need clear flag */

  sbr_fd_share *fd_share = (sbr_fd_share *) ((char *) conn + SS_NETCONN_SIZE);
  sbr_reset_fd_share (fd_share);

  sk->stack_obj = (void *) conn;
  sk->sk_obj = (void *) fd_share;

  return 0;
}

void
sbr_free_conn_from_sk (sbr_socket_t * sk)
{
  ss_free_conn (sbr_get_conn (sk));
  sk->stack_obj = NULL;
  sk->sk_obj = NULL;
  NSSBR_LOGDBG ("free conn ok]fd=%d", sk->fd);
}
