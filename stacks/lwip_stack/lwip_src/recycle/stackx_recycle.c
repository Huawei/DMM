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

#include "stackx_spl_share.h"
#include "nsfw_recycle_api.h"
#include "nstack_log.h"
#include "nsfw_msg_api.h"
#include "stackx_socket.h"
#include "stackx_spl_msg.h"
#include "stackx_app_res.h"
#include "common.h"
#include "sc_dpdk.h"
#include "nsfw_mt_config.h"
#include "spl_instance.h"

#define SS_DELAY_CLOSE_SEC 5

extern struct stackx_port_zone *p_stackx_port_zone;

/*****************************************************************************
*   Prototype    : sbr_recycle_rx_mbuf
*   Description  : iterator and free rx mbufs with pid flags, when the app
                   with certain pid is no longer exist
*   Input        : void *data
*                  void *arg
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_recycle_rx_mbuf (void *data, void *arg)
{
  u32 *recycle_flg;
  pid_t *pid = (pid_t *) arg;
  struct common_mem_mbuf *m_buf = (struct common_mem_mbuf *) data;
#ifdef HAL_LIB
#else
  recycle_flg =
    (u32 *) ((char *) (m_buf->buf_addr) + RTE_PKTMBUF_HEADROOM -
             sizeof (u32));
#endif
  if (m_buf->refcnt > 0 && *recycle_flg == *pid)
    {
      NSSBR_LOGDBG ("free rx mbuf hold by app], mbuf=%p", m_buf);
      *recycle_flg = MBUF_UNUSED;
      spl_mbuf_free (m_buf);
    }
  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_recycle_rx_pool
*   Description  : recycle rx mbufs hold by app when app crahes
*   Input        : pid_t pid
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
NSTACK_STATIC int
sbr_recycle_rx_pool (pid_t pid)
{
  static struct common_mem_mempool *rx_pool[MAX_VF_NUM * 2][MAX_THREAD_NUM] =
    { {0} };
  struct common_mem_mempool *mp;
  nsfw_mem_name lookup_mbuf_pool;
  u32 nic_id, queue_id = 0;
  int retval;
  struct common_mem_ring *ring;

  for (nic_id = 0;
       nic_id < p_stackx_port_zone->port_num && nic_id < MAX_VF_NUM * 2;
       nic_id++)
    {
      mp = rx_pool[nic_id][queue_id];
      if (mp == NULL)
        {
          retval =
            spl_snprintf (lookup_mbuf_pool.aname, NSFW_MEM_NAME_LENGTH - 1,
                          "%s", get_mempoll_rx_name (queue_id, nic_id));
          if (-1 == retval)
            {
              NSPOL_LOGERR ("spl_snprintf fail");
              break;
            }

          lookup_mbuf_pool.entype = NSFW_SHMEM;
          lookup_mbuf_pool.enowner = NSFW_PROC_MAIN;
          mp =
            (struct common_mem_mempool *)
            nsfw_mem_mbfmp_lookup (&lookup_mbuf_pool);
          if (mp == NULL)
            break;
          rx_pool[nic_id][queue_id] = mp;
        }
      if (nsfw_mem_mbuf_iterator
          (mp, 0, mp->size, sbr_recycle_rx_mbuf, (void *) &pid) < 0)
        {
          NSSBR_LOGERR ("nsfw_mem_mbuf_iterator return fail");
          return -1;
        }
      ring = (struct common_mem_ring *) (mp->pool_data);
      NSSBR_LOGINF
        ("after recycling rx pbuf hold by app]ring=%p,prod.head=%u,prod.tail=%u,"
         "cons.head=%u,cons.tail=%u.", ring, ring->prod.head, ring->prod.tail,
         ring->cons.head, ring->cons.tail);
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_recycle_rx_pool
*   Description  : recycle stackx tx mbufs hold by app when app crahes
*   Input        : pid_t pid
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
NSTACK_STATIC int
sbr_recycle_tx_pool (pid_t pid)
{
  struct common_mem_mempool *mp;
  struct common_mem_ring *ring;

  /* Try to free all the RX mbufs which are holded in stackx TX pool */
  mp = (struct common_mem_mempool *) p_def_stack_instance->mp_tx;
  if (mp == NULL)
    return -1;

  if (nsfw_mem_mbuf_iterator
      (mp, 0, mp->size, sbr_recycle_rx_mbuf, (void *) &pid) < 0)
    {
      NSSBR_LOGERR ("nsfw_mem_mbuf_iterator return fail");
      return -1;
    }
  ring = (struct common_mem_ring *) (mp->pool_data);
  NSSBR_LOGINF
    ("after recycling stackx tx pbuf hold by app]ring=%p,prod.head=%u,prod.tail=%u,"
     "cons.head=%u,cons.tail=%u.", ring, ring->prod.head, ring->prod.tail,
     ring->cons.head, ring->cons.tail);

  return 0;
}

