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

#include "stackx_spl_msg.h"
#include "stackx_spl_share.h"
#include "nstack_log.h"
#include "nsfw_msg_api.h"
#include "nsfw_recycle_api.h"

/*****************************************************************************
*   Prototype    : ss_reset_conn_recycle
*   Description  : reset conn recycle
*   Input        : netconn_recycle* recycle
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
ss_reset_conn_recycle (netconn_recycle * recycle)
{
  recycle->accept_from = NULL;
  recycle->is_listen_conn = 0;
  (void) nsfw_pidinfo_init (&recycle->pid_info);
  recycle->fork_ref = 0;
  recycle->delay_msg = NULL;
  recycle->delay_flag = SS_DELAY_STOPPED;
}

/*****************************************************************************
*   Prototype    : ss_reset_conn
*   Description  : reset conn
*   Input        : spl_netconn_t* conn
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
ss_reset_conn (spl_netconn_t * conn)
{
  conn->recv_obj = 0;
  conn->private_data = 0;
  conn->msg_box = NULL;
  conn->snd_buf = 0;
  conn->epoll_flag = 0;
  conn->recv_avail_prod = 0;
  conn->recv_avail_cons = 0;
  conn->rcvevent = 0;
  conn->state = SPL_NETCONN_NONE;
  conn->sendevent = 0;
  conn->errevent = 0;
  conn->shut_status = 0xFFFF;
  conn->flags = 0;
  conn->last_err = 0;
  conn->CanNotReceive = 0;
  conn->bind_thread_index = 0;
  conn->tcp_sndbuf = 0;
  conn->tcp_wmem_alloc_cnt = 0;
  conn->tcp_wmem_sbr_free_cnt = 0;
  conn->tcp_wmem_spl_free_cnt = 0;
  conn->mss = 0;
  conn->remote_port = 0;
  conn->remote_ip.addr = 0;
  conn->local_ip.addr = 0;
  conn->local_port = 0;
  conn->type = SPL_NETCONN_INVALID;
  conn->tcp_state = SPL_CLOSED;
  ss_reset_conn_recycle (&conn->recycle);
  conn->extend_member_bit = 0;
  conn->epInfo = NULL;
}

/*****************************************************************************
*   Prototype    : ss_init_conn
*   Description  : init conn
*   Input        : spl_netconn_t* conn
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
ss_init_conn (spl_netconn_t * conn, mring_handle pool,
              spl_netconn_type_t type)
{
  res_alloc (&conn->res_chk);

  conn->type = type;
  conn->recycle.fork_ref = 1;
  conn->recv_ring_valid = 1;
  sys_sem_init (&conn->close_completed);
  NSSBR_LOGINF ("malloc conn ok]conn=%p,pool=%p", conn, pool);
}

/*****************************************************************************
*   Prototype    : ss_malloc_conn
*   Description  : malloc conn, only used in spl
*   Input        : mring_handle pool
*                  netconn_type_t type
*   Output       : None
*   Return Value : spl_netconn_t*
*   Calls        :
*   Called By    :
*
*****************************************************************************/
spl_netconn_t *
ss_malloc_conn (mring_handle pool, spl_netconn_type_t type)
{
  spl_netconn_t *conn = NULL;

  if (nsfw_mem_ring_dequeue (pool, (void **) &conn) != 1)
    {
      NSSBR_LOGERR ("malloc conn failed");
      return NULL;
    }

  ss_init_conn (conn, pool, type);
  return conn;
}

/*****************************************************************************
*   Prototype    : ss_malloc_conn_app
*   Description  : malloc conn, only used in app
*   Input        : mring_handle pool
*                  netconn_type_t type
*   Output       : None
*   Return Value : spl_netconn_t*
*   Calls        :
*   Called By    :
*
*****************************************************************************/
spl_netconn_t *
ss_malloc_conn_app (mring_handle pool, spl_netconn_type_t type)
{
  spl_netconn_t *conn = NULL;

  if (nsfw_mem_ring_dequeue (pool, (void **) &conn) != 1)
    {
      NSSBR_LOGERR ("malloc conn failed");
      return NULL;
    }

  if (ss_add_pid (conn, get_sys_pid ()) < 0)
    {
      NSSBR_LOGERR ("ss_add_pid failed]conn=%p", conn);
    }

  ss_init_conn (conn, pool, type);
  return conn;
}

/*****************************************************************************
*   Prototype    : ss_free_conn
*   Description  : free conn
*   Input        : spl_netconn_t* conn
*                  mring_handle pool
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
ss_free_conn (spl_netconn_t * conn)
{
  ss_reset_conn (conn);

  if (res_free (&conn->res_chk))
    {
      NSFW_LOGERR ("conn refree!]conn=%p", conn);
      return;
    }

  mring_handle pool = ss_get_conn_pool (conn);
  if (nsfw_mem_ring_enqueue (pool, (void *) conn) != 1)
    {
      NSSBR_LOGERR ("nsfw_mem_ring_enqueue failed,this can not happen");
    }

  NSSBR_LOGINF ("free conn ok]conn=%p,pool=%p", conn, pool);
}
