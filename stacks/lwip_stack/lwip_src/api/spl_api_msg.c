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

#include "nsfw_msg.h"
#include "spl_opt.h"
#include "spl_ip_addr.h"
//#include "stackx_socket.h"
#include "cpuid.h"
//#include "sockets.h"
#include <netinet/in.h>
#include <errno.h>

#include "stackx_prot_com.h"
#include "spl_api.h"
//#include "stackx/ip.h"
#include "sc_dpdk.h"
#include "spl_sbr.h"
//#include "maintain/spl_dfx.h"
#include "stackx_tx_box.h"
#include "spl_instance.h"
#include "spl_sockets.h"

#include "nstack_dmm_adpt.h"

#include "tcp.h"
#include "udp.h"
#include "pbuf.h"
#include "tcp_priv.h"
#include "init.h"
#include "timeouts.h"

#include "stackx_pbuf_comm.h"
#include "spl_netbuf.h"
#include "spl_hal.h"
#include "sys_arch.h"
#include "tcpip.h"
#include "debug.h"
#ifdef HAL_LIB
#else
#include "rte_memcpy.h"
#endif

extern struct tcp_pcb *tcp_bound_pcbs;
extern union tcp_listen_pcbs_t tcp_listen_pcbs;
extern struct tcp_pcb *tcp_active_pcbs; /* List of all TCP PCBs that are in a
                                           state in which they accept or send
                                           data. */
extern struct tcp_pcb *tcp_tw_pcbs;     /* List of all TCP PCBs in TIME-WAIT. */
extern struct tcp_pcb **const tcp_pcb_lists[NUM_TCP_PCB_LISTS];
extern stackx_instance *p_def_stack_instance;
extern u16_t g_offSetArry[SPL_PBUF_MAX_LAYER];

#define TCP_FN_NULL     0
#define TCP_FN_DEFAULT  1
#define TCP_FN_STACKX    2
#define TCP_FN_MAX  3

struct callback_fn tcp_fn[TCP_FN_MAX] = {
  {NULL, NULL, NULL, NULL, NULL, NULL, NULL},
  {NULL, spl_tcp_recv_null, NULL, NULL, NULL, NULL},
  {spl_sent_tcp, spl_recv_tcp, spl_do_connected, spl_poll_tcp, spl_err_tcp,
   NULL, spl_accept_function},
};

// extern int nstack_get_nstack_fd_snapshot_from_proto_fd(int proto_fd);
extern void tcp_drop_conn (spl_netconn_t * conn);
extern void unlink_pcb (struct common_pcb *cpcb);
extern struct queue *get_msgbox (int tos);

#define SPL_SET_NONBLOCKING_CONNECT(conn, val) \
do { \
       if (val) \
       { \
        (conn)->flags |= SPL_NETCONN_FLAG_IN_NONBLOCKING_CONNECT; \
       } \
       else \
       { \
           (conn)->flags &= ~SPL_NETCONN_FLAG_IN_NONBLOCKING_CONNECT;  \
       } \
   } while (0)

#define SPL_IN_NONBLOCKING_CONNECT(conn) (((conn)->flags & SPL_NETCONN_FLAG_IN_NONBLOCKING_CONNECT) != 0)

extern int tcpip_thread_control;

void
spl_event_callback (struct spl_netconn *conn, enum spl_netconn_evt evt,
                    int postFlag)
{
  NSPOL_LOGDBG (SOCK_INFO, "Get event]conn=%p,evt=%d,postFlag=%d", conn, evt,
                postFlag);
  /* Get socket */
  if (!conn)
    {
      NSPOL_LOGDBG (SOCK_INFO, "conn=NULL");
      return;
    }

  switch (evt)
    {
    case SPL_NETCONN_EVT_RCVPLUS:
      __sync_fetch_and_add (&conn->rcvevent, 1);
      if ((conn->epoll_flag) && (postFlag))
        {
          nstack_event_callback (ADDR_SHTOL (conn->epInfo), EPOLLIN);
        }
      break;
    case SPL_NETCONN_EVT_RCVMINUS:     // This will never be reached
      __sync_fetch_and_sub (&conn->rcvevent, 1);
      if ((conn->epoll_flag) && (postFlag))
        {
          nstack_event_callback (ADDR_SHTOL (conn->epInfo), EPOLLIN);
        }
      break;
    case SPL_NETCONN_EVT_SENDPLUS:
      conn->sendevent = 1;
      if ((conn->epoll_flag) && (postFlag))
        {
          nstack_event_callback (ADDR_SHTOL (conn->epInfo), EPOLLOUT);
        }
      break;
    case SPL_NETCONN_EVT_SENDMINUS:
      conn->sendevent = 0;

      if ((conn->epoll_flag) && (postFlag))
        {
          nstack_event_callback (ADDR_SHTOL (conn->epInfo), EPOLLOUT);
        }
      break;
    case SPL_NETCONN_EVT_ERROR:
      conn->errevent |= EPOLLERR;
      if ((conn->epoll_flag) && (postFlag))
        {
          nstack_event_callback (ADDR_SHTOL (conn->epInfo), conn->errevent);
          conn->errevent = 0;
        }
      break;
#if 1
    case SPL_NETCONN_EVT_ACCEPT:
      __sync_fetch_and_add (&conn->rcvevent, 1);
      if ((conn->epoll_flag) && (postFlag))
        {
          nstack_event_callback (ADDR_SHTOL (conn->epInfo), EPOLLIN);
        }
      break;

    case SPL_NETCONN_EVT_HUP:
      conn->errevent |= EPOLLHUP;
      if ((conn->epoll_flag) && (postFlag))
        {
          nstack_event_callback (ADDR_SHTOL (conn->epInfo), conn->errevent);
          conn->errevent = 0;
        }
      break;
    case SPL_NETCONN_EVT_RDHUP:
      conn->errevent |= EPOLLRDHUP;
      if ((conn->epoll_flag) && (postFlag))
        {
          nstack_event_callback (ADDR_SHTOL (conn->epInfo), conn->errevent);
          conn->errevent = 0;
        }
      break;
#endif

    default:
      NSTCP_LOGERR ("unknown event]conn=%p,event=%d", conn, evt);
      return;
    }
  return;
}

u8
get_shut_op (data_com_msg * m)
{
  if (m && m->param.module_type == MSG_MODULE_SBR
      && m->param.major_type == SPL_TCPIP_NEW_MSG_API)
    {
      if (m->param.minor_type == SPL_API_DO_CLOSE)
        return ((msg_close *) (m->buffer))->shut;

      if (m->param.minor_type == SPL_API_DO_DELCON)
        return ((msg_delete_netconn *) (m->buffer))->shut;
    }

  return SPL_NETCONN_SHUT_RDWR;
}

/*
 Never using head/tail to judge whether need to do enque/deque, just do enque&deque, the return val will tell you ring is full/empty or not
 rtp_perf_ring never provid ring_full/ring_empty function;
 one more thing the rtp_ring_enque/deque result must be checked.
*/
err_t
accept_enqueue (spl_netconn_t * conn, void *p)
{
  if (conn == NULL)
    {
      NSPOL_LOGERR ("accept_enqueue: invalid input]conn=%p", conn);
      return ERR_VAL;
    }

  NSPOL_LOGDBG (NEW_RING_DEBUG, "accept_enqueue]conn=%p,p=%p", conn, p);
  mring_handle ring = conn->recv_ring;
  if (NULL == ring)
    {
      NSPOL_LOGERR ("conn=%p accept new conn=%p enqueue ring addr is null",
                    conn, p);
      return ERR_VAL;
    }

  err_t retVal = ERR_MEM;
  int enQSucCnt = 0;
  int nTryCnt = 0;
  do
    {
      enQSucCnt = nsfw_mem_ring_enqueue (ring, (void *) p);
      if (1 == enQSucCnt)
        {
          retVal = ERR_OK;
          break;
        }
      else if (0 == enQSucCnt)
        {
          sys_sleep_ns (0, 3000);
          nTryCnt++;
        }
      else
        {
          retVal = ERR_VAL;
          break;
        }
    }
  while (nTryCnt < MAX_TRY_GET_MEMORY_TIMES);

  return retVal;
}

err_t
accept_dequeue (spl_netconn_t * lconn, void **new_buf,
                u32_t timeout /*miliseconds */ )
{
  struct timespec starttm;
  struct timespec endtm;
  long timediff;
  long timediff_sec;
  u32_t timeout_sec = timeout / 1000;

#define FAST_SLEEP_TIME 10000
#define SLOW_SLEEP_TIME 500000
#define FAST_RETRY_COUNT 100
  unsigned int retry_count = 0;

  if (NULL == lconn || NULL == new_buf)
    {
      NSPOL_LOGERR ("accept_dequeue invalid input]conn=%p,newbuf=%p", lconn,
                    new_buf);
      return ERR_ARG;
    }

  if (timeout != 0)
    {
      /* Handle system time change side-effects */
      if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &starttm)))
        {
          //NSPOL_LOGERR("Failed to get time]errno=%d", errno);
        }
    }

  mring_handle ring = lconn->recv_ring;
  if (ring == NULL)
    {
      NSPOL_LOGERR ("Get rtp_perf_ring failed]conn=%p", lconn);
      return ERR_ARG;
    }

  int nDequeRt;
  int retVal;
  while (1)
    {
      if ((lconn->shut_status) && (-1 != (int) timeout))
        {
          retVal = ERR_VAL;
          break;
        }

      nDequeRt = nsfw_mem_ring_dequeue (ring, new_buf);
      if (nDequeRt == 1)
        {
          retVal = ERR_OK;
          break;
        }
      else if (nDequeRt == 0)
        {
          if ((int) timeout == -1)
            {
              retVal = ERR_WOULDBLOCK;
              break;
            }

          if (timeout != 0)
            {
              /* Handle system time change side-effects */
              if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &endtm)))
                {
                  //NSPOL_LOGERR("Failed to get time, errno = %d", errno);
                }
              timediff_sec = endtm.tv_sec - starttm.tv_sec;
              if (timediff_sec >= timeout_sec)
                {
                  timediff = endtm.tv_nsec > starttm.tv_nsec ?
                    (timediff_sec * 1000) + (endtm.tv_nsec -
                                             starttm.tv_nsec) /
                    USEC_TO_SEC : (timediff_sec * 1000) -
                    ((starttm.tv_nsec - endtm.tv_nsec) / USEC_TO_SEC);

                  /*NOTE: if user configured the timeout as say 0.5 ms, then timediff value
                     will be negetive if still 0.5 ms is not elapsed. this is intended and we should
                     not typecast to any unsigned type during this below if check */
                  if (timediff > (long) timeout)
                    {
                      retVal = ERR_TIMEOUT;
                      break;
                    }
                }
            }

          /* reduce CPU usage in blocking mode */
          if (retry_count < FAST_RETRY_COUNT)
            {
              sys_sleep_ns (0, FAST_SLEEP_TIME);
              retry_count++;
            }
          else
            {
              sys_sleep_ns (0, SLOW_SLEEP_TIME);
            }

          continue;
        }
      else
        {
          retVal = ERR_VAL;
          break;
        }
    }

  return retVal;
}

err_t
sp_enqueue (struct common_pcb * cpcb, void *p)
{
  mring_handle ring = NULL;
  spl_netconn_t *conn = NULL;
  int enQueRet = 0;

  if (cpcb == NULL || (NULL == (conn = cpcb->conn)))
    {
      NSTCP_LOGERR ("conn NULL!");
      return ERR_VAL;
    }

  NSPOL_LOGDBG (NEW_RING_DEBUG, "]conn=%p,buf=%p", cpcb->conn, p);
  ring = conn->recv_ring;
  if (NULL == ring)
    {
      NSPOL_LOGERR ("Get rtp_perf_ring failed]conn=%p.", conn);
      return ERR_CLSD;
    }

  /* clear LOOP_SEND_FLAG to indicate that this pbuf has been given to upper layer */
  struct spl_pbuf *tmpbuf = p;
  while (tmpbuf)
    {
      tmpbuf->res_chk.u8Reserve &= ~LOOP_SEND_FLAG;
      tmpbuf = tmpbuf->next;
    }

  enQueRet = nsfw_mem_ring_enqueue (ring, p);
  if (1 != enQueRet)
    {
      NS_LOG_CTRL (LOG_CTRL_RECV_QUEUE_FULL, LOGTCP, "NSTCP", NSLOG_WAR,
                   "ringvbox_enqueue buf is full]conn=%p.", conn);

      return ERR_MEM;
    }

  return ERR_OK;
}

/**
 * Receive callback function for UDP netconns.
 * Posts the packet to conn->recvmbox or deletes it on memory error.
 *
 * @see udp.h (struct udp_pcb.recv) for parameters
 */

static void
spl_recv_udp (void *arg, struct udp_pcb *pcb, struct pbuf *p,
              const ip_addr_t * ipaddr, u16_t port)
{
  struct spl_netbuf *buf;
  spl_netconn_t *conn = (spl_netconn_t *) arg;
  struct spl_pbuf *spl_pb = NULL;       //??

  if (NULL == pcb)
    {
      NSPOL_LOGERR ("recv_udp must have a pcb argument");
      pbuf_free (p);
      return;
    }

  if (NULL == arg)
    {
      NSPOL_LOGERR ("recv_udp must have an argument");
      pbuf_free (p);
      return;
    }
  /* //@TODO: malloc and Copy splbuf */
  struct common_pcb *cpcb = (struct common_pcb *) (conn->comm_pcb_data);

  u16_t proc_id = spl_get_lcore_id ();

  spl_pb = spl_pbuf_alloc_hugepage (SPL_PBUF_TRANSPORT,
                                    p->tot_len +
                                    g_offSetArry[SPL_PBUF_TRANSPORT],
                                    SPL_PBUF_HUGE, proc_id, conn);

  if (!spl_pb)
    {
      NSPOL_LOGINF (TCP_DEBUG, "spl_pbuf_alloc_hugepage Failed!!!");
      return;
    }

  pbuf_to_splpbuf_copy (spl_pb, p);
  pbuf_free (p);

  buf = (struct spl_netbuf *) ((char *) spl_pb + sizeof (struct spl_pbuf));
  buf->p = spl_pb;
  spl_ip_addr_set (&buf->addr, ipaddr);
  buf->port = port;

  err_t ret = sp_enqueue (cpcb, (void *) spl_pb);
  if (ret != ERR_OK)
    {
      NSPOL_LOGDBG (UDP_DEBUG, "mbox post failed");
      spl_netbuf_delete (buf);
      return;
    }
  else
    {
      /* Register event with callback */
      NSPOL_LOGDBG (UDP_DEBUG | LWIP_DBG_TRACE,
                    "recv a udp packet: and api_event");
      SPL_API_EVENT (cpcb->conn, SPL_NETCONN_EVT_RCVPLUS, 1);
    }
}