/*****************************************************************************
*   Prototype    : ss_free_del_conn_msg
*   Description  : free msg
*   Input        : msg_delete_netconn *dmsg
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
ss_free_del_conn_msg (msg_delete_netconn * dmsg)
{
  data_com_msg *msg = (data_com_msg *) ((char *) dmsg - MAX_MSG_PARAM_SIZE);

  if (MSG_ASYN_POST == msg->param.op_type)      /* should check type for linger */
    {
      msg_free (msg);
    }
}

extern int nsep_recycle_ep (u32 pid);

/*****************************************************************************
*   Prototype    : ss_recycle_done
*   Description  : recycle done,need recycle ep and rx pool
*   Input        : pid_t pid
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
ss_recycle_done (pid_t pid)
{
  spl_free_tx_pool (pid);
  spl_recycle_msg_pool (pid);
  (void) sbr_recycle_rx_pool (pid);
  (void) sbr_recycle_tx_pool (pid);
  (void) nsep_recycle_ep (pid);
  (void) nsfw_recycle_obj_end (pid);
}

/*****************************************************************************
*   Prototype    : ss_notify_omc
*   Description  : try to notify omc
*   Input        : spl_netconn_t** conn_array
*                  u32 conn_num
*                  u8 notify_omc
*                  pid_t pid
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
ss_notify_omc (spl_netconn_t ** conn_array, u32 conn_num, u8 notify_omc,
               pid_t pid)
{
  if (notify_omc)
    {
      u32 i;
      for (i = 0; i < conn_num; ++i)
        {
          struct spl_netconn *conn = conn_array[i];
          if (ss_is_pid_exist (conn, pid))
            {
              NSSBR_LOGINF ("there are still conn at work]pid=%d", pid);
              break;
            }

          msg_delete_netconn *delay_msg = conn->recycle.delay_msg;
          if (delay_msg && (delay_msg->pid == pid))
            {
              delay_msg->notify_omc = notify_omc;
              NSSBR_LOGINF ("there are still conn at delay]pid=%d", pid);
              break;
            }
        }

      if (conn_num == i)
        {
          NSSBR_LOGINF ("recycle done,notify omc]pid=%d", pid);
          ss_recycle_done (pid);
        }
    }
}

extern void do_pbuf_free (struct spl_pbuf *buf);

static void
ss_recycle_fd_share (sbr_fd_share * fd_share)
{
  /* to free pbufs which are attached to sbr_fd_share */
  if (fd_share->recoder.head)
    {
      struct spl_pbuf *buf = fd_share->recoder.head;
      fd_share->recoder.head = NULL;
      fd_share->recoder.tail = NULL;
      fd_share->recoder.totalLen = 0;
      do_pbuf_free (buf);
    }
}

extern void nsep_recycle_epfd (void *epinfo, u32 pid);
extern void tcp_free_accept_ring (spl_netconn_t * conn);
extern void free_conn_by_spl (spl_netconn_t * conn);
extern void tcp_drop_conn (spl_netconn_t * conn);