/**
 * Receive callback function for TCP netconns.
 * Posts the packet to conn->recvmbox, but doesn't delete it on errors.
 *
 * @see tcp.h (struct tcp_pcb.recv) for parameters and return value
 */

err_t
spl_recv_tcp (void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
  struct spl_pbuf *spl_pb = NULL;
  u32_t len;

  if (NULL == pcb)
    {
      NSTCP_LOGERR ("recv_tcp must have a pcb argument");
      pbuf_free (p);
      return ERR_ARG;
    }

  if (NULL == arg)
    {
      NSTCP_LOGERR ("must have an argument]pcb=%p", pcb);
      pbuf_free (p);
      return ERR_ARG;
    }

  spl_netconn_t *conn = (spl_netconn_t *) arg;

  /* Unlike for UDP or RAW pcbs, don't check for available space
     using recv_avail since that could break the connection
     (data is already ACKed) */
  /* don't overwrite fatal errors! */
  (void) err;
  if (p == NULL)
    {
      return ERR_OK;
    }

  len = p->tot_len;

  u16_t proc_id = spl_get_lcore_id ();

  spl_pb = spl_pbuf_alloc_hugepage (SPL_PBUF_TRANSPORT,
                                    p->tot_len +
                                    g_offSetArry[SPL_PBUF_TRANSPORT],
                                    SPL_PBUF_HUGE, proc_id, conn);

  if (!spl_pb)
    {
      NSPOL_LOGINF (TCP_DEBUG, "spl_pbuf_alloc_hugepage Failed!!!");
      return ERR_MEM;
    }

  pbuf_to_splpbuf_copy (spl_pb, p);
  pbuf_free (p);

  if (sp_enqueue ((struct common_pcb *) conn->comm_pcb_data, (void *) spl_pb)
      != ERR_OK)
    {
      NSPOL_LOGDBG (TCP_DEBUG, "sp_enqueue!=ERR_OK]conn=%p", conn);
      spl_pbuf_free (spl_pb);
      return ERR_MEM;
    }

  SPL_API_EVENT (conn, SPL_NETCONN_EVT_RCVPLUS, 1);
  conn->recv_avail_prod += len;

  return ERR_OK;
}

/**
 * Poll callback function for TCP netconns.
 * Wakes up an application thread that waits for a connection to close
 * or data to be sent. The application thread then takes the
 * appropriate action to go on.
 *
 * Signals the conn->sem.
 * netconn_close waits for conn->sem if closing failed.
 *
 * @see tcp.h (struct tcp_pcb.poll) for parameters and return value
 */
err_t
spl_poll_tcp (void *arg, struct tcp_pcb * pcb)
{
  spl_netconn_t *conn = (spl_netconn_t *) arg;
  struct tcp_pcb *tpcb = NULL;

  if (NULL == conn)
    {
      NSPOL_LOGERR ("conn==NULL");
      return ERR_VAL;
    }

  if (conn->state == SPL_NETCONN_WRITE)
    {
      (void) do_writemore (conn);

      /* do_close_internal, can release th PCB
         and connection CB. so checking NETCONN_FLAG_CHECK_WRITESPACE should not be
         done after do_close_internal. anyway this flag is used with data send.(do_writemore) */
      /* Did a nonblocking write fail before? Then check available write-space. */
      if (conn->flags & SPL_NETCONN_FLAG_CHECK_WRITESPACE)
        {
          tpcb = (struct tcp_pcb *) conn->private_data;
          /* If the queued byte- or pbuf-count drops below the configured low-water limit,
             let select mark this pcb as writable again. */
          if ((tpcb != NULL) && (tcp_sndbuf (tpcb) > TCP_SNDLOWAT) &&
              (tcp_sndqueuelen (tpcb) < TCP_SNDQUEUELOWAT))
            {
              conn->flags &= ~SPL_NETCONN_FLAG_CHECK_WRITESPACE;
              SPL_API_EVENT (conn, SPL_NETCONN_EVT_SENDPLUS, 1);
            }
        }
    }
  else if (conn->state == SPL_NETCONN_CLOSE)
    {
      (void) do_close_internal ((struct common_pcb *) conn->comm_pcb_data, 0);
    }

  /* @todo: implement connect timeout here? */
  return ERR_OK;
}

/**
 * Default receive callback that is called if the user didn't register
 * a recv callback for the pcb.
 */
err_t
spl_tcp_recv_null (void *arg, struct tcp_pcb * pcb, struct pbuf * p,
                   err_t err)
{
  LWIP_UNUSED_ARG (arg);

  if (p != NULL)
    {
      tcp_recved (pcb, p->tot_len);
      pbuf_free (p);
    }
  else if (err == ERR_OK)
    {
      return tcp_close (pcb);
    }

  return ERR_OK;
}

/**
 * Sent callback function for TCP netconns.
 * Signals the conn->sem and calls API_EVENT.
 * netconn_write waits for conn->sem if send buffer is low.
 *
 * @see tcp.h (struct tcp_pcb.sent) for parameters and return value
 */
err_t
spl_sent_tcp (void *arg, struct tcp_pcb * pcb, u16_t len)
{
  spl_netconn_t *conn = (spl_netconn_t *) arg;

  if (NULL == conn)
    {
      NSPOL_LOGERR ("conn==NULL");
      return ERR_VAL;
    }

  if (conn->state == SPL_NETCONN_WRITE)
    {
      (void) do_writemore (conn);
    }
  else if (conn->state == SPL_NETCONN_CLOSE)
    {
      (void) do_close_internal ((struct common_pcb *) conn->comm_pcb_data, 0);
    }

  /* conn is already checked for NULL above with ASSERT */
  /* If the queued byte- or pbuf-count drops below the configured low-water limit,
     let select mark this pcb as writable again. */
  if (conn->snd_buf > TCP_SNDLOWAT)
    {
      conn->flags &= ~SPL_NETCONN_FLAG_CHECK_WRITESPACE;
      if (((struct common_pcb *) conn->comm_pcb_data)->model == SOCKET_STACKX)
        SPL_API_EVENT (conn, SPL_NETCONN_EVT_SENDPLUS, 1);
    }

  return ERR_OK;
}

/**
 * Error callback function for TCP netconns.
 * Signals conn->sem, posts to all conn mboxes and calls API_EVENT.
 * The application thread has then to decide what to do.
 *
 * @see tcp.h (struct tcp_pcb.err) for parameters
 */
void
spl_err_tcp (void *arg, err_t err)
{
  spl_netconn_t *conn;
  enum spl_netconn_state old_state;
  data_com_msg *forFree = NULL;

  struct common_pcb *cpcb;

  NSTCP_LOGINF ("Enter err_tcp.");

  conn = (spl_netconn_t *) arg;
  cpcb = (struct common_pcb *) (conn->comm_pcb_data);

  old_state = conn->state;
  conn->state = SPL_NETCONN_NONE;

  /* Call unlink pcb no matter what */
  unlink_pcb (cpcb);
  if (old_state == SPL_NETCONN_CLOSE)
    {
      /* RST during close: let close return success & dealloc the netconn */
      err = ERR_OK;
      SPL_NETCONN_SET_SAFE_ERR (conn, ERR_OK);
    }
  else
    {
      SPL_NETCONN_SET_SAFE_ERR (conn, err);
    }

  NSTCP_LOGWAR ("inform HUP and ERROR event.");
  SPL_API_EVENT (conn, SPL_NETCONN_EVT_RCVPLUS, 0);
  SPL_API_EVENT (conn, SPL_NETCONN_EVT_SENDPLUS, 0);
  SPL_API_EVENT (conn, SPL_NETCONN_EVT_ERROR, 0);
  SPL_API_EVENT (conn, SPL_NETCONN_EVT_HUP, 1);

  if ((old_state == SPL_NETCONN_WRITE) || (old_state == SPL_NETCONN_CLOSE)
      || (old_state == SPL_NETCONN_CONNECT))
    {
      int was_nonblocking_connect = spl_netconn_is_nonblocking (conn);
      SPL_SET_NONBLOCKING_CONNECT (conn, 0);

      if (!was_nonblocking_connect)
        {
          /* set error return code */
          if (NULL == cpcb->current_msg)
            {
              NSPOL_LOGERR ("conn->current_msg==NULL");
              return;
            }

          /* current msg could be connect msg or write_buf_msg */
          SET_MSG_ERR (cpcb->current_msg, err);
          /* signal app to exit  */
          if (old_state == SPL_NETCONN_CONNECT
              && !spl_netconn_is_nonblocking (conn))
            {
              SYNC_MSG_ACK (cpcb->current_msg);
            }

          /* no matter it's connect msg or write msg, we will no process it any more */
          cpcb->current_msg = NULL;

          /* but, if msg is write_buf_msg, then we should release the pbuf in msg */
          if (cpcb->msg_head != NULL)
            {
              /* current_msg should be write_buf_msg */
              msg_write_buf *tmp_msg = cpcb->msg_head;
              while (tmp_msg != NULL)
                {

                  forFree = MSG_ENTRY (tmp_msg, data_com_msg, buffer);
                  spl_pbuf_free (tmp_msg->p);
                  tmp_msg = tmp_msg->next;

                  // free msg
                  ASYNC_MSG_FREE (forFree);
                }
              cpcb->current_msg = NULL;
              cpcb->msg_head = cpcb->msg_tail = NULL;
            }
        }
    }
  else
    {

    }
}

/**
 * Setup a tcp_pcb with the correct callback function pointers
 * and their arguments.
 *
 * @param conn the TCP netconn to setup
 */
NSTACK_STATIC void
setup_tcp (struct tcp_pcb *pcb, spl_netconn_t * conn)
{
  struct common_pcb *cpcb = (struct common_pcb *) conn->comm_pcb_data;

  /* callback have the same value as cpcb->conn */
  cpcb->conn = conn;
  tcp_arg (pcb, (void *) conn);
  tcp_recv (pcb, tcp_fn[TCP_FN_STACKX].recv_fn);
  tcp_sent (pcb, tcp_fn[TCP_FN_STACKX].sent_fn);
  tcp_poll (pcb, tcp_fn[TCP_FN_STACKX].poll_fn, 4);
  tcp_err (pcb, tcp_fn[TCP_FN_STACKX].err_fn);

}

/**
 * Accept callback function for TCP netconns.
 * Allocates a new netconn and posts that to conn->acceptmbox.
 *
 * @see tcp.h (struct tcp_pcb_listen.accept) for parameters and return value
 */
err_t
spl_accept_function (void *arg, struct tcp_pcb *newpcb, err_t err)
{
  spl_netconn_t *newconn;
  spl_netconn_t *lconn = (spl_netconn_t *) arg; /* listen conneciton */
  struct common_pcb *cpcb = NULL;

  NSPOL_LOGDBG (API_MSG_DEBUG, "]state=%s",
                tcp_debug_state_str (newpcb->state));

  if (lconn == NULL)
    {
      NSPOL_LOGERR ("accept_function: conn is NULL");
      return ERR_VAL;
    }

  /* We have to set the callback here even though
   * the new socket is unknown. conn->socket is marked as -1. */
  newconn = ss_malloc_conn (lconn->conn_pool, SPL_NETCONN_TCP);
  if (newconn == NULL)
    {
      NSPOL_LOGERR ("conn alloc failed");
      return ERR_MEM;
    }

  cpcb = alloc_common_pcb (SPL_NETCONN_TCP);
  if (cpcb == NULL)
    {
      return ERR_MEM;
    }

  /* used by application to recycle conn */
  newconn->recycle.accept_from = lconn;
  newconn->conn_pool = lconn->conn_pool;

  newconn->recv_obj = (i64) & newconn->private_data;
  newconn->private_data = (i64) newpcb;
  newconn->comm_pcb_data = (i64) cpcb;

  NSFW_LOGINF ("alloc accept conn]conn=%p,pcb=%p,lconn=%p", newconn, newpcb,
               lconn);
  newconn->shut_status = 0xFFFF;
  newconn->last_err = ERR_OK;
  newconn->CanNotReceive = 0;
  newconn->flags = 0;

  newconn->state = SPL_NETCONN_NONE;
  newconn->mss = newpcb->mss;
  newconn->remote_ip.addr = newpcb->remote_ip.addr;
  newconn->remote_port = newpcb->remote_port;
  ss_set_local_ip (newconn, newpcb->local_ip.addr);
  ss_set_local_port (newconn, newpcb->local_port);

  setup_tcp (newpcb, newconn);
  newpcb->state = ESTABLISHED;
  update_tcp_state (newconn, ESTABLISHED);

  /*Initialize flow control variables */
  newconn->tcp_sndbuf = CONN_TCP_MEM_DEF_LINE;
  newconn->tcp_wmem_alloc_cnt = 0;
  newconn->tcp_wmem_sbr_free_cnt = 0;
  newconn->tcp_wmem_spl_free_cnt = 0;
  newconn->snd_buf = 0;

  newconn->bind_thread_index = lconn->bind_thread_index;
  ss_set_msg_box (newconn,
                  ss_get_instance_msg_box (newconn->bind_thread_index, 0));

  /* no protection: when creating the pcb, the netconn is not yet known
     to the application thread */
  newconn->last_err = err;

  NSPOL_LOGDBG (TCP_DEBUG,
                "trypost]newconn=%p, tcp=%p state=%d conn state=%d ", newconn,
                newpcb, newpcb->state, newconn->state);
  if (accept_enqueue (lconn, (void *) newconn) != ERR_OK)
    {
      /* remove all references to this netconn from the pcb */
      NSPOL_LOGERR ("sp_enqueue!=ERR_OK]ring = %p", newconn->recv_ring);

      tcp_arg (newpcb, NULL);
      tcp_recv (newpcb, tcp_fn[TCP_FN_NULL].recv_fn);
      tcp_sent (newpcb, tcp_fn[TCP_FN_NULL].sent_fn);
      tcp_poll (newpcb, tcp_fn[TCP_FN_NULL].poll_fn, 0);
      tcp_err (newpcb, tcp_fn[TCP_FN_NULL].err_fn);
      /* drop conn */
      tcp_drop_conn (newconn);
      return ERR_MEM;
    }

  SPL_API_EVENT (lconn, SPL_NETCONN_EVT_ACCEPT, 1);
  /* tcp_accepted_with_return_value(newpcb); */

  return ERR_OK;
}

int
common_pcb_init (struct common_pcb *cpcb)
{
  // u32 threadnum = p_lwip_instance->rss_queue_id + 1;//0 for app

  cpcb->hostpid = get_sys_pid ();

  cpcb->socket = 0;
  /* run and close repeatly nstackmain coredum */
  cpcb->close_progress = 0;
  cpcb->model = SOCKET_STACKX;
  cpcb->current_msg = NULL;
  cpcb->msg_head = cpcb->msg_tail = NULL;
  cpcb->write_offset = 0;

  cpcb->recv_timeout = cpcb->send_timeout = 0;

  cpcb->sk_rcvlowat = 1;

  cpcb->l4_tick = 0;
  cpcb->dataSentFlag = 0;
  cpcb->recv_ring_not_empty = 0;

  return 0;
}

int
common_pcb_reset (struct common_pcb *cpcb)
{
  cpcb->conn = NULL;
  cpcb->hostpid = 0;

  cpcb->socket = 0;
  /* run and close repeatly nstackmain coredum */
  cpcb->close_progress = 0;
  cpcb->model = 0;
  cpcb->current_msg = NULL;
  cpcb->msg_head = cpcb->msg_tail = NULL;
  cpcb->write_offset = 0;

  cpcb->recv_timeout = cpcb->send_timeout = 0;

  cpcb->sk_rcvlowat = 0;

  cpcb->l4_tick = 0;
  cpcb->dataSentFlag = 0;
  cpcb->recv_ring_not_empty = 0;

  return 0;
}

void
unlink_pcb (struct common_pcb *cpcb)
{
  if (cpcb && cpcb->conn)
    {
      /* whatever the  private data will be reset */
      cpcb->conn->private_data = 0;
      update_tcp_state (cpcb->conn, CLOSED);
    }
  else
    {
      NSPOL_LOGINF (API_MSG_DEBUG, "conn is detached already!");
    }
}

void
unlink_recv_ring (spl_netconn_t * conn)
{
  if (conn)
    conn->recv_ring_valid = 0;
}

void
recycle_tmr (void *arg)
{
  if (arg == NULL)
    {
      NSPOL_LOGERR ("recycle_message is NULL!");
    }
  else
    {
      (void) ss_recycle_conn (arg, do_try_delconn);
    }
}

/**
 * Create a new pcb of a specific type.
 * Called from do_newconn().
 *
 * @param msg the api_msg_msg describing the connection type
 * @return msg->conn->err, but the return value is currently ignored
 */
int
spl_pcb_new (msg_new_netconn * pmsg)
{
  u16_t thread_index = spl_get_lcore_id ();
  struct common_pcb *cpcb = NULL;
  enum lwip_ip_addr_type iptype = IPADDR_TYPE_V4;

  /* Allocate a PCB for this connection */
  switch (SPL_NETCONNTYPE_GROUP (pmsg->type))
    {
    case SPL_NETCONN_UDP:
      {
        struct udp_pcb *u = udp_new_ip_type (iptype);
        if (u == NULL)
          {
            return ERR_MEM;
          }

        if (pmsg->type == SPL_NETCONN_UDPNOCHKSUM)
          {
            udp_setflags (u, UDP_FLAGS_NOCHKSUM);
          }

        cpcb = (struct common_pcb *) alloc_common_pcb (SPL_NETCONN_UDP);
        if (cpcb == NULL)
          {
            return ERR_MEM;
          }

        pmsg->conn->comm_pcb_data = (i64) cpcb;
        pmsg->conn->private_data = (i64) u;
        udp_recv (u, spl_recv_udp, (void *) pmsg->conn);
        break;
      }

    case SPL_NETCONN_TCP:
      {
        struct tcp_pcb *t = tcp_new_ip_type (iptype);
        if (t == NULL)
          {
            return ERR_MEM;
          }

        cpcb = alloc_common_pcb (SPL_NETCONN_TCP);
        if (cpcb == NULL)
          {
            return ERR_MEM;
          }

        pmsg->conn->comm_pcb_data = (i64) cpcb;
        pmsg->conn->private_data = (i64) t;
        setup_tcp (t, pmsg->conn);
        break;
      }

    default:
      /* Unsupported netconn type, e.g. protocol disabled */
      NSPOL_LOGERR ("Unsupported netconn type!");
      return ERR_VAL;
    }

  /* type and socket value for all type */
  cpcb->socket = pmsg->socket;
  cpcb->conn = pmsg->conn;
  cpcb->bind_thread_index = thread_index;

  /* used to find pcb when a msg is received */
  return ERR_OK;
}

/**
 * Delete rcvmbox and acceptmbox of a netconn and free the left-over data in
 * these mboxes
 *
 * @param conn the netconn to free
 * @bytes_drained bytes drained from recvmbox
 * @accepts_drained pending connections drained from acceptmbox
 */
int
netconn_drain (enum spl_netconn_type type, spl_netconn_t * conn)
{
  int i, n;
  mring_handle ring = NULL;
  void *addr[32] = { 0 };
  int not_empty = 0;

  /* This runs in tcpip_thread, so we don't need to lock against rx packets */
  ring = conn->recv_ring;
  do
    {
      /*every times get atmost 32 to release */
      n = nsfw_mem_ring_dequeuev (ring, addr, 32);
      for (i = 0; i < n; i++)
        {
          /* if the con is UDP, buffer type also is struct spl_netbuf,it is same as RAW  */
          if ((type == SPL_NETCONN_RAW) || (type == SPL_NETCONN_UDP))
            {
              spl_pbuf_free ((struct spl_pbuf *) addr[i]);
              if (type == SPL_NETCONN_UDP)
                {
                  UDP_STATS_INC (udp.drop);
                }
            }
          else
            {
              spl_pbuf_free ((struct spl_pbuf *) addr[i]);
            }

          if (not_empty == 0)
            not_empty = 1;
        }
    }
  while (0 != n);
  return not_empty;
}

/* need free mbuf inside recv ring before free conn */
void
free_conn_by_spl (spl_netconn_t * conn)
{
  struct common_pcb *cpcb = (struct common_pcb *) conn->comm_pcb_data;

  free_common_pcb (cpcb);

  (void) netconn_drain (conn->type, conn);
  ss_free_conn (conn);
}

int
do_close_finished (struct common_pcb *cpcb, u8_t close_finished, u8_t shut,
                   err_t err, int OpShutDown)
{
  struct tcp_pcb *tpcb = (struct tcp_pcb *) cpcb->conn->private_data;
  spl_netconn_t *conn = cpcb->conn;

  if (!close_finished)
    {
      /* Closing succeeded */
      /* Closing failed, restore some of the callbacks */
      /* Closing of listen pcb will never fail! */
      if (LISTEN == tpcb->state)
        {
          NSPOL_LOGERR ("Closing a listen pcb may not fail!");
          return ERR_VAL;
        }

      tcp_sent (tpcb, tcp_fn[TCP_FN_STACKX].sent_fn);
      tcp_poll (tpcb, tcp_fn[TCP_FN_STACKX].poll_fn, 1);
      tcp_err (tpcb, tcp_fn[TCP_FN_STACKX].err_fn);
      tcp_arg (tpcb, conn);
      /* don't restore recv callback: we don't want to receive any more data */
    }
  else
    {
      /* Closing done (succeeded, non-memory error, nonblocking error or timeout) */
      if (cpcb->current_msg)
        {
          SET_MSG_ERR (cpcb->current_msg, err);
          cpcb->current_msg = NULL;
        }

      conn->state = SPL_NETCONN_NONE;
      NSTCP_LOGINF ("release pcb]conn=%p,pcb=%p", conn, tpcb);
      unlink_pcb (cpcb);
    }

  return ERR_OK;
}

NSTACK_STATIC void
reset_tcp_callback (struct tcp_pcb *tpcb, struct common_pcb *cpcb, u8 shut)
{
  spl_netconn_t *conn = cpcb->conn;

  /* some callbacks have to be reset if tcp_close is not successful */
  if (shut & SPL_NETCONN_SHUT_RD)
    {
      conn->CanNotReceive = 1;
      tcp_recv (tpcb, tcp_fn[TCP_FN_NULL].recv_fn);
      tcp_accept (tpcb, tcp_fn[TCP_FN_NULL].accept_fn);
    }

  if (shut & SPL_NETCONN_SHUT_WR)
    {
      tcp_sent (tpcb, tcp_fn[TCP_FN_NULL].sent_fn);
    }

  if (shut == SPL_NETCONN_SHUT_RDWR)
    {
      tcp_poll (tpcb, tcp_fn[TCP_FN_NULL].poll_fn, 0);
      tcp_err (tpcb, tcp_fn[TCP_FN_NULL].err_fn);
    }
}

/**
 * Internal helper function to close a TCP netconn: since this sometimes
 * doesn't work at the first attempt, this function is called from multiple
 * places.
 *
 * @param conn the TCP netconn to close
 */
err_t
do_close_internal (struct common_pcb *cpcb, int OpShutDown)
{
  err_t err;
  u8_t shut, ucclose;
  u8_t close_finished = 0;

  if (NULL == cpcb)
    {
      NSTCP_LOGERR ("invalid conn");
      return ERR_ARG;
    }

  spl_netconn_t *conn = cpcb->conn;
  if (SPL_NETCONN_TCP != cpcb->type || SPL_NETCONN_CLOSE != conn->state)
    {
      NSTCP_LOGERR ("conn=%p, err_type=%d, error_state=%d",
                    conn, cpcb->type, conn->state);
      return ERR_VAL;
    }

  struct tcp_pcb *tpcb = (struct tcp_pcb *) (conn->private_data);

  if (cpcb->current_msg != NULL)
    shut = get_shut_op (cpcb->current_msg);
  else
    shut = SPL_NETCONN_SHUT_RDWR;

  /* shutting down both ends is the same as closing */
  ucclose = (shut == SPL_NETCONN_SHUT_RDWR);

  /* Set back some callback pointers */
  if (ucclose)
    {
      conn->CanNotReceive = 1;
      tcp_arg (tpcb, conn);
    }

  if (tpcb->state == LISTEN)
    {
      tcp_accept (tpcb, tcp_fn[TCP_FN_NULL].accept_fn);
    }
  else
    {
      reset_tcp_callback (tpcb, cpcb, shut);
    }

  /* Try to close the connection */
  if (shut == SPL_NETCONN_SHUT_RDWR && !OpShutDown)
    {
      err = tcp_close (tpcb);
    }
  else
    {
      err =
        tcp_shutdown (tpcb, shut & SPL_NETCONN_SHUT_RD,
                      shut & SPL_NETCONN_SHUT_WR);
    }

  if (err == ERR_OK)
    {
      close_finished = 1;
    }
  else
    {
      if (err == ERR_MEM)
        {
          /* Blocking close, check the timeout */
          close_finished = 1;
          if (ucclose)
            {
              /* in this case, we want to RST the connection */
              tcp_abort (tpcb);
              err = ERR_OK;
            }
        }
      else
        {
          /* Closing failed for a non-memory error: give up */
          close_finished = 1;
        }
    }

  err_t err1 = do_close_finished (cpcb, close_finished,
                                  shut, err, OpShutDown);
  if (err1 != ERR_OK)
    {
      NSTCP_LOGERR ("return err1]pcb=%p,err1=%d", tpcb, err1);
      return err1;
    }
  return err;

  /* If closing didn't succeed, we get called again either
     from poll_tcp or from sent_tcp */
}

void
do_try_delconn (void *close_data, u32 delay_sec)
{
  msg_delete_netconn *dmsg = (msg_delete_netconn *) close_data;
  data_com_msg *m = MSG_ENTRY (dmsg, data_com_msg, buffer);
  struct common_pcb *cpcb = NULL;

  spl_netconn_t *conn = dmsg->conn;

  /* no receiver in some recycle cases */
  if (0 == m->param.comm_receiver)
    {
      if (NULL == conn)
        {
          NSFW_LOGERR ("release err conn!]pid=%d", dmsg->pid);
          /* need to free pbufs which are attached to msg */
          do_pbuf_free (dmsg->buf);
          return;
        }

      cpcb = (struct common_pcb *) conn->comm_pcb_data;
    }
  else
    {
      cpcb = COMM_PRIVATE_PTR (m);
    }

  /* no pcb in some recycle cases */
  if (conn->private_data == 0)
    {
      /* though pcb is gone, still need to clean the mbox */
      if (conn != NULL)
        (void) netconn_drain (conn->type, conn);

      NSFW_LOGWAR ("release conn pcb null]pcb=%p", m->param.receiver);
      /* need to free pbufs which are attached to msg */
      do_pbuf_free (dmsg->buf);
      return;
    }

  if (delay_sec)
    {
      NSFW_LOGWAR ("delay to recycle!]pcb=%p, msg=%d", cpcb, dmsg);
      sys_timeout (delay_sec * 1000, recycle_tmr, dmsg);
      return;
    }

  do_delconn (cpcb, dmsg);

  /* after the do_delconn, the PCB maybe released, but the conn is ok */
  sys_sem_s_signal (&dmsg->conn->close_completed);

  return;
}