/*****************************************************************************
*   Prototype    : ss_close_conn_now
*   Description  : close netconn now
*   Input        : spl_netconn_t *conn
*                  msg_delete_netconn *dmsg
*                  pid_t pid
*                  ss_close_conn_fun close_conn
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
ss_close_conn_now (spl_netconn_t * conn, msg_delete_netconn * dmsg, pid_t pid,
                   ss_close_conn_fun close_conn)
{
  spl_netconn_t **conn_array = conn->recycle.group->conn_array;
  u32 conn_num = conn->recycle.group->conn_num;
  u8 notify_omc = dmsg->notify_omc;

  close_conn (dmsg, 0);

  nsep_recycle_epfd (conn->epInfo, pid);
  conn->epInfo = NULL;          /*must be set to NULL */

  u32 i;
  if (conn->recycle.is_listen_conn)
    {
      /* drop the conn inside the accept ring */
      tcp_free_accept_ring (conn);

      /* app coredump and accept_from not changed, need recyle */
      for (i = 0; i < conn_num; ++i)
        {
          struct spl_netconn *accept_conn = conn_array[i];
          if ((accept_conn->recycle.accept_from == conn)
              && ss_is_pid_array_empty (accept_conn))
            {
              NSSBR_LOGINF
                ("recycle lost conn]listen_conn=%p,listen_private_data=%p,accept_conn=%p,accept_private_data=%p,pid=%d",
                 conn, conn->private_data, accept_conn,
                 accept_conn->private_data, pid);
              data_com_msg *msg =
                (data_com_msg *) ((char *) dmsg - MAX_MSG_PARAM_SIZE);
              msg->param.receiver = ss_get_recv_obj (accept_conn);
              dmsg->buf = NULL;
              dmsg->time_started = sys_now ();
              dmsg->shut = 0;
              dmsg->conn = accept_conn;
              close_conn (dmsg, 0);

              nsep_recycle_epfd (accept_conn->epInfo, pid);
              accept_conn->epInfo = NULL;       /*must be set to NULL */

              /* lost conn need drop first, can't just free conn */
              tcp_drop_conn (accept_conn);
            }
        }
    }

  if (SS_DELAY_STOPPED == conn->recycle.delay_flag)
    {
      sbr_fd_share *fd_share =
        (sbr_fd_share *) ((char *) conn + SS_NETCONN_SIZE);
      ss_recycle_fd_share (fd_share);
      free_conn_by_spl (conn);
    }

  ss_free_del_conn_msg (dmsg);
  ss_notify_omc (conn_array, conn_num, notify_omc, pid);
}