/**
 * Delete the pcb inside a netconn.
 * Called from netconn_delete.
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_delconn (struct common_pcb *cpcb, msg_delete_netconn * dmsg)
{
  spl_netconn_t *conn = cpcb->conn;
  data_com_msg *pmsg = MSG_ENTRY (dmsg, data_com_msg, buffer);

  /*if already close, just return */
  /* run and close repeatly nstackmain coredum */
  if (1 == cpcb->close_progress)
    {
      SET_MSG_ERR (pmsg, ERR_OK);
      return;
    }
  cpcb->close_progress = 1;

  /* Pbuf free should be done in network stack */
  do_pbuf_free (dmsg->buf);

  /* @todo TCP: abort running write/connect? */
  if ((conn->state != SPL_NETCONN_NONE)
      && (conn->state != SPL_NETCONN_LISTEN)
      && (conn->state != SPL_NETCONN_CONNECT))
    {
      /* this only happens for TCP netconns */
      if (SPL_NETCONN_TCP != cpcb->type)
        {
          NSTCP_LOGERR ("msg->conn->type error!]conn=%p,msg->conn->type=%d",
                        conn, cpcb->type);
          SET_MSG_ERR (pmsg, ERR_VAL);
          return;
        }
      NSPOL_LOGINF (SOCK_INFO, "conn is not for free state]conn=%p,state=%d",
                    conn, conn->state);

      //need to release pcb
      if (cpcb->current_msg != NULL && cpcb->msg_tail != NULL)
        {
          NSPOL_LOGINF (SOCK_INFO,
                        "conn there is data in conn->current_msg when close the conn]conn=%p,state=%d",
                        conn, conn->state);
          (void) do_writemore (cpcb->conn);
        }
    }

  /*
     the accpet connections waiting in listen queue will be clear in
     netconn_drain.
   */
  if (conn->state == SPL_NETCONN_LISTEN)
    {
      /* This function does nothing in original code. */
    }
  else
    {
      /* Drain pbuf from non-listen mboxes */
      if (netconn_drain (cpcb->type, conn))
        {
          cpcb->recv_ring_not_empty = 1;

          NSPOL_LOGWAR (SOCK_INFO,
                        "still some data leave in recv ring when close");
        }
    }

  /* conn will be released outside */

  switch (SPL_NETCONNTYPE_GROUP (cpcb->type))
    {

    case SPL_NETCONN_UDP:
      {
        struct udp_pcb *upcb = (struct udp_pcb *) cpcb->conn->private_data;
        upcb->recv_arg = NULL;
        udp_remove (upcb);
        break;
      }

    case SPL_NETCONN_TCP:
      {
        conn->state = SPL_NETCONN_CLOSE;
        dmsg->shut = SPL_NETCONN_SHUT_RDWR;
        cpcb->current_msg = pmsg;

        if (ERR_INPROGRESS == do_close_internal (cpcb, 0))
          {
            return;
          }
      }
    default:
      break;
    }

  /* tcp netconns don't come here! */

}

/**
 * Bind a pcb contained in a netconn
 * Called from netconn_bind.
 *
 * @param msg the api_msg_msg pointing to the connection and containing
 *            the IPaddress and port to bind to
 */
void
do_bind (struct common_pcb *cpcb, msg_bind * bmsg)
{
  spl_netconn_t *conn = cpcb->conn;
  data_com_msg *pmsg = MSG_ENTRY (bmsg, data_com_msg, buffer);
  ip_addr_t stIpaddr;

  stIpaddr.addr = bmsg->ipaddr.addr;

  if (SPL_ERR_IS_FATAL (conn->last_err)
      && ERR_CANTASSIGNADDR != conn->last_err)
    {
      NSTCP_LOGERR ("bind but conn has err]pcb=%p,conn=%p,err=%d", cpcb, conn,
                    conn->last_err);
      SET_MSG_ERR (pmsg, conn->last_err);
    }
  else
    {
      if (!ip_addr_isany (&bmsg->ipaddr)
          && !ip_addr_ismulticast (&bmsg->ipaddr))
        {
          if (!get_netif_by_ip (bmsg->ipaddr.addr)
              && !netif_check_broadcast_addr (&bmsg->ipaddr))
            {
              NSPOL_LOGERR ("ip is not exist]pcb=%p,ip=%u", cpcb,
                            bmsg->ipaddr.addr);
              SET_MSG_ERR (pmsg, ERR_CANTASSIGNADDR);
              return;
            }
        }

      SET_MSG_ERR (pmsg, ERR_VAL);
      switch (SPL_NETCONNTYPE_GROUP (cpcb->type))
        {
        case SPL_NETCONN_UDP:
          SET_MSG_ERR (pmsg,
                       udp_bind ((struct udp_pcb *) (cpcb->
                                                     conn->private_data),
                                 &stIpaddr, bmsg->port));

          /* Set local Ip, willbe used during connect */
          struct udp_pcb *upcb = (struct udp_pcb *) cpcb->conn->private_data;
          ss_set_local_ip (cpcb->conn, upcb->local_ip.addr);
          ss_set_local_port (cpcb->conn, upcb->local_port);
          NSTCP_LOGINF ("updated Ip to spl_netconn_t] %x",
                        upcb->local_ip.addr);

          NSUDP_LOGINF
            ("udp_bind return]pcb=%p,IP=%u.%u.%u.%u,port=%u,err=%d",
             (struct udp_pcb *) cpcb->conn->private_data,
             &bmsg->ipaddr == NULL ? 0 : ip4_addr1_16 (&stIpaddr),
             &bmsg->ipaddr == NULL ? 0 : ip4_addr2_16 (&stIpaddr),
             &bmsg->ipaddr == NULL ? 0 : ip4_addr3_16 (&stIpaddr),
             &bmsg->ipaddr == NULL ? 0 : ip4_addr4_16 (&stIpaddr), bmsg->port,
             GET_MSG_ERR (pmsg));
          break;

        case SPL_NETCONN_TCP:
          SET_MSG_ERR (pmsg,
                       tcp_bind ((struct tcp_pcb *) cpcb->conn->private_data,
                                 &stIpaddr, bmsg->port));

          /* Set local Ip, willbe used during connect */
          struct tcp_pcb *tpcb = (struct tcp_pcb *) cpcb->conn->private_data;
          NSTCP_LOGINF ("updated Ip to spl_netconn_t] %x",
                        tpcb->local_ip.addr);
          ss_set_local_ip (cpcb->conn, tpcb->local_ip.addr);
          ss_set_local_port (cpcb->conn, tpcb->local_port);

          NSTCP_LOGINF
            ("tcp_bind return]pcb=%p,IP=%u.%u.%u.%u,port=%u,err=%d",
             (struct tcp_pcb *) cpcb->conn->private_data,
             &bmsg->ipaddr == NULL ? 0 : ip4_addr1_16 (&stIpaddr),
             &bmsg->ipaddr == NULL ? 0 : ip4_addr2_16 (&stIpaddr),
             &bmsg->ipaddr == NULL ? 0 : ip4_addr3_16 (&stIpaddr),
             &bmsg->ipaddr == NULL ? 0 : ip4_addr4_16 (&stIpaddr), bmsg->port,
             GET_MSG_ERR (pmsg));
          break;
        default:
          break;
        }
    }

  conn->last_err = GET_MSG_ERR (pmsg);
  NSPOL_LOGDBG (TESTSOCKET_DEBUG | LWIP_DBG_TRACE, "the msg is doing bind");
  NSTCP_LOGINF ("the msg is doing bind");
}

/**
 * TCP callback function if a connection (opened by tcp_connect/do_connect) has
 * been established (or reset by the remote host).
 *
 * @see tcp.h (struct tcp_pcb.connected) for parameters and return values
 */
err_t
spl_do_connected (void *arg, struct tcp_pcb *pcb, err_t err)
{
  spl_netconn_t *conn = (spl_netconn_t *) arg;
  struct common_pcb *cpcb = (struct common_pcb *) conn->comm_pcb_data;
  data_com_msg *m = cpcb->current_msg;
  int was_blocking;

  if (conn == NULL)
    {
      NSTCP_LOGERR ("conn is NULL!]arg=%p", arg);
      return ERR_ARG;
    }

  if (SPL_NETCONN_CONNECT != conn->state)
    {
      NSTCP_LOGERR ("conn->state error!]conn=%p,state=%d", conn, conn->state);
      return ERR_VAL;
    }

  if ((NULL == m) && (0 == spl_netconn_is_nonblocking (conn)))
    {
      NSTCP_LOGERR ("conn->current_msg!]conn=%p,current_msg=%p", conn, m);
      return ERR_VAL;
    }

  if (m != NULL)
    {
      SET_MSG_ERR (m, err);
    }

  if ((cpcb->type == SPL_NETCONN_TCP) && (err == ERR_OK))
    {
      setup_tcp (pcb, conn);
    }

  ss_set_local_ip (conn, pcb->local_ip.addr);
  ss_set_local_port (conn, pcb->local_port);
  conn->remote_ip.addr = pcb->remote_ip.addr;
  conn->remote_port = pcb->remote_port;

  conn->mss = pcb->mss;
  cpcb->current_msg = NULL;

  was_blocking = !SPL_IN_NONBLOCKING_CONNECT (conn);
  SPL_SET_NONBLOCKING_CONNECT (conn, 0);
  conn->state = SPL_NETCONN_NONE;
  SPL_NETCONN_SET_SAFE_ERR (conn, ERR_OK);

  SPL_API_EVENT (conn, SPL_NETCONN_EVT_SENDPLUS, 1);

  if (was_blocking && m != NULL)
    {
      SYNC_MSG_ACK (m);
    }

  return ERR_OK;
}

int
do_internal_tcp_connect (struct common_pcb *cpcb, struct tcp_pcb *tpcb,
                         msg_connect * cmsg)
{
  spl_netconn_t *conn = cpcb->conn;
  data_com_msg *pmsg = MSG_ENTRY (cmsg, data_com_msg, buffer);
  ip_addr_t ip_addr;

  ip_addr.addr = cmsg->ipaddr.addr;

  /* Prevent connect while doing any other action. */
  if (conn->state != SPL_NETCONN_NONE)
    {
      if (conn->state == SPL_NETCONN_CONNECT
          || conn->state == SPL_NETCONN_WRITE)
        {
          if (tpcb->state != ESTABLISHED)
            {
              SET_MSG_ERR (pmsg, ERR_ALREADY);
            }
          else
            {
              SET_MSG_ERR (pmsg, ERR_ISCONN);
            }
        }
      else
        {
          SET_MSG_ERR (pmsg, ERR_ISCONN);
        }

      NSTCP_LOGINF
        ("tcp_connect]pcb=%p,state=%d,remote_ip=%u.%u.%u.%u,remote_port=%u,local_ip=%u.%u.%u.%u,local_port=%u,err=%d",
         tpcb, conn->state, ip4_addr1_16 (&ip_addr), ip4_addr2_16 (&ip_addr),
         ip4_addr3_16 (&ip_addr), ip4_addr4_16 (&ip_addr), cmsg->port,
         ip4_addr1_16 (&tpcb->local_ip), ip4_addr2_16 (&tpcb->local_ip),
         ip4_addr3_16 (&tpcb->local_ip), ip4_addr4_16 (&tpcb->local_ip),
         tpcb->local_port, GET_MSG_ERR (pmsg));
    }
  else
    {
      setup_tcp (tpcb, conn);
      SET_MSG_ERR (pmsg,
                   tcp_connect (tpcb, &ip_addr, cmsg->port,
                                tcp_fn[TCP_FN_STACKX].connected_fn));
      conn->mss = tpcb->mss;
      SPL_API_EVENT (conn, NETCONN_EVT_SENDMINUS, 1);
      SPL_API_EVENT (conn, NETCONN_EVT_RCVMINUS, 1);

      NSTCP_LOGINF
        ("tcp_connect]pcb=%p,state=%d,remote_ip=%u.%u.%u.%u,remote_port=%u,local_ip=%u.%u.%u.%u,local_port=%u,err=%d",
         tpcb, conn->state, ip4_addr1_16 (&ip_addr), ip4_addr2_16 (&ip_addr),
         ip4_addr3_16 (&ip_addr), ip4_addr4_16 (&ip_addr), cmsg->port,
         ip4_addr1_16 (&tpcb->local_ip), ip4_addr2_16 (&tpcb->local_ip),
         ip4_addr3_16 (&tpcb->local_ip), ip4_addr4_16 (&tpcb->local_ip),
         tpcb->local_port, GET_MSG_ERR (pmsg));
      if (GET_MSG_ERR (pmsg) == ERR_OK)
        {
          int nonblock = spl_netconn_is_nonblocking (conn);
          SPL_SET_NONBLOCKING_CONNECT (conn, nonblock);
          conn->state = SPL_NETCONN_CONNECT;
          if (nonblock)
            {
              SET_MSG_ERR (pmsg, ERR_INPROGRESS);
            }
          else
            {
              cpcb->current_msg = pmsg;
              /* sys_sem_signal() is called from do_connected (or err_tcp()),
               * when the connection is established! */
              return -1;
            }
        }
    }

  return 0;
}

/**
 * Connect a pcb contained inside a netconn
 * Called from netconn_connect.
 *
 * @param msg the api_msg_msg pointing to the connection and containing
 *            the IPaddress and port to connect to
 */
void
do_connect (struct common_pcb *cpcb, msg_connect * cmsg)
{
  data_com_msg *pmsg = MSG_ENTRY (cmsg, data_com_msg, buffer);
  ip_addr_t ipaddr;

  SET_MSG_ERR (pmsg, ERR_OK);

  if (cpcb == NULL)
    {
      /* This may happen when calling netconn_connect() a second time */
      NSTCP_LOGERR ("conn connect but conn has err]msg=%p", pmsg);
      SET_MSG_ERR (pmsg, ERR_CLSD);
      return;
    }

  ipaddr.addr = cmsg->ipaddr.addr;
  switch (SPL_NETCONNTYPE_GROUP (cpcb->type))
    {
    case SPL_NETCONN_UDP:
      {
        struct udp_pcb *upcb = (struct udp_pcb *) (cpcb->conn->private_data);
        if (ip_addr_isany (&upcb->local_ip))
          {
            upcb->local_ip.addr = cmsg->local_ip.addr;
            ss_set_local_ip (cpcb->conn, upcb->local_ip.addr);
          }

        SET_MSG_ERR (pmsg, udp_connect (upcb, &ipaddr, cmsg->port));
        if (ERR_OK == pmsg->param.err)
          {
            cpcb->conn->remote_ip.addr = cmsg->ipaddr.addr;
            cpcb->conn->remote_port = cmsg->port;
          }
        break;
      }
    case SPL_NETCONN_TCP:
      {
        struct tcp_pcb *tpcb = (struct tcp_pcb *) (cpcb->conn->private_data);
        if (ip_addr_isany (&tpcb->local_ip))
          {
            tpcb->local_ip.addr = cmsg->local_ip.addr;
            ss_set_local_ip (cpcb->conn, tpcb->local_ip.addr);
          }

        if (0 != do_internal_tcp_connect (cpcb, tpcb, cmsg))
          {
            return;
          }

      }
      break;
    default:
      NSPOL_LOGERR ("Invalid netconn type]type=%d", cpcb->type);
      SET_MSG_ERR (pmsg, ERR_VAL);
      break;
    }

  /* For all other protocols, netconn_connect() calls TCPIP_APIMSG(),
     so use TCPIP_APIMSG_ACK() here.
     Do as lwip-2.0.0. set conn->last_error here.  */
  SPL_NETCONN_SET_SAFE_ERR (cpcb->conn, GET_MSG_ERR (pmsg));
}

/* Pbuf free should be done in network stack */
void
do_pbuf_free (struct spl_pbuf *buf)
{
  if (buf)
    {
      struct spl_pbuf *pCur = buf;
      while (buf)
        {
          pCur = buf;
          buf = buf->freeNext;
          spl_pbuf_free (pCur);
        }
    }
}

/**
 * Set a TCP pcb contained in a netconn into listen mode
 * Called from netconn_listen.
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_listen (struct common_pcb *cpcb, msg_listen * lmsg)
{
  err_t err = ERR_OK;
  spl_netconn_t *conn = cpcb->conn;
  data_com_msg *pmsg = MSG_ENTRY (lmsg, data_com_msg, buffer);

  if (SPL_ERR_IS_FATAL (conn->last_err))
    {
      NSTCP_LOGERR ("conn listern but conn has err]conn=%p,err=%d",
                    conn, conn->last_err);
      SET_MSG_ERR (pmsg, conn->last_err);
    }
  else
    {
      SET_MSG_ERR (pmsg, ERR_CONN);

      if (cpcb->type == SPL_NETCONN_TCP)
        {
          if (conn->state == SPL_NETCONN_NONE)
            {
              struct tcp_pcb *lpcb;
              if (((struct tcp_pcb *) (cpcb->conn->private_data))->state !=
                  CLOSED)
                {
                  /* connection is not closed, cannot listen */
                  SET_MSG_ERR (pmsg, ERR_VAL);;
                }
              else
                {
                  u8_t backlog = TCP_DEFAULT_LISTEN_BACKLOG;

                  lpcb =
                    tcp_listen_with_backlog_and_err ((struct tcp_pcb
                                                      *) (cpcb->
                                                          conn->private_data),
                                                     backlog, &err);
                  if (lpcb == NULL)
                    {
                      /* in this case, the old pcb is still allocated */
                      SET_MSG_ERR (pmsg, err);
                    }
                  else
                    {
                      SET_MSG_ERR (pmsg, ERR_OK);
                      /* conn now is put on lpcb */
                      conn->state = SPL_NETCONN_LISTEN;

                      /* NOTE: pmsg->conn->comm_pcb_data == (i64)cpcb; should be already same. */
                      conn->private_data = (i64) lpcb;

                      tcp_arg (lpcb, conn);
                      tcp_accept (lpcb, tcp_fn[TCP_FN_STACKX].accept_fn);

                      SPL_API_EVENT (conn, NETCONN_EVT_SENDMINUS, 1);
                      SPL_API_EVENT (conn, NETCONN_EVT_RCVMINUS, 1);
                    }
                }
            }
          else if (conn->state == SPL_NETCONN_LISTEN)
            {
              SET_MSG_ERR (pmsg, ERR_OK);
            }
        }
      NSTCP_LOGINF ("listen]conn=%p,pcb=%p,connstate=%d,err=%d",
                    conn, cpcb, conn->state, GET_MSG_ERR (pmsg));

    }

}

/**
 * Send some data on UDP pcb contained in a netconn
 * Called from do_send
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
spl_udp_send (struct common_pcb *cpcb, msg_send_buf * smsg)
{
  struct spl_pbuf *p_from = smsg->p;
  spl_netconn_t *conn = cpcb->conn;
  struct udp_pcb *upcb = (struct udp_pcb *) (cpcb->conn->private_data);
  data_com_msg *m = MSG_ENTRY (smsg, data_com_msg, buffer);
  struct pbuf *p_to = NULL;
  err_t err = ERR_OK;

  //allocate pbuf and copy spl_pbuf, send , free pbuf and spl_pbuf
  do
    {
      p_to = pbuf_alloc (PBUF_TRANSPORT, p_from->len, PBUF_RAM);
      if (NULL == p_to)
        {
          NSPOL_LOGERR ("pbuf is NULL]conn=%p,pcb=%p", conn, upcb);
          return;
        }

      err = splpbuf_to_pbuf_transport_copy (p_to, p_from);
      if (err != ERR_OK)
        {
          SET_MSG_ERR (m, conn->last_err);
          return;
        }

      if (ip_addr_isany (&smsg->addr))
        {
          SET_MSG_ERR (m, udp_send (upcb, p_to));
        }
      else
        {
          SET_MSG_ERR (m,
                       udp_sendto (upcb, p_to, (ip_addr_t *) & smsg->addr,
                                   smsg->port));
        }

      p_from = (struct spl_pbuf *) ADDR_SHTOL (p_from->next_a);
    }
  while (p_from != NULL);

}

/**
 * Send some data on a RAW or UDP pcb contained in a netconn
 * Called from netconn_send
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_send (struct common_pcb *cpcb, msg_send_buf * smsg)
{
  struct spl_pbuf *p = smsg->p;
  data_com_msg *m = MSG_ENTRY (smsg, data_com_msg, buffer);
  spl_netconn_t *conn = cpcb->conn;

  pbuf_set_recycle_flg (p, MBUF_HLD_BY_SPL);    /* release buf hold by app on abnormal exit */

  if (SPL_ERR_IS_FATAL (conn->last_err))
    {
      SET_MSG_ERR (m, conn->last_err);
      NSPOL_LOGERR ("Invalid param]msg->conn=%p", conn);
      goto err_return;
    }

  switch (SPL_NETCONNTYPE_GROUP (cpcb->type))
    {
    case SPL_NETCONN_UDP:
      {
        struct udp_pcb *upcb = (struct udp_pcb *) (cpcb->conn->private_data);
        if (ip_addr_isany (&upcb->local_ip))
          {
            upcb->local_ip.addr = smsg->local_ip.addr;
            ss_set_local_ip (conn, smsg->local_ip.addr);
          }

        spl_udp_send (cpcb, smsg);

        break;
      }

    default:
      SET_MSG_ERR (m, ERR_CONN);
      break;
    }

err_return:
  pbuf_free_safe (smsg->p);
  ASYNC_MSG_FREE (m);

  return;
}

/**
 * Indicate data has been received from a TCP pcb contained in a netconn
 * Called from netconn_recv
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_recv (struct common_pcb *cpcb, msg_recv_buf * rmsg)
{
  data_com_msg *m = MSG_ENTRY (rmsg, data_com_msg, buffer);
  SET_MSG_ERR (m, ERR_OK);
  struct tcp_pcb *tpcb = (struct tcp_pcb *) cpcb->conn->private_data;

  if (NULL == tpcb)
    {
      if (rmsg->p)
        {
          NSPOL_LOGDBG (TCP_DEBUG,
                        "When pcb was freed: do recv, and free pbuf");
          spl_pbuf_free (rmsg->p);
          rmsg->p = NULL;
        }

      return;
    }

  if (cpcb->conn->type == SPL_NETCONN_TCP)
    {
      /* Pbuf free should be done in network stack */
      if (rmsg->p)
        {
          NSPOL_LOGDBG (TCP_DEBUG, "do recv, and free pbuf");
          spl_pbuf_free (rmsg->p);
          rmsg->p = NULL;
        }
      tcp_recved (tpcb, rmsg->len * TCP_MSS);
    }
}

/**
 * See if more data needs to be written from a previous call to netconn_write.
 * Called initially from do_write. If the first call can't send all data
 * (because of low memory or empty send-buffer), this function is called again
 * from sent_tcp() or poll_tcp() to send more data. If all data is sent, the
 * blocking application thread (waiting in netconn_write) is released.
 *
 * @param conn netconn (that is currently in state NETCONN_WRITE) to process
 * @return ERR_OK
 *         ERR_MEM if LWIP_TCPIP_CORE_LOCKING=1 and sending hasn't yet finished
 */

/**
 * See if more data needs to be written from a previous call to netconn_write.
 * Called initially from do_write. If the first call can't send all data
 * (because of low memory or empty send-buffer), this function is called again
 * from sent_tcp() or poll_tcp() to send more data. If all data is sent, the
 * blocking application thread (waiting in netconn_write) is released.
 *
 * @param conn netconn (that is currently in state NETCONN_WRITE) to process
 * @return ERR_OK
 *         ERR_MEM if LWIP_TCPIP_CORE_LOCKING=1 and sending hasn't yet finished
 */
err_t
do_writemore (struct spl_netconn *conn)
{
  err_t err = ERR_OK;
  u16_t len = 0, available;
  u8_t write_finished = 0;
  struct tcp_pcb *tpcb = (struct tcp_pcb *) conn->private_data;
  common_pcb *cpcb = (struct common_pcb *) conn->comm_pcb_data;
  size_t diff;
  const void *dataptr;

  if ((NULL == tpcb) || (NULL == conn) || (SPL_NETCONN_WRITE != conn->state)
      || (NULL == cpcb->current_msg))
    {
      NSPOL_LOGERR ("conn=NULL");
      return ERR_ARG;
    }

  msg_write_buf *wmsg = (msg_write_buf *) cpcb->current_msg->buffer;
start_write:

  if (NULL == wmsg->p)
    {
      NSPOL_LOGERR ("wmsg->p is NULL]conn=%p,pcb=%p", conn, tpcb);
      return ERR_VAL;
    }

  u8_t apiflags = wmsg->apiflags;
  struct spl_pbuf *current_w_pbuf = wmsg->p;
  current_w_pbuf->res_chk.u8Reserve |= NEED_ACK_FLAG;
  wmsg->p = current_w_pbuf->next;
  current_w_pbuf->next = NULL;

  dataptr = (const u8_t *) current_w_pbuf->payload + cpcb->write_offset;
  diff = current_w_pbuf->len - cpcb->write_offset;

  if (diff > 0xffffUL)
    {
      len = 0xffff;
      apiflags |= TCP_WRITE_FLAG_MORE;
    }
  else
    {
      len = (u16_t) diff;
    }

  available = tcp_sndbuf (tpcb);
  if (!available)
    {
      err = ERR_MEM;
      goto err_mem;
    }

  if (available < len)
    {
      /* don't try to write more than sendbuf */
      len = available;
      apiflags |= TCP_WRITE_FLAG_MORE;
    }

  err = tcp_write (tpcb, dataptr, len, apiflags);
  if ((err == ERR_OK) || (err == ERR_MEM))
    {
    err_mem:
      if ((tcp_sndbuf (tpcb) <= TCP_SNDLOWAT) ||
          (tcp_sndqueuelen (tpcb) >= TCP_SNDQUEUELOWAT))
        {
          SPL_API_EVENT (conn, NETCONN_EVT_SENDMINUS, 1);
        }
    }
  if (err == ERR_OK)
    {
      cpcb->write_offset += len;
      tcp_output (tpcb);

      if (cpcb->write_offset == current_w_pbuf->len)
        {
          cpcb->write_offset = 0;
          spl_pbuf_free (current_w_pbuf);
          if (NULL == wmsg->p)
            {
              /* this message is finished */
              cpcb->write_offset = 0;
              SET_MSG_ERR (cpcb->current_msg, err);

              /* go to next msg */
              data_com_msg *forFreemsg = cpcb->current_msg;
              msg_write_buf *msg_head_prev = cpcb->msg_head;
              cpcb->msg_head = cpcb->msg_head->next;

              /* no msg remain */
              if (cpcb->msg_head == NULL)
                {
                  write_finished = 1;
                  if (cpcb->msg_tail != NULL
                      && msg_head_prev != cpcb->msg_tail)
                    {
                      NSPOL_LOGWAR (TCP_DEBUG,
                                    "err maybe lost one mesg]conn=%p,pcb=%p",
                                    conn, tpcb);
                    }
                  cpcb->msg_tail = NULL;
                  cpcb->current_msg = NULL;
                  conn->state = SPL_NETCONN_NONE;
                }
              else              /* go to handle next message */
                {

                  cpcb->current_msg =
                    MSG_ENTRY (cpcb->msg_head, data_com_msg, buffer);
                  wmsg = cpcb->msg_head;
                  write_finished = 0;
                }

              ASYNC_MSG_FREE (forFreemsg);
            }

        }
      else
        {
          if (cpcb->write_offset > current_w_pbuf->len)
            {
              NSPOL_LOGERR ("Big trouble write_offset > current_w_pbuf->len");
            }

          current_w_pbuf->next = wmsg->p;
          wmsg->p = current_w_pbuf;
        }
      if ((write_finished == 0) && (NULL != wmsg->p))
        {
          goto start_write;
        }
    }
  else if (err == ERR_MEM)
    {
      current_w_pbuf->next = wmsg->p;
      wmsg->p = current_w_pbuf;
      tcp_output (tpcb);
    }
  else
    {
      NSPOL_LOGERR ("]pcb=%p, error when send %d", tpcb, err);
      write_finished = 1;
      cpcb->write_offset = 0;
      current_w_pbuf->next = wmsg->p;
      wmsg->p = current_w_pbuf;
      tcp_abort (tpcb);
      return err;
    }
  NSTCP_LOGINF ("do_writemore finished with err %d", err);
  return ERR_OK;
}