/*****************************************************************************
*   Prototype    : ss_close_conn_delay
*   Description  : delay to close conn
*   Input        : spl_netconn_t *conn
*                  pid_t pid
*                  msg_delete_netconn *dmsg
*                  ss_close_conn_fun close_conn
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
ss_close_conn_delay (spl_netconn_t * conn, pid_t pid,
                     msg_delete_netconn * dmsg, ss_close_conn_fun close_conn)
{
  NSSBR_LOGINF
    ("ref > 0 and pid array is empty, start delay closing conn]conn=%p,pid=%d,private_data=%p",
     conn, pid, conn->private_data);
  close_conn (dmsg, SS_DELAY_CLOSE_SEC);
}

/*****************************************************************************
*   Prototype    : ss_process_delay_up
*   Description  : delay is up
*   Input        : spl_netconn_t *conn
*                  pid_t pid
*                  msg_delete_netconn *dmsg
*                  ss_close_conn_fun close_conn
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
ss_process_delay_up (spl_netconn_t * conn, pid_t pid,
                     msg_delete_netconn * dmsg, ss_close_conn_fun close_conn)
{
  spl_netconn_t **conn_array = conn->recycle.group->conn_array;
  u32 conn_num = conn->recycle.group->conn_num;
  u8 notify_omc = dmsg->notify_omc;

  if (SS_DELAY_STARTED == conn->recycle.delay_flag)
    {
      if (ss_is_pid_array_empty (conn))
        {
          NSSBR_LOGINF
            ("delay time is up,close conn now]conn=%p,pid=%d,private_data=%p",
             conn, pid, conn->private_data);
          conn->recycle.delay_flag = SS_DELAY_STOPPED;
          conn->recycle.delay_msg = NULL;
          ss_close_conn_now (conn, dmsg, pid, close_conn);
          return;
        }
      else
        {
          NSSBR_LOGINF
            ("stop delay closing conn,conn still working]conn=%p,pid=%d,private_data=%p",
             conn, pid, conn->private_data);
          conn->recycle.delay_flag = SS_DELAY_STOPPED;
          conn->recycle.delay_msg = NULL;
          ss_free_del_conn_msg (dmsg);
          ss_notify_omc (conn_array, conn_num, notify_omc, pid);
          return;
        }
    }
  else if (SS_DELAY_AGAIN == conn->recycle.delay_flag)
    {
      if (ss_is_pid_array_empty (conn))
        {
          NSSBR_LOGINF
            ("delay time is up,but need delay again]conn=%p,pid=%d,private_data=%p",
             conn, pid, conn->private_data);
          conn->recycle.delay_flag = SS_DELAY_STARTED;
          ss_close_conn_delay (conn, pid, dmsg, close_conn);
          return;
        }
      else
        {
          NSSBR_LOGINF
            ("stop delay closing conn,conn still working]conn=%p,pid=%d,private_data=%p",
             conn, pid, conn->private_data);
          conn->recycle.delay_flag = SS_DELAY_STOPPED;
          conn->recycle.delay_msg = NULL;
          ss_free_del_conn_msg (dmsg);
          ss_notify_omc (conn_array, conn_num, notify_omc, pid);
          return;
        }
    }
  else if (SS_DELAY_STOPPING == conn->recycle.delay_flag)
    {
      NSSBR_LOGINF
        ("the conn has been closed,free conn]conn=%p,pid=%d,private_data=%p",
         conn, pid, conn->private_data);
      conn->recycle.delay_flag = SS_DELAY_STOPPED;
      conn->recycle.delay_msg = NULL;
      free_conn_by_spl (conn);
      ss_free_del_conn_msg (dmsg);
      ss_notify_omc (conn_array, conn_num, notify_omc, pid);
      return;
    }
  else
    {
      NSSBR_LOGERR ("this can not happen]conn=%p,pid=%d,private_data=%p",
                    conn, pid, conn->private_data);
    }
}

/*****************************************************************************
*   Prototype    : ss_recycle_conn
*   Description  : recycle conn
*   Input        : void *close_data
*                  ss_close_conn_fun close_conn
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
ss_recycle_conn (void *close_data, ss_close_conn_fun close_conn)
{
  msg_delete_netconn *dmsg = (msg_delete_netconn *) close_data;
  spl_netconn_t *conn = dmsg->conn;
  pid_t pid = dmsg->pid;
  u8 notify_omc = dmsg->notify_omc;
  struct spl_netconn **conn_array = conn->recycle.group->conn_array;
  u32 conn_num = conn->recycle.group->conn_num;

  int ret = ss_del_pid (conn, pid);
  if (0 == ret)
    {
      i32 ref = ss_dec_fork_ref (conn);
      if (0 == ref)
        {
          if (conn->recycle.delay_flag != SS_DELAY_STOPPED)
            {
              conn->recycle.delay_flag = SS_DELAY_STOPPING;
              NSSBR_LOGINF
                ("stop delay closing conn,close conn now]conn=%p,pid=%d,private_data=%p",
                 conn, pid, conn->private_data);
            }
          else
            {
              NSSBR_LOGINF ("close conn now]conn=%p,pid=%d,private_data=%p",
                            conn, pid, conn->private_data);
            }

          ss_close_conn_now (conn, dmsg, pid, close_conn);
          return 0;
        }
      else
        {
          if (ss_is_pid_array_empty (conn))
            {
              if (SS_DELAY_STOPPED == conn->recycle.delay_flag) /* only start one delay */
                {
                  conn->recycle.delay_flag = SS_DELAY_STARTED;
                  conn->recycle.delay_msg = close_data;
                  ss_close_conn_delay (conn, pid, dmsg, close_conn);
                  return 0;
                }
              else if (SS_DELAY_STARTED == conn->recycle.delay_flag)
                {
                  conn->recycle.delay_flag = SS_DELAY_AGAIN;
                  NSSBR_LOGINF
                    ("ref > 0 and pid array is empty, delay again]conn=%p,pid=%d,private_data=%p",
                     conn, pid, conn->private_data);
                }
            }
        }
    }
  else
    {
      if (conn->recycle.delay_msg && (conn->recycle.delay_msg == close_data))   /* only the stater can process */
        {
          ss_process_delay_up (conn, pid, dmsg, close_conn);
          return 0;
        }
    }

  NSSBR_LOGINF ("go to notify omc]conn=%p,pid=%d,private_data=%p", conn, pid,
                conn->private_data);
  ss_free_del_conn_msg (dmsg);
  ss_notify_omc (conn_array, conn_num, notify_omc, pid);
  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_handle_recycle_conn