/**
 * Send some data on a TCP pcb contained in a netconn
 * Called from netconn_write
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_write (struct common_pcb *cpcb, msg_write_buf * wmsg)
{
  struct tcp_pcb *tpcb = (struct tcp_pcb *) cpcb->conn->private_data;
  spl_netconn_t *conn = cpcb->conn;
  data_com_msg *m = MSG_ENTRY (wmsg, data_com_msg, buffer);

  pbuf_set_recycle_flg (wmsg->p, MBUF_HLD_BY_SPL);      /* release buf hold by app on abnormal exit */

  int tcpState = -1;

  /* if msg->conn is null, then return */
  if (NULL == conn)
    {
      NSPOL_LOGERR ("Invalid param]msg->conn=%p", conn);
      SET_MSG_ERR (m, ERR_VAL);
      goto err_return;
    }

  tcpState = tpcb->state;

  if ((SPL_ERR_IS_FATAL (conn->last_err))
      && !((ERR_CLSD == conn->last_err) && (CLOSE_WAIT == tcpState)))
    {
      SET_MSG_ERR (m, conn->last_err);
    }
  else
    {
      if (cpcb->type == SPL_NETCONN_TCP)
        {
          if (cpcb->current_msg != NULL)
            {
              if (NULL == cpcb->msg_head || NULL == cpcb->msg_tail)
                {
                  /* if possible only if connect is not finished and it's
                     blocking, then the current_msg is connect msg.
                   */
                  NSPOL_LOGERR ("netconn do_write msg is null]msg_type=%d",
                                cpcb->current_msg->param.minor_type);
                  goto err_return;
                }

              /* only msg_write_buf will be in msg_head-msg_tail queue */
              wmsg->next = NULL;
              cpcb->msg_tail->next = wmsg;
              cpcb->msg_tail = wmsg;
              (void) do_writemore (conn);
              NSTCP_LOGINF ("do_write finished....");
              return;
            }

          if (conn->state != SPL_NETCONN_NONE)
            {
              /* netconn is connecting, closing or in blocking write */
              NSPOL_LOGINF (TCP_DEBUG,
                            "msg->conn->state != SPL_NETCONN_NONE..netconn is connecting, "
                            "closing or in blocking write]conn=%p", conn);
              SET_MSG_ERR (m, ERR_INPROGRESS);
            }
          /* this means we should start to process this message */
          else if (tpcb != NULL)
            {
              conn->state = SPL_NETCONN_WRITE;

              /* set all the variables used by do_writemore */
              if (0 != cpcb->write_offset)
                {
                  NSPOL_LOGERR ("already writing or closing]conn=%p", conn);
                  goto err_return;
                }

              if (0 == wmsg->len)
                {
                  NSPOL_LOGERR ("msg->msg.w.len=0]conn=%p", conn);
                  goto err_return;
                }

              /* record the msg will be processed */
              cpcb->current_msg = m;
              if (cpcb->msg_head != NULL || cpcb->msg_tail != NULL)
                {
                  NSPOL_LOGERR ("error maybe lost mesg]conn=%p", conn);
                }
              cpcb->msg_head = cpcb->msg_tail = wmsg;
              wmsg->next = NULL;
              cpcb->write_offset = 0;

              (void) do_writemore (conn);

              if ((conn->snd_buf) > TCP_SNDLOWAT)
                {
                  if (cpcb->model == SOCKET_STACKX)
                    SPL_API_EVENT (conn, SPL_NETCONN_EVT_SENDPLUS, 1);
                }
              NSTCP_LOGINF ("do_write finished %d", conn->snd_buf);

              /* for both cases: if do_writemore was called, don't ACK the APIMSG
                 since do_writemore ACKs it! */
              return;
            }
          else
            {
              SET_MSG_ERR (m, ERR_CONN);
            }
        }
      else
        {
          SET_MSG_ERR (m, ERR_VAL);
        }
    }
  NSTCP_LOGINF ("do_write finished");

err_return:
  pbuf_free_safe (wmsg->p);
  ASYNC_MSG_FREE (m);

  return;
}

void
do_getsockname (struct common_pcb *cpcb, msg_getaddrname * amsg)
{
  spl_netconn_t *conn;
  struct tcp_pcb *tcp;
  struct udp_pcb *udp;
  struct sockaddr_in *paddr = (struct sockaddr_in *) &amsg->sock_addr;

  data_com_msg *m = MSG_ENTRY (amsg, data_com_msg, buffer);

  if ((NULL == cpcb) || !(conn = cpcb->conn))
    {
      NSTCP_LOGERR ("failed to get sock");
      paddr->sin_family = 0;
      paddr->sin_port = 0;
      paddr->sin_addr.s_addr = 0;
      SET_MSG_ERR (m, ERR_CONN);
      return;
    }

  NS_LOG_CTRL (LOG_CTRL_GETSOCKNAME, LOGTCP, "NSTCP", NSLOG_DBG,
               "cpcb=%p,conn=%p,cmd=%u", cpcb, cpcb->conn, amsg->cmd);

  paddr->sin_family = AF_INET;
  SET_MSG_ERR (m, ERR_OK);

  if (amsg->cmd == 0)
    {
      if (cpcb->type == SPL_NETCONN_TCP)
        {
          tcp = (struct tcp_pcb *) cpcb->conn->private_data;
          /* add judgement:(NETCONN_LISTEN == conn->state)  in following if words */
          /* If connect is not done in TCP then the remote address will not be there so need to update proper error code
             if the application call the getpeername in TCP mode */
          if ((SPL_NETCONN_LISTEN == conn->state)
              || (tcp->remote_ip.addr == 0) || (tcp->remote_port == 0))
            {
              paddr->sin_family = 0;
              paddr->sin_port = 0;
              paddr->sin_addr.s_addr = 0;
              SET_MSG_ERR (m, ERR_CONN);
            }
          else
            {
              paddr->sin_port = spl_htons (tcp->remote_port);
              paddr->sin_addr.s_addr = tcp->remote_ip.addr;
            }
        }
      else if (cpcb->type == SPL_NETCONN_UDP)
        {
          udp = (struct udp_pcb *) cpcb->conn->private_data;
          /* If connect is not done in UDP then the remote address will not be there so need to update proper error code
             if the application call the getpeername in UDP mode
           */
          if ((udp->remote_ip.addr == 0) || (udp->remote_port == 0))
            {
              paddr->sin_family = 0;
              paddr->sin_port = 0;
              paddr->sin_addr.s_addr = 0;
              SET_MSG_ERR (m, ERR_CONN);
            }
          else
            {
              paddr->sin_port = spl_htons (udp->remote_port);
              paddr->sin_addr.s_addr = udp->remote_ip.addr;
            }
        }
    }
  else
    {
      if (cpcb->type == SPL_NETCONN_TCP)
        {
          tcp = (struct tcp_pcb *) cpcb->conn->private_data;
          paddr->sin_port = spl_htons (tcp->local_port);
          paddr->sin_addr.s_addr = tcp->local_ip.addr;
        }
      else if (cpcb->type == SPL_NETCONN_UDP)
        {
          udp = (struct udp_pcb *) cpcb->conn->private_data;
          paddr->sin_port = spl_htons (udp->local_port);
          paddr->sin_addr.s_addr = udp->local_ip.addr;
        }
    }
}

/**
 * Close a TCP pcb contained in a netconn
 * Called from netconn_close
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_close (struct common_pcb *cpcb, msg_close * cmsg)
{
  spl_netconn_t *conn = cpcb->conn;
  data_com_msg *m = MSG_ENTRY (cmsg, data_com_msg, buffer);

  NSTCP_LOGDBG ("msg->conn=%p,state=%d", conn, conn->state);

  /* @todo: abort running write/connect? */
  if ((conn->state != SPL_NETCONN_NONE)
      && (conn->state != SPL_NETCONN_LISTEN)
      && (conn->state != SPL_NETCONN_CONNECT))
    {
      if (SPL_NETCONN_TCP != cpcb->type)
        {
          NSTCP_LOGERR ("msg->conn=%p,type=%d", conn, cpcb->type);
          return;
        }
      NSTCP_LOGWAR ("msg->conn=%p,state=%d", conn, conn->state);
      SET_MSG_ERR (m, ERR_INPROGRESS);
    }
  else if (cpcb->type == SPL_NETCONN_TCP)       //clear codeDEX warning , CID:24336, cpcb can't be null.
    {
      struct tcp_pcb *tpcb = (struct tcp_pcb *) cpcb->conn->private_data;

      if (tpcb == NULL)
        {
          NSTCP_LOGERR ("tpcb null msg->conn=%p,type=%d", conn, cpcb->type);
          return;
        }
      if ((cmsg->shut != SPL_NETCONN_SHUT_RDWR)
          && (conn->state == SPL_NETCONN_LISTEN))
        {
          /* LISTEN doesn't support half shutdown */
          NSTCP_LOGERR
            ("LISTEN doesn't support half shutdown!]conn=%p,pcb=%p,state=%d",
             conn, tpcb, conn->state);
          SET_MSG_ERR (m, ERR_CONN);
        }
      else
        {
          if (cmsg->shut & SPL_NETCONN_SHUT_RD)
            {
              /* Drain and delete mboxes */
              (void) netconn_drain (cpcb->type, conn);
              unlink_recv_ring (conn);
            }

          if (((NULL != cpcb->current_msg)
               && (conn->state != SPL_NETCONN_CONNECT))
              || (0 != cpcb->write_offset))
            {
              NSTCP_LOGERR
                ("already writing or closing]conn=%p,pcb=%p,offset=%zu,curmsg=%p",
                 conn, tpcb, cpcb->write_offset, cpcb->current_msg);
              SET_MSG_ERR (m, ERR_CONN);
              return;
            }

          if (conn->state == SPL_NETCONN_CONNECT)
            {
              if (cpcb->current_msg != NULL
                  && cpcb->current_msg->param.minor_type ==
                  SPL_API_DO_CONNECT)
                {
                  SET_MSG_ERR (m, ERR_RST);
                  SYNC_MSG_ACK (cpcb->current_msg);
                }
              else
                {
                  NSTCP_LOGINF
                    ("already in connecting]conn=%p,cpcb=%p,msg=%p", conn,
                     cpcb, cpcb->current_msg);
                }
              /* maybe need to clean cpcb->msg_head */
            }

          conn->state = SPL_NETCONN_CLOSE;
          cpcb->current_msg = m;
          (void) do_close_internal (cpcb, 1);

          if (SPL_NETCONN_SHUT_RD == cmsg->shut
              || SPL_NETCONN_SHUT_RDWR == cmsg->shut)
            {
              SPL_API_EVENT (conn, SPL_NETCONN_EVT_RCVPLUS, 1);
            }

          return;
        }
    }
  else
    {
      SET_MSG_ERR (m, ERR_VAL);
    }
}

/*trans kerner option to stackx option*/
int
ks_to_stk_opt (int opt)
{
  int stack_opt = opt;
  switch (opt)
    {
    case SO_DEBUG:
      stack_opt = SOF_DEBUG;
      break;
    case SO_ACCEPTCONN:
      stack_opt = SOF_ACCEPTCONN;
      break;
    case SO_BROADCAST:
      stack_opt = SOF_BROADCAST;
      break;
    case SO_KEEPALIVE:
      stack_opt = SOF_KEEPALIVE;
      break;
    case SO_REUSEADDR:
      stack_opt = SOF_REUSEADDR;
      break;
    case SO_DONTROUTE:
      stack_opt = SOF_DONTROUTE;
      break;
    case SO_USELOOPBACK:
      stack_opt = SOF_USELOOPBACK;
      break;
    case SO_OOBINLINE:
      stack_opt = SOF_OOBINLINE;
      break;
    default:
      stack_opt = opt;
      break;
    }
  return stack_opt;
}

void
do_get_tcpproto_getsockopt_internal (struct common_pcb *cpcb,
                                     msg_setgetsockopt * smsg)
{
  int optname;
  void *optval;
  struct tcp_pcb *tpcb = (struct tcp_pcb *) cpcb->conn->private_data;
  data_com_msg *m = MSG_ENTRY (smsg, data_com_msg, buffer);

  optname = smsg->optname;
  optval = &smsg->optval;

  switch (optname)
    {
    case SPL_TCP_NODELAY:
      *(int *) optval = tcp_nagle_disabled (tpcb);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "]fd=%d,TCP_NODELAY=%s",
                    cpcb->socket, (*(int *) optval) ? "on" : "off");
      break;

    case SPL_TCP_KEEPIDLE:
      *(int *) optval = (int) (tpcb->keep_idle / 1000);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "]fd=%d,SPL_TCP_KEEPIDLE=%d",
                    cpcb->socket, *(int *) optval);
      break;
    case SPL_TCP_KEEPINTVL:
      *(int *) optval = (int) (tpcb->keep_intvl / 1000);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "]fd=%d,SPL_TCP_KEEPINTVL=%d",
                    cpcb->socket, *(int *) optval);
      break;
    case SPL_TCP_KEEPCNT:
      *(int *) optval = (int) tpcb->keep_cnt;
      NSPOL_LOGDBG (SOCKETS_DEBUG, "]fd=%d,SPL_TCP_KEEPCNT=%d",
                    cpcb->socket, *(int *) optval);
      break;

    default:
      NSPOL_LOGDBG (SOCKETS_DEBUG, "unsupported]optname=%d", optname);
      SET_MSG_ERR (m, EOPNOTSUPP);
      break;
    }
}

void
do_get_ipproto_getsockopt_internal (struct common_pcb *cpcb,
                                    msg_setgetsockopt * smsg)
{
  int optname;
  u32_t optlen;
  void *optval;

  struct ip_pcb *ipcb = (struct ip_pcb *) (cpcb);
  data_com_msg *m = MSG_ENTRY (smsg, data_com_msg, buffer);

  optname = smsg->optname;
  optval = &smsg->optval;
  optlen = smsg->optlen;

  switch (optname)
    {
    case IP_TTL:
      *(int *) optval = ipcb->ttl;
      NSPOL_LOGDBG (SOCKETS_DEBUG, "]fd=%d,IP_TTL=%d",
                    cpcb->socket, *(int *) optval);
      break;
    case IP_TOS:
      smsg->optlen =
        (optlen < sizeof (u32_t)) ? sizeof (u8_t) : sizeof (u32_t);
      if (smsg->optlen == sizeof (u8_t))
        {
          *(u8_t *) optval = ipcb->tos;
        }
      else
        {
          *(u32_t *) optval = (u32_t) ipcb->tos;
        }
      NSPOL_LOGDBG (SOCKETS_DEBUG, "]fd=%d,IP_TOS=%d",
                    cpcb->socket, *(int *) optval);
      break;
    default:
      SET_MSG_ERR (m, ERR_VAL);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "unsupported]optname=%d", optname);
      break;
    }
}

void
do_get_solsocket_getsockopt_internal (struct common_pcb *cpcb,
                                      msg_setgetsockopt * smsg)
{
  int optname;
  void *optval;
  struct tcp_pcb *tpcb = NULL;
  data_com_msg *m = MSG_ENTRY (smsg, data_com_msg, buffer);
  struct tcp_pcb *pcb = (struct tcp_pcb *) cpcb->conn->private_data;

  optname = smsg->optname;
  optval = &smsg->optval;

  switch (optname)
    {
      /* The option flags */
    case SO_BROADCAST:
    case SO_KEEPALIVE:
    case SO_REUSEADDR:
      *(int *) optval =
        (ip_get_option ((struct ip_pcb *) cpcb, ks_to_stk_opt (optname))) ? 1
        : 0;
      NSPOL_LOGDBG (SOCKETS_DEBUG, "]fd=%d,optname=%d,optval=%s",
                    cpcb->socket, optname, (*(int *) optval ? "on" : "off"));
      break;
    case SO_ACCEPTCONN:
      tpcb = (struct tcp_pcb *) cpcb->conn->private_data;
      if ((smsg->optlen < sizeof (int)) || (NULL == tpcb))
        {
          SET_MSG_ERR (m, ERR_VAL);
          NSPOL_LOGDBG (SOCKETS_DEBUG,
                        "(SOL_SOCKET, SO_ACCEPTCONN) failed]fd=%d,tcp=%p,optlen=%u",
                        cpcb->socket, cpcb, smsg->optlen);
          break;
        }
      if (tpcb->state == LISTEN)
        {
          *(int *) optval = 1;
        }
      else
        {
          *(int *) optval = 0;
        }
      NSPOL_LOGDBG (SOCKETS_DEBUG,
                    "(SOL_SOCKET, SO_ACCEPTCONN)]fd=%d,optval=%d",
                    cpcb->socket, *(int *) optval);
      break;
    case SO_TYPE:
      switch (cpcb->type)
        {
        case SPL_NETCONN_RAW:
          *(int *) optval = SOCK_RAW;
          break;
        case SPL_NETCONN_TCP:
          *(int *) optval = SOCK_STREAM;
          break;
        case SPL_NETCONN_UDP:
          *(int *) optval = SOCK_DGRAM;
          break;
        default:               /* unrecognized socket type */
          *(int *) optval = cpcb->type;
          NSPOL_LOGDBG (SOCKETS_DEBUG,
                        "(SOL_SOCKET, SO_TYPE): unrecognized socket type]fd=%d,optval=%d",
                        cpcb->socket, *(int *) optval);
        }                       /* switch (sock->conn->type) */
      NSPOL_LOGDBG (SOCKETS_DEBUG, "(SOL_SOCKET, SO_TYPE)]fd=%d,optval=%d",
                    cpcb->socket, *(int *) optval);
      break;
    case SO_RCVTIMEO:
      (*(struct timeval *) optval).tv_sec =
        spl_netconn_get_recvtimeout (cpcb) / 1000;
      (*(struct timeval *) optval).tv_usec =
        (spl_netconn_get_recvtimeout (cpcb) % 1000) * 1000;
      NSPOL_LOGDBG (SOCKETS_DEBUG,
                    "stackx_getsockopt(SOL_SOCKET, SO_RCVTIMEO)]fd=%d,sec=%ld,usec=%ld",
                    cpcb->socket, (*(struct timeval *) optval).tv_sec,
                    (*(struct timeval *) optval).tv_usec);
      break;
    case SO_SNDTIMEO:
      (*(struct timeval *) optval).tv_sec =
        spl_netconn_get_sendtimeout (cpcb) / 1000;
      (*(struct timeval *) optval).tv_usec =
        (spl_netconn_get_sendtimeout (cpcb) % 1000) * 1000;
      NSPOL_LOGDBG (SOCKETS_DEBUG,
                    "(SOL_SOCKET, SO_SNDTIMEO)]fd=%dsec=%ld,usec=%ld",
                    cpcb->socket, (*(struct timeval *) optval).tv_sec,
                    (*(struct timeval *) optval).tv_usec);
      break;
    case SO_SNDBUF:
      {
        u16_t mss = (pcb->mss > TCP_MSS
                     || pcb->mss == 0) ? TCP_MSS : pcb->mss;
        *(int *) optval = spl_netconn_get_sendbufsize (cpcb->conn) * mss;
        /*If user has not configured any sendbuffer value then we should return minimum
           promissed for the connection based on the flow control. */
        if (*(int *) optval == 0)
          {
            *(int *) optval = CONN_TCP_MEM_MIN_LINE * mss;
          }
        NSPOL_LOGDBG (SOCKETS_DEBUG,
                      "(SOL_SOCKET, SO_SNDBUF)]fd=%d,optval=%d", cpcb->socket,
                      *(int *) optval);
        break;
      }
    case SO_NO_CHECK:
      *(int *) optval =
        (udp_flags ((struct udp_pcb *) cpcb->conn->private_data) &
         UDP_FLAGS_NOCHKSUM) ? 1 : 0;
      NSPOL_LOGDBG (SOCKETS_DEBUG, "(SOL_SOCKET, SO_NO_CHECK)fd=%d,optval=%d",
                    cpcb->socket, *(int *) optval);
      break;
    case SO_SNDLOWAT:
      *(int *) optval = 1;
      NSPOL_LOGDBG (SOCKETS_DEBUG, "(SOL_SOCKET, SO_SNDLOWAT)fd=%d,optval=%d",
                    cpcb->socket, *(int *) optval);
      break;

    case SO_RCVLOWAT:
      *(int *) optval = spl_netconn_get_reclowbufsize (cpcb);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "(SOL_SOCKET, SO_RCVLOWAT)fd=%d,optval=%d",
                    cpcb->socket, *(int *) optval);
      break;
    case SO_ERROR:
      *(int *) optval = GET_MSG_ERR (m);
      SET_MSG_ERR (m, 0);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "SOL_SOCKET]fd=%d,optval=%d", cpcb->socket,
                    *(int *) optval);
      break;
    default:
      SET_MSG_ERR (m, ERR_VAL);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "unsupported opt]optname=%d", optname);
      break;
    }
}

void
do_getsockopt_internal (struct common_pcb *cpcb, msg_setgetsockopt * smsg)
{
  int level;

  if (NULL == smsg)
    {
      NSTCP_LOGERR ("arg NULL");
      return;
    }

  data_com_msg *m = MSG_ENTRY (smsg, data_com_msg, buffer);

  level = smsg->level;
  switch (level)
    {
      /* Level: SOL_SOCKET */
    case SOL_SOCKET:
      do_get_solsocket_getsockopt_internal (cpcb, smsg);

      break;

      /* Level: IPPROTO_IP */
    case IPPROTO_IP:
      do_get_ipproto_getsockopt_internal (cpcb, smsg);

      break;
      /* Level: IPPROTO_TCP */
    case IPPROTO_TCP:
      do_get_tcpproto_getsockopt_internal (cpcb, smsg);

      break;
    default:
      SET_MSG_ERR (m, ERR_VAL);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "unsupported level]level=%d", level);
      break;
    }                           /* switch (level) */
}

void
do_tcpsock_setsockopt (spl_netconn_t * conn, msg_setgetsockopt * smsg)
{
  const void *optval;
  int optname;
  struct common_pcb *cpcb = (struct common_pcb *) conn->comm_pcb_data;
  struct tcp_pcb *tpcb = (struct tcp_pcb *) conn->private_data;

  data_com_msg *m = MSG_ENTRY (smsg, data_com_msg, buffer);

  optname = smsg->optname;
  optval = &smsg->optval;

  switch (optname)
    {
    case SPL_TCP_NODELAY:
      if (*(int *) optval)
        {
          tcp_nagle_disable (tpcb);
        }
      else
        {
          tcp_nagle_enable (tpcb);
        }

      NSTCP_LOGDBG ("IPPROTO_TCP, TCP_NODELAY]fd=%d,optval=%s",
                    cpcb->socket, (*(int *) optval) ? "on" : "off");
      break;

    case SPL_TCP_KEEPIDLE:
      /* CID 52121 (#2 of 3): Other violation (HW_VPP_C_FIND_OPERATORS_2_1_1_3) */
      if ((u32_t) (*(int *) optval) >= INT_MAX / 1000)
        {
          NSTCP_LOGWAR ("optval too big]optval=%u",
                        (u32_t) (*(int *) optval));
          *(int *) optval = INT_MAX / 1000;
        }

      tpcb->keep_idle = 1000 * (u32_t) (*(int *) optval);

      if (tpcb->keep_idle > TCP_KEEPIDLE_DEFAULT)
        {
          NSTCP_LOGWAR
            ("(%d, IPPROTO_TCP, SPL_TCP_KEEPIDLE. value bigger than 7200000UL so setting to default 7200000UL) requested is -> %"
             U32_F " ", cpcb->socket, tpcb->keep_idle);
          tpcb->keep_idle = TCP_KEEPIDLE_DEFAULT;
        }

      NSTCP_LOGDBG ("IPPROTO_TCP, SPL_TCP_KEEPIDLE](fd=%d,keep_idle=%" U32_F
                    " ", cpcb->socket, tpcb->keep_idle);

      /* Not required lwip_slowtmr will take care about timer. */
      break;
    case SPL_TCP_KEEPINTVL:
      /* CID 52121 (#1 of 3): Other violation (HW_VPP_C_FIND_OPERATORS_2_1_1_3) */
      if ((u32_t) (*(int *) optval) >= INT_MAX / 1000)
        {
          NSTCP_LOGWAR ("optval too big]optval=%u",
                        (u32_t) (*(int *) optval));
          *(int *) optval = INT_MAX / 1000;
        }

      tpcb->keep_intvl = 1000 * (u32_t) (*(int *) optval);

      if (tpcb->keep_intvl > TCP_KEEPIDLE_DEFAULT)      /*max timer value supported */
        {
          NSTCP_LOGWAR
            ("(%d, IPPROTO_TCP, SPL_TCP_KEEPINTVL. value bigger than 7200000UL so setting to default 7200000UL) requested is -> %"
             U32_F " ", cpcb->socket, tpcb->keep_intvl);
          tpcb->keep_intvl = TCP_KEEPIDLE_DEFAULT;
        }
      NSTCP_LOGDBG ("IPPROTO_TCP, SPL_TCP_KEEPINTVL]fd=%d,keep_intvl=%" U32_F
                    " ", cpcb->socket, tpcb->keep_intvl);
      break;
    case SPL_TCP_KEEPCNT:
      tpcb->keep_cnt = (u32_t) (*(int *) optval);
      NSTCP_LOGDBG ("IPPROTO_TCP, SPL_TCP_KEEPCNT]fd=%d,keep_cnt=%" U32_F " ",
                    cpcb->socket, tpcb->keep_cnt);
      break;
    default:
      SET_MSG_ERR (m, ERR_VAL);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "unsupported]optname=%d", optname);
      break;
    }
}