*   Description  : post msg to spl
*   Input        : spl_netconn_t* conn
*                  pid_t pid
*                  u8 notify_omc
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_handle_recycle_conn (spl_netconn_t * conn, pid_t pid, u8 notify_omc)
{
  data_com_msg *m = msg_malloc (ss_get_msg_pool (conn));

  if (!m)
    {
      NSSBR_LOGERR ("malloc msg failed]conn=%p,pid=%d,private_data=%p", conn,
                    pid, conn->private_data);
      return -1;
    }

  NSSBR_LOGINF ("recycle conn]conn=%p,pid=%d,private_data=%p", conn, pid,
                conn->private_data);

  m->param.module_type = MSG_MODULE_SBR;
  m->param.major_type = SPL_TCPIP_NEW_MSG_API;
  m->param.minor_type = SPL_API_DO_DELCON;
  m->param.err = 0;
  m->param.op_type = MSG_ASYN_POST;
  sys_sem_init (&m->param.op_completed);
  m->param.receiver = ss_get_recv_obj (conn);
  m->param.extend_member_bit = 0;

  msg_delete_netconn *p = (msg_delete_netconn *) m->buffer;
  p->extend_member_bit = 0;
  p->time_started = sys_now ();
  p->shut = 0;
  p->pid = pid;
  p->conn = conn;
  p->notify_omc = notify_omc;
  p->msg_box_ref = SPL_MSG_BOX_NUM;
  p->buf = NULL;

  /* to ensure that the last deal with SPL_API_DO_DELCON message */
  int i;
  for (i = 0; i < SPL_MSG_BOX_NUM; ++i)
    {
      if (msg_post_with_lock_rel
          (m,
           ss_get_instance_msg_box (ss_get_bind_thread_index (conn), i)) < 0)
        {
          msg_free (m);
          NSSBR_LOGERR ("post msg failed]conn=%p,pid=%d,private_data=%p",
                        conn, pid, conn->private_data);
          return -1;
        }
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_recycle_fd_share
*   Description  : recycle sbr_fd_share
*   Input        : sbr_fd_share* fd_share
*                  pid_t pid
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
sbr_recycle_fd_share (sbr_fd_share * fd_share, pid_t pid)
{
  if (fd_share->common_lock.locked == pid)
    {
      common_spinlock_unlock (&fd_share->common_lock);
    }

  if (fd_share->recv_lock.locked == pid)
    {
      common_spinlock_unlock (&fd_share->recv_lock);
    }
}

/*****************************************************************************
*   Prototype    : sbr_recycle_conn
*   Description  : recycle api,called by recycle module
*   Input        : u32 exit_pid
*                  void *pdata
*                  u16 rec_type
*   Output       : None
*   Return Value : nsfw_rcc_stat
*   Calls        :
*   Called By    :
*
*****************************************************************************/
nsfw_rcc_stat
sbr_recycle_conn (u32 exit_pid, void *pdata, u16 rec_type)
{
  NSSBR_LOGINF ("start recycle]pid=%d", exit_pid);

  if (0 == exit_pid)
    {
      NSSBR_LOGERR ("pid is not ok]pid=%d", exit_pid);
      return NSFW_RCC_CONTINUE;
    }

  spl_netconn_t **conn_array = spl_get_conn_array (exit_pid);
  if (!conn_array)
    {
      NSSBR_LOGERR ("conn_array is NULL]pid=%d", exit_pid);
      return NSFW_RCC_CONTINUE;
    }

  u32 num = spl_get_conn_num ();
  spl_netconn_t *conn;
  sbr_fd_share *fd_share;
  u32 i;
  for (i = 0; i < num; ++i)
    {
      conn = conn_array[i];
      if (ss_is_pid_exist (conn, exit_pid))
        {
          fd_share = (sbr_fd_share *) ((char *) conn + SS_NETCONN_SIZE);
          sbr_recycle_fd_share (fd_share, exit_pid);
          sbr_handle_recycle_conn (conn, exit_pid, FALSE);
        }
    }

  sbr_handle_recycle_conn (conn_array[0], exit_pid, TRUE);
  return NSFW_RCC_SUSPEND;
}

REGIST_RECYCLE_OBJ_FUN (NSFW_REC_SBR_SOCKET, sbr_recycle_conn)