void
do_ipsock_setsockopt (struct common_pcb *cpcb, msg_setgetsockopt * smsg)
{
  const void *optval;
  u32_t optlen;
  int optname;

  data_com_msg *m = MSG_ENTRY (smsg, data_com_msg, buffer);

  struct udp_pcb *upcb = (struct udp_pcb *) cpcb->conn->private_data;
  struct ip_pcb *ipcb = (struct ip_pcb *) upcb;

  optname = smsg->optname;
  optval = &smsg->optval;
  optlen = smsg->optlen;

  int val = 0;
  int bNotAllowSet = 0;
  switch (optname)
    {
    case IP_TTL:
      ipcb->ttl = (u8_t) (*(int *) optval);
      NSIP_LOGDBG ("IPPROTO_IP,IP_TTL]fd=%d,ttl=%u", cpcb->socket, ipcb->ttl);
      break;
    case IP_TOS:
      bNotAllowSet = (cpcb->dataSentFlag != 0)
        && ((SPL_NETCONN_TCP == cpcb->type)
            || (SPL_NETCONN_UDP == cpcb->type)
            || (SPL_NETCONN_RAW == cpcb->type));
      /*not allow set tos value when sending data */
      if (bNotAllowSet)
        {
          SET_MSG_ERR (m, ERR_VAL);
          break;
        }

      if (optlen >= sizeof (u32_t))
        {
          val = (int) (*(const int *) optval);
        }
      else if (optlen >= sizeof (u8_t))
        {
          val = (int) (*(const u8_t *) optval);
        }

      if (SPL_NETCONN_TCP == cpcb->type)
        {
          val &= ~INET_ECN_MASK;
          val |= ipcb->tos & INET_ECN_MASK;
        }
      ipcb->tos = (u8_t) (val);

      smsg->msg_box = (get_msgbox ((u8_t) val))->llring;

      NSIP_LOGDBG ("IPPROTO_IP,IP_TOS]]fd=%d,tos=%u", cpcb->socket,
                   ipcb->tos);
      break;
    default:
      SET_MSG_ERR (m, ERR_VAL);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "unsupported]optname=%d", optname);
      break;
    }
}

 /*Made seperate functions to reduce code complexity */
void
do_setsockopt_recvtimeout_internal (const void *optval,
                                    struct common_pcb *cpcb)
{
  if ((*(struct timeval *) optval).tv_sec < 0)
    {
      spl_netconn_set_recvtimeout (cpcb, 0);
    }
  else
    {
      spl_netconn_set_recvtimeout (cpcb, MAX_WAIT_TIMEOUT);
      if ((*(struct timeval *) optval).tv_sec != 0
          || (*(struct timeval *) optval).tv_usec != 0)
        {
          if ((*(struct timeval *) optval).tv_sec <
              ((MAX_WAIT_TIMEOUT / 1000) - 1))
            {
              spl_netconn_set_recvtimeout (cpcb,
                                           (*(struct timeval *) optval).tv_sec
                                           * 1000 +
                                           (*(struct timeval *)
                                            optval).tv_usec / 1000);
            }
        }
    }

  NSPOL_LOGDBG (SOCKETS_DEBUG,
                "SOL_SOCKET, SO_RCVTIMEO]conn=%p,fd=%d,optval=%d", cpcb,
                cpcb->socket, cpcb->recv_timeout);
}

void
do_setsockopt_sndtimeout_internal (const void *optval,
                                   struct common_pcb *cpcb)
{
  if ((*(struct timeval *) optval).tv_sec < 0)
    {
      spl_netconn_set_sendtimeout (cpcb, 0);
    }
  else
    {
      spl_netconn_set_sendtimeout (cpcb, MAX_WAIT_TIMEOUT);
      if ((*(struct timeval *) optval).tv_sec != 0
          || (*(struct timeval *) optval).tv_usec != 0)
        {
          if ((*(struct timeval *) optval).tv_sec <
              ((MAX_WAIT_TIMEOUT / 1000) - 1))
            {
              spl_netconn_set_sendtimeout (cpcb,
                                           (*(struct timeval *) optval).tv_sec
                                           * 1000 +
                                           (*(struct timeval *)
                                            optval).tv_usec / 1000);
            }
        }
    }

  NSPOL_LOGDBG (SOCKETS_DEBUG,
                "SOL_SOCKET, SO_SNDTIMEO]conn=%p,fd=%d,optval=%d", cpcb,
                cpcb->socket, cpcb->send_timeout);
}

#define IS_TCP_PCB(cpcb) (cpcb && (cpcb->conn) && (SPL_NETCONN_TCP == cpcb->conn->type))
NSTACK_STATIC inline void
set_opt_sol_socket (struct common_pcb *cpcb, struct tcp_pcb *pcb,
                    data_com_msg * m, int level, int optname,
                    const void *optval)
{
  switch (optname)
    {
      /* The option flags */
    case SO_BROADCAST:

      /* UNIMPL case SO_DEBUG: */
      /* UNIMPL case SO_DONTROUTE: */
    case SO_KEEPALIVE:
    case SO_REUSEADDR:

      if (NULL == pcb)
        {
          NSPOL_LOGERR ("conn->pcb.ip is null");
          break;
        }

      if (*(const int *) optval)
        {
          ip_set_option ((struct ip_pcb *) pcb, ks_to_stk_opt (optname));
        }
      else
        {
          ip_reset_option ((struct ip_pcb *) pcb, ks_to_stk_opt (optname));
        }

      NSPOL_LOGDBG (SOCKETS_DEBUG, "]conn=%p,fd=%d,optname=0x%x,optval=%s",
                    cpcb, cpcb->socket, optname,
                    (*(const int *) optval ? "on" : "off"));

      /* No use now, since tcp_slowtmr will take care about not sending/sending of keepalive */
      break;

    case SO_RCVLOWAT:
      spl_netconn_set_reclowbufsize (cpcb, *(int *) optval);
      NSPOL_LOGDBG (SOCKETS_DEBUG,
                    "SOL_SOCKET, SO_RCVLOWAT]conn=%p,fd=%d,optval=%d", cpcb,
                    cpcb->socket, cpcb->sk_rcvlowat);
      break;

    case SO_RCVTIMEO:
      /*to reduce code complexity */
      do_setsockopt_recvtimeout_internal (optval, cpcb);
      break;

    case SO_SNDTIMEO:
      /*to reduce code complexity */
      do_setsockopt_sndtimeout_internal (optval, cpcb);
      break;

    case SO_SNDBUF:
      {
        /* udp pcb invalid access will cause coredump */
        if (!IS_TCP_PCB (cpcb))
          {
            NSPOL_LOGDBG (SOCKETS_DEBUG,
                          "not support for non tcp socket]optname=%d, level=%d",
                          optname, level);
            return;
          }

        u16_t mss = (pcb->mss > TCP_MSS
                     || pcb->mss == 0) ? TCP_MSS : pcb->mss;

        if (*(int *) optval < (int) mss)
          {
            /*set value of one TCP_MSS */
            spl_netconn_set_sendbufsize (cpcb->conn, 1);
          }
        else
          {
            spl_netconn_set_sendbufsize (cpcb->conn,
                                         (*(int *) optval) / (int) mss);
          }

        NSPOL_LOGDBG (SOCKETS_DEBUG,
                      "SOL_SOCKET, SO_SNDBUF]conn=%p,fd=%d,optval=%d", cpcb,
                      cpcb->socket, cpcb->conn->send_bufsize * mss);
        break;
      }

    case SO_NO_CHECK:
      /* How udp is coming here.. ??  @TODO: May be move to someother function.? */
      /* solve segment issue when the PCB is not exist */
      if (NULL == cpcb)
        {
          NSPOL_LOGERR ("conn->pcb.udp is null");
          break;
        }

      struct udp_pcb *upcb = (struct udp_pcb *) pcb;

      if (*(int *) optval)
        {
          udp_setflags (upcb, udp_flags (upcb) | UDP_FLAGS_NOCHKSUM);
        }
      else
        {
          udp_setflags (upcb, udp_flags (upcb) & ~UDP_FLAGS_NOCHKSUM);
        }

      NSPOL_LOGDBG (SOCKETS_DEBUG,
                    "SOL_SOCKET, SO_NO_CHECK]conn=%p,fd=%d,optval=0x%x", cpcb,
                    cpcb->socket, upcb->flags);
      break;

    default:
      SET_MSG_ERR (m, ERR_VAL);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "unsupported]optname=%d", optname);
      break;
    }                           /* switch (optname) */

}

void
do_setsockopt_internal (struct common_pcb *cpcb, msg_setgetsockopt * smsg)
{
  int level, optname;
  const void *optval;

  data_com_msg *m = MSG_ENTRY (smsg, data_com_msg, buffer);
  struct tcp_pcb *pcb = (struct tcp_pcb *) cpcb->conn->private_data;

  if (NULL == smsg)
    {
      NSTCP_LOGERR ("arg null!");
      return;
    }

  level = smsg->level;
  optname = smsg->optname;
  optval = &smsg->optval;

  switch (level)
    {
      /* Level: SOL_SOCKET */
    case SOL_SOCKET:
      set_opt_sol_socket (cpcb, pcb, m, SOL_SOCKET, optname, optval);
      break;

      /* Level: IPPROTO_IP */
    case IPPROTO_IP:

      if (NULL == cpcb)
        {
          NSPOL_LOGERR ("conn->pcb.ip is null");
          break;
        }

      do_ipsock_setsockopt (cpcb, smsg);

      break;
      /* Level: IPPROTO_TCP */
    case IPPROTO_TCP:
      /* udp pcb invalid access will cause coredump */
      if (!IS_TCP_PCB (cpcb))
        {
          SET_MSG_ERR (m, ERR_VAL);
          NSPOL_LOGDBG (SOCKETS_DEBUG,
                        "not support for non tcp socket]optname=%d,level=%d",
                        optname, level);
          return;
        }

      if (NULL == cpcb)
        {
          NSPOL_LOGERR ("conn->pcb.tcp is null");
          break;
        }

      do_tcpsock_setsockopt (cpcb->conn, smsg);

      break;
    default:
      SET_MSG_ERR (m, ERR_VAL);
      NSPOL_LOGDBG (SOCKETS_DEBUG, "unsupported]level=%d", level);
      break;
    }                           /* switch (level) */
}

/*app send its version info to nStackMain */
void
do_app_touch (msg_app_touch * smsg)
{
  //write app version info to running.log
  NSPOL_LOGINF (SOCKETS_DEBUG, "hostpid=%u,app_version=%s", smsg->hostpid,
                smsg->app_version);
}

void
tcp_free_accept_ring (spl_netconn_t * conn)
{
  err_t de_err = ERR_OK;
  spl_netconn_t *newconn;
  while (1)
    {
      newconn = NULL;
      /* -1 means nonblocking */
      de_err = accept_dequeue (conn, (void **) &newconn, (u32_t) - 1);
      if (de_err == ERR_WOULDBLOCK || newconn == NULL)
        return;

      tcp_drop_conn (newconn);
    }
}

void
tcp_drop_conn (spl_netconn_t * conn)
{
  /* usually we should not access pcb by recv_obj, but no choice */
  struct tcp_pcb *pcb = (struct tcp_pcb *) conn->private_data;

  /* free conn first, even pcb is NULL */
  free_conn_by_spl (conn);

  if (pcb == NULL)
    {
      NSTCP_LOGWAR
        ("a tcp connection in accept queue without pcb!]newconn=%p", conn);
      return;
    }

  /* tell peer conneciton is reset */
  NSTCP_LOGWAR ("sending RST in accept waiting queue!]pcb=%p", pcb);

  tcp_rst (pcb->snd_nxt, pcb->rcv_nxt, &pcb->local_ip, &pcb->remote_ip,
           pcb->local_port, pcb->remote_port);

  tcp_pcb_remove (&tcp_active_pcbs, pcb);
  memp_free (MEMP_TCP_PCB, pcb);
}

void
update_tcp_state (spl_netconn_t * conn, enum tcp_state state)
{
  spl_tcp_state_t spl_state;

  if (conn != NULL)
    {
      switch (state)
        {
        case CLOSED:
          spl_state = SPL_CLOSED;
          break;
        case LISTEN:
          spl_state = SPL_LISTEN;
          break;
        case SYN_SENT:
          spl_state = SPL_SYN_SENT;
          break;
        case SYN_RCVD:
          spl_state = SPL_SYN_RCVD;
          break;
        case ESTABLISHED:
          spl_state = SPL_ESTABLISHED;
          break;
        case FIN_WAIT_1:
          spl_state = SPL_FIN_WAIT_1;
          break;
        case FIN_WAIT_2:
          spl_state = SPL_FIN_WAIT_2;
          break;
        case CLOSE_WAIT:
          spl_state = SPL_CLOSE_WAIT;
          break;
        case CLOSING:
          spl_state = SPL_CLOSING;
          break;
        case LAST_ACK:
          spl_state = SPL_LAST_ACK;
          break;
        case TIME_WAIT:
          spl_state = SPL_TIME_WAIT;
          break;
        default:
          spl_state = SPL_CLOSED;
          break;
        }
      if (conn->tcp_state != spl_state)
        {
          conn->tcp_state = spl_state;
          NSTCP_LOGINF ("conn=%p,private_data=%p,state=%d", conn,
                        conn->private_data, spl_state);
        }
    }
}

void
do_update_pcbstate ()
{
  struct tcp_pcb *tpcb;
  int i;
  for (i = 0; i < NUM_TCP_PCB_LISTS; i++)
    {
      for (tpcb = *tcp_pcb_lists[i]; tpcb != NULL; tpcb = tpcb->next)
        {
          if (tpcb->callback_arg)
            {
              update_tcp_state ((spl_netconn_t *) tpcb->callback_arg,
                                tpcb->state);
            }
        }
    }

  return;
}

void
init_stackx_lwip ()
{
  lwip_init ();
  sys_timeouts_init ();
  return;
}

void
free_common_pcb (struct common_pcb *cpcb)
{
  if (res_free (&cpcb->res_chk))
    {
      NSFW_LOGERR ("conn refree!]conn=%p", cpcb->conn);
      return;
    }

  common_pcb_reset (cpcb);

  mring_handle pool = p_def_stack_instance->cpcb_seg;
  if (nsfw_mem_ring_enqueue (pool, (void *) cpcb) != 1)
    {
      NSSBR_LOGERR ("nsfw_mem_ring_enqueue failed,this can not happen");
    }
  return;
}

struct common_pcb *
alloc_common_pcb (enum spl_netconn_type type)
{
  struct common_pcb *cpcb = NULL;

  if (nsfw_mem_ring_dequeue (p_def_stack_instance->cpcb_seg, (void **) &cpcb)
      != 1)
    {
      NSSBR_LOGERR ("malloc conn failed");
      return NULL;
    }

  NSFW_LOGINF ("alloc_common_pcb]cpcb=%p", cpcb);

  common_pcb_init (cpcb);
  cpcb->type = type;
  return cpcb;
}
