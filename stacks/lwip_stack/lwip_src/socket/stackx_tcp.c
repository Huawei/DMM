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

#include "stackx_prot_com.h"
#include "stackx_msg_handler.h"
#include "stackx_pbuf.h"
#include "stackx_epoll_api.h"
#include "nstack_securec.h"
#include "common_pal_bitwide_adjust.h"
#include "common_mem_mbuf.h"
#include "stackx_spl_share.h"
#include "stackx_err.h"
#include "stackx_cfg.h"
#include "spl_opt.h"
//#include "stackx_tcp_sctl.h"
#include "stackx_tcp_opt.h"
//#include "stackx_dfx_api.h"
#include "stackx_spl_msg.h"
#ifdef HAL_LIB
#else
#include "rte_memcpy.h"
#endif

static void
sbr_tcp_init_conn (spl_netconn_t * conn)
{
  conn->tcp_sndbuf = CONN_TCP_MEM_DEF_LINE;
  conn->conn_pool = sbr_get_conn_pool ();
}

/* need close after accept failed */
static void
sbr_tcp_accept_failed (sbr_socket_t * sk)
{
  (void) sbr_handle_close (sk, 0);
  sk->stack_obj = NULL;
  sk->sk_obj = NULL;
}

NSTACK_STATIC int
sbr_tcp_socket (sbr_socket_t * sk, int domain, int type, int protocol)
{
  int err = 0;

  if (sbr_malloc_conn_for_sk (sk, SPL_NETCONN_TCP) != 0)
    {
      return -1;
    }

  sbr_tcp_init_conn (sbr_get_conn (sk));

  err = sbr_handle_socket (sk, SPL_NETCONN_TCP, 0);
  if (0 == err)
    {
      /* Prevent automatic window updates, we do this on our own! */
      ss_set_noautorecved_flag (sbr_get_conn (sk), 1);

      ss_set_nonblock_flag (sbr_get_conn (sk), (type & O_NONBLOCK));

      ss_add_recv_event (sbr_get_conn (sk));

      ss_set_send_event (sbr_get_conn (sk), 1);

      NSSBR_LOGINF ("add write and read events]fd=%d", sk->fd);

      return 0;
    }
  else
    {
      sbr_free_conn_from_sk (sk);
      return err;
    }
}

NSTACK_STATIC int
sbr_tcp_bind (sbr_socket_t * sk, const struct sockaddr *name,
              socklen_t namelen)
{
  const struct sockaddr_in *addr_in = (const struct sockaddr_in *) name;
  NSSBR_LOGINF ("fd=%d,addr=%s,port=%d,conn=%p,private_data=%p", sk->fd,
                spl_inet_ntoa (addr_in->sin_addr), ntohs (addr_in->sin_port),
                sbr_get_conn (sk), ss_get_private_data (sbr_get_conn (sk)));
  spl_ip_addr_t local_addr;
  inet_addr_to_ipaddr (&local_addr, &addr_in->sin_addr);
  u16 local_port = addr_in->sin_port;
  return sbr_handle_bind (sk, &local_addr, ntohs (local_port));
}

NSTACK_STATIC int
sbr_tcp_listen (sbr_socket_t * sk, int backlog)
{
  ss_set_is_listen_conn (sbr_get_conn (sk), 1);
  return sbr_handle_listen (sk, backlog);
}

static inline int
sbr_tcp_recv_is_timeout (sbr_socket_t * sk, struct timespec *starttm)
{
  struct timespec currtm;
  i64 timediff_ms, timediff_sec;
  i64 timeout_thr_ms, timeout_thr_sec;

  timeout_thr_ms = sbr_get_fd_share (sk)->recv_timeout;
  if (0 == timeout_thr_ms)
    {
      return ERR_OK;
    }

  timeout_thr_sec = (i64) (timeout_thr_ms / 1000);

  /* Handle system time change side-effects */
  if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &currtm)))
    {
      NSSBR_LOGERR ("Failed to get time, errno = %d", errno);
    }

  timediff_sec = currtm.tv_sec - starttm->tv_sec;
  if (timediff_sec >= timeout_thr_sec)
    {
      timediff_ms = currtm.tv_nsec > starttm->tv_nsec ?
        (timediff_sec * 1000) + (currtm.tv_nsec -
                                 starttm->tv_nsec) /
        USEC_TO_SEC : (timediff_sec * 1000) -
        ((starttm->tv_nsec - currtm.tv_nsec) / USEC_TO_SEC);

      /*NOTE: if user configured the timeout as say 0.5 ms, then timediff value
         will be negetive if still 0.5 ms is not elapsed. this is intended and we should
         not typecast to any unsigned type during this below if check */
      if (timediff_ms > timeout_thr_ms)
        {
          sbr_set_sk_errno (sk, ETIMEDOUT);
          return ETIMEDOUT;
        }
    }

  return ERR_OK;
}

static inline int
sbr_tcp_wait_new_conn (sbr_socket_t * sk, void **new_buf)
{
  int ret = 0;
  int elem_num;
  u32 timeout_thr_sec;
  struct timespec starttm = { 0, 0 };
  unsigned int retry_count = 0;
  mring_handle ring = NULL;

#define FAST_SLEEP_TIME 10000
#define SLOW_SLEEP_TIME 500000
#define FAST_RETRY_COUNT 100

  ring = ss_get_recv_ring (sbr_get_conn (sk));  //clear codeDEX warning , CID:24284
  timeout_thr_sec = sbr_get_fd_share (sk)->recv_timeout / 1000;
  if (0 != timeout_thr_sec)
    {
      if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &starttm)))
        {
          NSSBR_LOGERR ("Failed to get time, errno = %d", errno);
        }
    }

  while (1)
    {
      if (ss_is_shut_rd (sbr_get_conn (sk)))
        {
          sbr_set_sk_errno (sk, EINVAL);
          ret = EINVAL;
          break;
        }

      elem_num = nsfw_mem_ring_dequeue (ring, new_buf);
      if (1 == elem_num)
        {
          break;
        }
      else if (0 == elem_num)
        {
          ret = sbr_tcp_recv_is_timeout (sk, &starttm);
          if (0 != ret)
            {
              break;
            }

          /* reduce CPU usage in blocking mode- Begin */
          if (retry_count < FAST_RETRY_COUNT)
            {
              sys_sleep_ns (0, FAST_SLEEP_TIME);
              retry_count++;
            }
          else
            {
              sys_sleep_ns (0, sbr_get_fd_share (sk)->block_polling_time);
            }

          continue;
        }
      else
        {
          sbr_set_sk_errno (sk, EINVAL);
          ret = EINVAL;
          break;
        }
    }

  return ret;
}

NSTACK_STATIC inline int
sbr_tcp_get_sockaddr (sbr_socket_t * sk, struct sockaddr *addr,
                      socklen_t * addrlen)
{
  int ret;
  spl_netconn_t *conn = sbr_get_conn (sk);

  ret = (addr
         && addrlen) ? sbr_get_sockaddr_and_len (ss_get_remote_port (conn),
                                                 ss_get_remote_ip (conn),
                                                 addr, addrlen) : 0;
  if (0 != ret)
    {
      sbr_set_sk_io_errno (sk, EINVAL);
      NSSBR_LOGERR ("sbr_tcp_get_sockaddr]fd=%d", sk->fd);
      return -1;
    }

  return 0;
}

static int
sbr_tcp_accept_socket (sbr_socket_t * sk, sbr_socket_t * new_sk,
                       struct sockaddr *addr, socklen_t * addrlen)
{
  int err;
  spl_netconn_t *newconn = NULL;

  err = sbr_tcp_wait_new_conn (sk, (void **) &newconn);
  if (ERR_OK != err)
    {
      return err;
    }

  err = sbr_init_conn_for_accept (new_sk, newconn);
  if (ERR_OK != err)
    {
      /*When conn is null, return err; then do not need to free conn */
      return err;
    }

  err = sbr_tcp_get_sockaddr (new_sk, addr, addrlen);
  if (ERR_OK != err)
    {
      NSSBR_LOGERR ("sbr_get_sockaddr_and_socklen]ret=%d.", err);
      sbr_tcp_accept_failed (new_sk);
      return -1;
    }

  NSSBR_LOGINF
    ("return]listen fd=%d,listen conn=%p,listen private_data=%p,accept fd=%d,accept conn=%p,accept private_data=%p",
     sk->fd, sbr_get_conn (sk), ss_get_private_data (sbr_get_conn (sk)),
     new_sk->fd, sbr_get_conn (new_sk),
     ss_get_private_data (sbr_get_conn (new_sk)));

  //ip_addr_info_print(SOCKETS_DEBUG, &sbr_get_conn(new_sk)->share_remote_ip);

  /* test_epollCtl_003_001: Accept a conn. Add epoll_ctl with IN event and
     send a msg from peer. The event will be given once epoll_wait is called.
     Now, modify to IN|OUT. Calling epoll_event should return immediately since
     the socket is writable. Currently, returns 0 events.
     This issue is fixed here
   */

  /* Prevent automatic window updates, we do this on our own! */
  ss_set_noautorecved_flag (sbr_get_conn (new_sk), 1);

  ss_set_send_event (sbr_get_conn (new_sk), 1);

  /* don't set conn->last_err: it's only ERR_OK, anyway */
  return ERR_OK;
}

NSTACK_STATIC int
sbr_tcp_accept (sbr_socket_t * sk, sbr_socket_t * new_sk,
                struct sockaddr *addr, socklen_t * addrlen)
{
  int err;

  /* If conn is not in listen state then return failure with error code: EINVAL(22) */
  if (!ss_is_listen_state (sbr_get_conn (sk)))
    {
      NSSBR_LOGERR ("fd is not listening for connections]fd=%d,err=%d",
                    sk->fd, EINVAL);
      sbr_set_sk_errno (sk, EINVAL);

      return -1;
    }

  if (ss_is_nonblock_flag (sbr_get_conn (sk))
      && (0 >= ss_get_recv_event (sbr_get_conn (sk))))
    {
      NSSBR_LOGERR ("fd is nonblocking and rcvevent<=0]fd=%d,err=%d", sk->fd,
                    EWOULDBLOCK);
      sbr_set_sk_errno (sk, EWOULDBLOCK);

      return -1;
    }

  err = ss_get_last_errno (sbr_get_conn (sk));
  if (SPL_ERR_IS_FATAL (err))
    {
      /* don't recv on fatal errors: this might block the application task
         waiting on acceptmbox forever! */
      sbr_set_sk_errno (sk, sbr_spl_err_to_errno (err));

      return -1;
    }

  /* wait for a new connection */
  err = sbr_tcp_accept_socket (sk, new_sk, addr, addrlen);
  if (ERR_OK != err)
    {
      NSSBR_LOGERR ("sbr_tcp_accept_socket failed]fd=%d,err=%d", sk->fd, err);

      return -1;
    }

  /* Prevent automatic window updates, we do this on our own! */
  ss_sub_recv_event (sbr_get_conn (sk));

  sbr_set_sk_errno (sk, ERR_OK);

  /* test_epollCtl_003_001: Accept a conn. Add epoll_ctl with IN event and
     send a msg from peer. The event will be given once epoll_wait is called.
     Now, modify to IN|OUT. Calling epoll_event should return immediately since
     the socket is writable. Currently, returns 0 events.
     This issue is fixed here
   */

  ss_set_send_event (sbr_get_conn (new_sk), 1);

  return new_sk->fd;
}

NSTACK_STATIC int
sbr_tcp_accept4 (sbr_socket_t * sk, sbr_socket_t * new_sk,
                 struct sockaddr *addr, socklen_t * addrlen, int flags)
{
  int fd = sbr_tcp_accept (sk, new_sk, addr, addrlen);

  if (0 > fd)
    {
      return fd;
    }

  if (flags & O_NONBLOCK)
    {
      int opts = new_sk->fdopt->fcntl (new_sk, F_GETFL, 0);
      if (opts < 0)
        {
          NSSBR_LOGERR ("sbr_tcp_fcntl(sock,GETFL)");
          sbr_tcp_accept_failed (new_sk);
          return -1;
        }

      opts = opts | O_NONBLOCK;

      if (new_sk->fdopt->fcntl (new_sk, F_SETFL, opts) < 0)

        {
          NSSBR_LOGERR ("sbr_tcp_fcntl(sock,F_SETFL,opts)");
          sbr_tcp_accept_failed (new_sk);
          return -1;
        }
    }

  return new_sk->fd;
}

NSTACK_STATIC int
sbr_tcp_connect (sbr_socket_t * sk, const struct sockaddr *name,
                 socklen_t namelen)
{
  const struct sockaddr_in *addr_in = (const struct sockaddr_in *) name;

  NSSBR_LOGINF ("fd=%d,addr=%s,port=%d,conn=%p,private_data=%p", sk->fd,
                spl_inet_ntoa (addr_in->sin_addr), ntohs (addr_in->sin_port),
                sbr_get_conn (sk), ss_get_private_data (sbr_get_conn (sk)));
  spl_ip_addr_t remote_addr;

  inet_addr_to_ipaddr (&remote_addr, &addr_in->sin_addr);
  u16 remote_port = addr_in->sin_port;

  spl_ip_addr_t local_addr;
  if (IPADDR_ANY == ss_get_local_ip (sbr_get_conn (sk))->addr)
    {
      if (sbr_get_src_ip (remote_addr.addr, &local_addr.addr) != 0)
        {
          sbr_set_sk_errno (sk, EHOSTUNREACH);
          NSSBR_LOGERR ("get src ip failed]fd=%d", sk->fd);
          return -1;
        }
    }

  if (sbr_handle_connect (sk, &remote_addr, ntohs (remote_port), &local_addr)
      != 0)
    {
      NSSBR_LOGERR ("fail]fd=%d", sk->fd);
      return -1;
    }

  if (ss_is_shut_rd (sbr_get_conn (sk)))
    {
      NSSBR_LOGERR ("shut_rd]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ECONNRESET);
      return -1;
    }

  NSSBR_LOGINF ("succeeded]fd=%d", sk->fd);
  sbr_set_sk_errno (sk, ERR_OK);

  return 0;
}

static u8 netconn_shutdown_opt[] = {
  SPL_NETCONN_SHUT_RD,
  SPL_NETCONN_SHUT_WR,
  SPL_NETCONN_SHUT_RDWR
};

NSTACK_STATIC int
sbr_tcp_shutdown (sbr_socket_t * sk, int how)
{
  err_t err;

  if (ss_is_listen_state (sbr_get_conn (sk)))
    {
      return 0;
    }

  ss_set_shut_status (sbr_get_conn (sk), how);

  err = sbr_handle_shutdown (sk, netconn_shutdown_opt[how]);

  return (err == ERR_OK ? 0 : -1);
}

NSTACK_STATIC int
sbr_tcp_getsockname (sbr_socket_t * sk, struct sockaddr *name,
                     socklen_t * namelen)
{

  NSSBR_LOGINF ("sockname]fd=%d,tcp_state=%d", sk->fd,
                ss_get_tcp_state (sbr_get_conn (sk)));

  return sbr_handle_get_name (sk, name, namelen, SBR_GET_SOCK_NAME);
}

NSTACK_STATIC int
sbr_tcp_getpeername (sbr_socket_t * sk, struct sockaddr *name,
                     socklen_t * namelen)
{

  if (SPL_CLOSED == ss_get_tcp_state (sbr_get_conn (sk)))
    {
      NSSBR_LOGERR ("connection not exist]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ENOTCONN);
      return -1;
    }

  NSSBR_LOGINF ("peername]fd=%d,tcp_state=%d", sk->fd,
                ss_get_tcp_state (sbr_get_conn (sk)));

  return sbr_handle_get_name (sk, name, namelen, SBR_GET_PEER_NAME);
}

static int
sbr_getsockopt_ipproto_tcp (int optname, void *optval, socklen_t optlen)
{
  int err = ERR_OK;

  switch (optname)
    {
    case SPL_TCP_NODELAY:
      break;
    case SPL_TCP_KEEPIDLE:
    case SPL_TCP_KEEPINTVL:
    case SPL_TCP_KEEPCNT:
      break;
    default:
      err = ENOPROTOOPT;
      break;
    }

  return err;
}

NSTACK_STATIC int inline
sbr_tcp_set_sockopt_err (sbr_socket_t * sk, int err)
{
  if (ENOPROTOOPT == err)
    {
      return 0;
    }
  else
    {
      sbr_set_sk_errno (sk, err);
      return -1;
    }
}

NSTACK_STATIC int inline
sbr_tcp_get_sockopt_err (sbr_socket_t * sk, int err)
{
  sbr_set_sk_errno (sk, err);
  return -1;
}

NSTACK_STATIC int
sbr_tcp_getsockopt (sbr_socket_t * sk, int level, int optname, void *optval,
                    socklen_t * optlen)
{
  int err = 0;

  switch (level)
    {
    case SOL_SOCKET:
      err = sbr_getsockopt_sol_socket (sk, optname, optval, *optlen);
      break;

    case IPPROTO_IP:
      err = sbr_getsockopt_ipproto_ip (optname, optval, *optlen);
      break;

    case IPPROTO_TCP:
      err = sbr_getsockopt_ipproto_tcp (optname, optval, *optlen);
      break;

    case NSTACK_SOCKOPT:
      if ((optname == NSTACK_SEM_SLEEP) || (*optlen < sizeof (u32_t)))
        {
          *(u32_t *) optval =
            sbr_get_fd_share (sk)->block_polling_time / 1000;
          NSSOC_LOGINF
            ("tcp get recv sleep time success, usleep time = %d,fd = %d",
             sbr_get_fd_share (sk)->block_polling_time, sk->fd);
          return ERR_OK;
        }
      else
        {
          NSSOC_LOGINF ("get recv sleep time failed, fd = %d", sk->fd);
          sbr_set_sk_io_errno (sk, EINVAL);
          return -1;
        }
    default:
      err = ENOPROTOOPT;
      break;
    }

  if (0 != err)
    {
      NSSBR_LOGERR ("fail]fd=%d,level=%d,optname=%d,err=%d", sk->fd, level,
                    optname, err);
      /* for option not support ,getsockopt() should return fail */
      return sbr_tcp_get_sockopt_err (sk, err);
    }

  return sbr_handle_getsockopt (sk, level, optname, optval, optlen);
}

int
sbr_setsockopt_ipproto_tcp (int optname, socklen_t optlen)
{
  int err = 0;

  if (optlen < sizeof (int))
    {
      return EINVAL;
    }

  switch (optname)
    {
    case SPL_TCP_KEEPIDLE:
    case SPL_TCP_KEEPINTVL:
    case SPL_TCP_KEEPCNT:
      break;
    default:
      err = ENOPROTOOPT;
      break;
    }

  return err;
}

NSTACK_STATIC int
sbr_tcp_setsockopt (sbr_socket_t * sk, int level, int optname,
                    const void *optval, socklen_t optlen)
{
  int err = 0;

  switch (level)
    {
    case SOL_SOCKET:
      err =
        sbr_setsockopt_sol_socket (sk, optname, optval, optlen,
                                   SPL_NETCONN_TCP);
      break;
    case IPPROTO_IP:
      err =
        sbr_setsockopt_ipproto_ip (optname, optval, optlen, SPL_NETCONN_TCP);
      break;
    case IPPROTO_TCP:
      err = sbr_setsockopt_ipproto_tcp (optname, optlen);
      break;
    case NSTACK_SOCKOPT:
      {
        u32_t sleep_time = *(u32_t *) optval;
        /*sleep time should less than 1s */
        if ((optname == NSTACK_SEM_SLEEP) && (optlen >= sizeof (u32_t))
            && (sleep_time < 1000000))
          {
            sbr_get_fd_share (sk)->block_polling_time = sleep_time * 1000;
            NSSOC_LOGINF
              ("tcp set recv sleep time success, usleep time = %d,fd = %d",
               sbr_get_fd_share (sk)->block_polling_time, sk->fd);
            return ERR_OK;
          }
        else
          {
            NSSOC_LOGINF ("set recv sleep time failed, fd = %d", sk->fd);
            sbr_set_sk_io_errno (sk, EINVAL);
            return -1;
          }
      }

    default:
      err = ENOPROTOOPT;
      break;
    }

  if (0 != err)
    {
      NSSBR_LOGERR ("fail]fd=%d,level=%d,optname=%d,err=%d", sk->fd, level,
                    optname, err);

      return sbr_tcp_set_sockopt_err (sk, err);
    }

  return sbr_handle_setsockopt (sk, level, optname, optval, optlen);
}

static inline u16
sbr_tcp_mbuf_count (struct spl_pbuf *p)
{
  u16 count = 0;
  struct spl_pbuf *buf = p;
  struct common_mem_mbuf *mbuf;

  while (buf)
    {
      mbuf =
        (struct common_mem_mbuf *) ((char *) buf -
                                    sizeof (struct common_mem_mbuf));
      while (mbuf)
        {
          count++;
#ifdef HAL_LIB
#else
          mbuf = mbuf->next;
#endif
        }

      buf = (struct spl_pbuf *) ADDR_SHTOL (buf->next_a);
    }

  return count;
}

static inline void
sbr_tcp_free_recvbuf (sbr_socket_t * sk, struct spl_pbuf *p)
{
  int len;

  if (ss_is_noautorecved_flag (sbr_get_conn (sk)))
    {
      len = sbr_tcp_mbuf_count (p);
      sbr_handle_tcp_recv (sk, len, p);
    }
}

static inline void
sbr_tcp_recv_no_peak (sbr_socket_t * sk, struct spl_pbuf *buf, u32 buflen,
                      u32 copylen)
{
  if ((buflen - copylen) > 0)
    {
      sbr_get_fd_share (sk)->lastdata = (void *) ADDR_LTOSH (buf);
      sbr_get_fd_share (sk)->lastoffset += copylen;
    }
  else
    {
      sbr_get_fd_share (sk)->lastdata = 0;
      sbr_get_fd_share (sk)->lastoffset = 0;
      sbr_tcp_free_recvbuf (sk, buf);
    }
}

static inline int
sbr_tcp_recv_from_ring (sbr_socket_t * sk, struct spl_pbuf **buf, i32 timeout)
{
  int err;
  spl_netconn_t *conn = sbr_get_conn (sk);

  err = ss_get_last_errno (conn);
  if (SPL_ERR_IS_FATAL (err))
    {
      /* don't recv on fatal errors: this might block the application task
         waiting on recvmbox forever! */

      /* @todo: this does not allow us to fetch data that has been put into recvmbox
         before the fatal error occurred - is that a problem? */
      NSSBR_LOGDBG ("last err when recv:]=%d", err);
      if (ERR_CLSD != err)
        {
          sbr_set_sk_io_errno (sk, sbr_spl_err_to_errno (err));
          return -1;
        }
    }

  *buf = NULL;
  if (0 != sbr_dequeue_buf (sk, (void **) buf, timeout))
    {
      return -1;
    }

  ss_sub_recv_avail (sbr_get_conn (sk), (*buf)->tot_len);

  ss_sub_recv_event (sbr_get_conn (sk));

  return 0;
}

static inline int
sbr_tcp_recv_state_check (sbr_socket_t * sk)
{

  spl_tcp_state_t state = ss_get_tcp_state (sbr_get_conn (sk));

  NSSBR_LOGDBG ("tcp state when recv:]=%d", state);

  //close_wait state also can recive data
  //no connect cannot recive data
  if (SPL_ESTABLISHED > state)
    {
      if (SPL_SHUT_WR != ss_get_shut_status (sbr_get_conn (sk)))
        {
          /* after all data retrnasmission, connection is active */
          /* patch solution as last_err is not maintained properly */
          if ((SPL_CLOSED == state)
              && (ERR_TIMEOUT == ss_get_last_errno (sbr_get_conn (sk))))
            {
              sbr_set_sk_io_errno (sk, ETIMEDOUT);
            }
          else if ((SPL_CLOSED == state)
                   && (ERR_RST == ss_get_last_errno (sbr_get_conn (sk))))
            {
              sbr_set_sk_io_errno (sk, ECONNRESET);
            }
          else
            {
              sbr_set_sk_io_errno (sk, ENOTCONN);
            }

          return -1;
        }
    }

  return 0;
}

NSTACK_STATIC int
sbr_tcp_recvfrom (sbr_socket_t * sk, void *mem, size_t len, int flags,
                  struct sockaddr *from, socklen_t * fromlen)
{
  struct spl_pbuf *p;
  u32 buflen;
  u32 copylen;
  u32 off = 0;
  u8 done = 0;
  int para_len = len;
  int retval = 0;

  sbr_com_lock_recv (sk);
  NSSOC_LOGINF ("recv start, fd = %d last data %p flags to be set %d", sk->fd,
                ADDR_SHTOL (sbr_get_fd_share (sk)->lastdata), MSG_DONTWAIT);

  if (0 != sbr_tcp_recv_state_check (sk))
    {
      retval = -1;
      goto sbr_tcp_recvfrom_exit;
    }

  do
    {
      if (sbr_get_fd_share (sk)->lastdata)
        {
          p = ADDR_SHTOL (sbr_get_fd_share (sk)->lastdata);
        }
      else
        {
          if ((flags & MSG_DONTWAIT)
              || ss_is_nonblock_flag (sbr_get_conn (sk)))
            {
              NSSOC_LOGINF ("call ss_get_recv_event");
              if (0 >= ss_get_recv_event (sbr_get_conn (sk)))
                {
                  NSSOC_LOGINF ("no recv event]fd=%d", sk->fd);
                  sbr_set_sk_io_errno (sk, EWOULDBLOCK);
                  retval = -1;
                  goto sbr_tcp_recvfrom_exit;
                }
            }

          if (0 !=
              sbr_tcp_recv_from_ring (sk, &p,
                                      sbr_get_fd_share (sk)->recv_timeout))
            {
              /* already received data, return that */
              if (off > 0)
                {
                  sbr_set_sk_io_errno (sk, ERR_OK);
                  retval = off;
                  goto sbr_tcp_recvfrom_exit;
                }

              /* we tell the user the connection is closed by returning zero */
              if (sbr_get_sk_errno (sk) == ENOTCONN)
                {
                  retval = 0;
                  goto sbr_tcp_recvfrom_exit;
                }

              retval = -1;
              goto sbr_tcp_recvfrom_exit;
            }

          sbr_get_fd_share (sk)->lastdata = (void *) ADDR_LTOSH (p);
        }

      buflen = p->tot_len - sbr_get_fd_share (sk)->lastoffset;
      copylen = len > buflen ? buflen : len;

      if ((copylen > 0)
          && 0 == spl_pbuf_copy_partial (p, (u8 *) mem + off, copylen,
                                         sbr_get_fd_share (sk)->lastoffset))
        {
          NSSBR_LOGERR ("copy failed]fd=%d", sk->fd);
          sbr_set_sk_io_errno (sk, EFAULT);
          retval = -1;
          goto sbr_tcp_recvfrom_exit;
        }

      off += copylen;

      len -= copylen;

      if ((len == 0) || (ss_get_recv_event (sbr_get_conn (sk)) <= 0)
          || ((flags & MSG_PEEK) != 0))
        {
          if ((off >= sbr_get_fd_share (sk)->rcvlowat)
              || (para_len <= sbr_get_fd_share (sk)->rcvlowat))
            {
              done = 1;
            }
        }

      if (done)
        {
          if (sbr_tcp_get_sockaddr (sk, from, fromlen) != 0)
            {
              retval = -1;
              goto sbr_tcp_recvfrom_exit;
            }
        }

      /* If this is a TCP socket, check if there is data left in the buffer,
         If so, it should be saved in the sock structure for next  time around. */
      if (!(flags & MSG_PEEK))
        {
          sbr_tcp_recv_no_peak (sk, p, buflen, copylen);
        }
    }
  while (!done);

  retval = off;

  NSSOC_LOGINF ("recv done, fd = %d last data %p", sk->fd);
sbr_tcp_recvfrom_exit:

  NSSOC_LOGINF ("recv exit, fd = %d last data %p", sk->fd);
  sbr_com_unlock_recv (sk);
  return retval;
}

/*****************************************************************************
*   Prototype    : sbr_tcp_recvdata
*   Description  : recvdata
*   Input        : sbr_socket_t* sk
*                  const struct iovec* iov
*                  int iovcnt
*                  int flags
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_tcp_recvdata (sbr_socket_t * sk, const struct iovec *iov, int iovcnt)
{
  int max = SBR_MAX_INTEGER;
  int len = 0;
  int ret = 0;
  int i = 0;

  do
    {
      len += ret;

      if (!iov[i].iov_base || (0 == iov[i].iov_len))
        {
          ret = 0;
          continue;
        }

      ret = sbr_tcp_recvfrom (sk, (char *) iov[i].iov_base, iov[i].iov_len, 0,
                              NULL, NULL);
    }
  while ((ret == (int) iov[i].iov_len) && (iovcnt > (++i))
         && (max - len - ret > (int) iov[i].iov_len));

  if (len == 0)
    {
      return ret;
    }
  else
    {
      return (ret == -1 ? len : len + ret);
    }
}

/*****************************************************************************
*   Prototype    : sbr_tcp_readv
*   Description  : readv
*   Input        : sbr_socket_t* sk
*                  const struct iovec* iov
*                  int iovcnt
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_tcp_readv (sbr_socket_t * sk, const struct iovec *iov, int iovcnt)
{
  return sbr_tcp_recvdata (sk, iov, iovcnt);
}

/*****************************************************************************
*   Prototype    : sbr_tcp_recvmsg
*   Description  : recvmsg,unsupport flags
*   Input        : sbr_socket_t* sk
*                  struct msghdr* msg
*                  int flags
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_tcp_recvmsg (sbr_socket_t * sk, struct msghdr *msg, int flags)
{
  if (sbr_tcp_get_sockaddr
      (sk, (struct sockaddr *) msg->msg_name, &msg->msg_namelen) != 0)
    {
      return -1;
    }

  return sbr_tcp_recvdata (sk, msg->msg_iov, msg->msg_iovlen);
}

static int
sbr_tcp_send_is_timeout (sbr_socket_t * sk, struct timespec *starttm)
{
  struct timespec currtm;
  i64 timediff_ms, timediff_sec;
  i64 timeout_thr_ms, timeout_thr_sec;

  timeout_thr_ms = sbr_get_fd_share (sk)->send_timeout;
  if (0 == timeout_thr_ms)
    {
      return 0;
    }

  /* it is possible that starttm don't be inited,
     if send_timeout is change when this write function is running */
  timeout_thr_sec = (timeout_thr_ms + 240) >> 10;

  /* Handle system time change side-effects */
  if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &currtm)))
    {
      NSSBR_LOGERR ("Failed to get time, errno = %d", errno);
    }

  timediff_sec = currtm.tv_sec - starttm->tv_sec;
  if (timediff_sec >= timeout_thr_sec)
    {
      timediff_ms = currtm.tv_nsec > starttm->tv_nsec ?
        (timediff_sec * 1000) + (currtm.tv_nsec -
                                 starttm->tv_nsec) /
        USEC_TO_SEC : (timediff_sec * 1000) -
        ((starttm->tv_nsec - currtm.tv_nsec) / USEC_TO_SEC);

      /*NOTE: if user configured the timeout as say 0.5 ms, then timediff value
         will be negetive if still 0.5 ms is not elapsed. this is intended and we should
         not typecast to any unsigned type during this below if check */
      if (timediff_ms > timeout_thr_ms)
        {
          return 1;
        }
    }

  return 0;
}

static inline int
sbr_tcp_write_is_wait (sbr_socket_t * sk, struct timespec *starttm,
                       int noneblockFlg)
{
  if (noneblockFlg || sbr_tcp_send_is_timeout (sk, starttm))
    {
      return 0;
    }
  else
    {
      return 1;
    }
}

static struct spl_pbuf *
sbr_tcp_write_alloc_buf (sbr_socket_t * sk, size_t seglen, u8 api_flag,
                         struct timespec *starttm, u8 * errno_flag)
{
  spl_netconn_t *conn = sbr_get_conn (sk);

  struct spl_pbuf *curr_buf = NULL;
  size_t head_len = SPL_TCP_HLEN + SPL_PBUF_IP_HLEN + SPL_PBUF_LINK_HLEN;
  int noneblockFlg = (api_flag & SPL_NETCONN_DONTBLOCK)
    || ss_is_nonblock_flag (sbr_get_conn (sk));

  do
    {
      /* When packages are lost more than TCP_MAXRTX times,
       * conn will be closed and pcb will be removed. */
      if (ss_get_tcp_state (conn) == SPL_CLOSED)
        {
          NSSBR_LOGERR ("pcb SPL_CLOSED]conn=%p", conn);
          sbr_set_sk_io_errno (sk, ECONNABORTED);
          /* Must set errno_flag when set errno, to avoid errnno overwritten by sbr_tcp_write */
          *errno_flag = 1;
          return NULL;
        }

      curr_buf = sbr_malloc_tx_pbuf (seglen + head_len, head_len);
      if (NULL == curr_buf)
        {
          if (!sbr_tcp_write_is_wait (sk, starttm, noneblockFlg))
            {
              return NULL;
            }

          int err = ss_get_last_errno (sbr_get_conn (sk));
          if (SPL_ERR_IS_FATAL (err))
            {
              NSSBR_LOGERR ("connection fatal error!err=%d", err);
              sbr_set_sk_io_errno (sk, sbr_spl_err_to_errno (err));
              *errno_flag = 1;
              return NULL;
            }

          sched_yield ();
        }
    }
  while (curr_buf == NULL);

  return curr_buf;
}

static inline void
sbr_tcp_write_rel_buf (sbr_socket_t * sk, struct spl_pbuf *buf,
                       u32 thread_index)
{
  if (buf != NULL)
    {
      sbr_handle_free_send_buf (sk, buf);
    }
}

static inline int
sbr_tcp_write_fill_buf (const void *data, size_t * pos,
                        struct spl_pbuf *seg_buf, size_t seglen,
                        size_t optlen)
{
  size_t start = *pos;
  size_t copy = seglen;
  struct spl_pbuf *first = seg_buf;

  while ((0 < copy) && (NULL != first))
    {
      char *dst_ptr = PTR_SHTOL (char *, first->payload_a) + optlen;

      if (NULL ==
          common_memcpy (dst_ptr, (u8_t *) data + start, first->len - optlen))
        {
          NSSBR_LOGERR ("common_memcpy error]dst=%p,src=%p,len=%u",
                        dst_ptr, (u8_t *) data + start, first->len);
          return -1;
        }

      start += (first->len - optlen);
      copy -= (first->len - optlen);
      first = ADDR_SHTOL (first->next_a);
    }

  (*pos) = start;

  return 0;
}

static inline int
sbr_tcp_writev_fill_buf (const struct iovec *iov, size_t * iov_pos,
                         int *iov_var, size_t * pos, struct spl_pbuf *seg_buf,
                         size_t seglen, size_t optlen)
{
  size_t valid_copy_len;
  size_t iov_data_left;

  size_t copy = seglen;
  size_t start = *pos;
  size_t current_iov_pos = *iov_pos;
  int current_iov_var = *iov_var;

  u32 pbuf_offset = optlen;
  u32 pbuf_data_len;
  struct spl_pbuf *first = seg_buf;

  while ((0 < copy) && (NULL != first))
    {
      iov_data_left = iov[current_iov_var].iov_len - current_iov_pos;
      if (seglen == copy)
        {
          pbuf_offset = optlen;
        }

      pbuf_data_len = first->len - pbuf_offset;
      valid_copy_len =
        (iov_data_left > pbuf_data_len ? pbuf_data_len : iov_data_left);
      if (NULL ==
          common_memcpy ((char *) ADDR_SHTOL (first->payload_a) + pbuf_offset,
                         (u8_t *) iov[current_iov_var].iov_base +
                         current_iov_pos, valid_copy_len))
        {
          NSSBR_LOGERR
            ("common_memcpy error]current_iov_var=%d, dst=%p,src=%p,len=%zu",
             current_iov_var,
             (char *) ADDR_SHTOL (first->payload_a) + pbuf_offset,
             (u8_t *) iov[current_iov_var].iov_base + current_iov_pos,
             valid_copy_len);
          return -1;
        }

      start += valid_copy_len;
      copy -= valid_copy_len;

      if (iov_data_left == pbuf_data_len)
        {
          first = PTR_SHTOL (struct spl_pbuf *, first->next_a);
          pbuf_offset = optlen; //+= valid_copy_len;
          current_iov_var++;
          current_iov_pos = 0;
        }
      else if (iov_data_left > pbuf_data_len)
        {
          first = PTR_SHTOL (struct spl_pbuf *, first->next_a);
          pbuf_offset = optlen; //+= valid_copy_len;
          current_iov_pos += valid_copy_len;
        }
      else
        {
          pbuf_offset += valid_copy_len;

          current_iov_var++;
          current_iov_pos = 0;
        }
    }

  *iov_pos = current_iov_pos;
  *iov_var = current_iov_var;
  *pos = start;

  return 0;
}

static inline void
sbr_tcp_write_add_buf_to_list (struct spl_pbuf **p_head,
                               struct spl_pbuf **p_tail,
                               struct spl_pbuf *seg_buf, size_t seglen,
                               size_t optlen)
{
  seg_buf->len = seglen + optlen;
  seg_buf->tot_len = seglen + optlen;
  seg_buf->next_a = 0;

  /*put seg_buf after p_head */
  if (NULL == (*p_head))
    {
      (*p_head) = seg_buf;
      (*p_tail) = seg_buf;
    }
  else
    {
      (*p_tail)->next_a = ADDR_LTOSH (seg_buf);
      (*p_tail) = seg_buf;
    }
}

NSTACK_STATIC int
sbr_tcp_write (sbr_socket_t * sk, const void *data, size_t size, u8 api_flag,
               size_t * written)
{
  err_t err = -1;
  size_t pos = 0, left, seglen;
  u32 pbuf_seg_cnt = 0;
  u32 thread_index = 0;
  struct spl_pbuf *seg_buf = NULL;
  struct spl_pbuf *p_head = NULL;
  struct spl_pbuf *p_tail = p_head;
  struct spl_netconn *conn = sbr_get_conn (sk);
  u32 mss = ss_get_mss (sbr_get_conn (sk));

  if (0 == size)
    {
      NSSBR_LOGERR ("fd=%d,size=%u", sk->fd, (u32) size);
      return 0;
    }

  struct timespec ts;
  if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &ts)))
    {
      NSSBR_LOGERR ("Failed to get time, errno = %d", errno);
    }

  while (pos < size)
    {
      left = size - pos;
      seglen = left > mss ? mss : left;
      u8 errno_set = 0;
      seg_buf =
        sbr_tcp_write_alloc_buf (sk, seglen, api_flag, &ts, &errno_set);
      if (NULL == seg_buf)
        {
          NSSBR_LOGINF ("sbr_tcp_write_alloc_buf failed......");
          if (NULL != p_head)
            {
              err = sbr_handle_tcp_send (sk, size, p_head, api_flag);
              if (ERR_OK != err)
                {
                  NSSBR_LOGERR ("sbr_handle_tcp_send error]err(%d)", err);
                  goto err_ref_buf;
                }
            }

          if (0 == pos)
            {
              /* If errno is already set in sbr_tcp_write_alloc_buf, do not overwrite here */
              if (!errno_set)
                {
                  sbr_set_sk_io_errno (sk, EWOULDBLOCK);
                }
              return -1;
            }

          NSSBR_LOGDBG ("sent size %zu", pos);
          *written = pos;

          return ERR_OK;
        }

      if (0 != sbr_tcp_write_fill_buf (data, &pos, seg_buf, seglen, 0))
        {
          sbr_set_sk_io_errno (sk, EFAULT);
          NSSBR_LOGERR ("sbr_tcp_write_fill_buf error]");
          goto err_ref_buf;
        }

      sbr_tcp_write_add_buf_to_list (&p_head, &p_tail, seg_buf, seglen, 0);

      ++pbuf_seg_cnt;
      if (p_head
          && ((SPL_TCP_SEND_MAX_SEG_PER_MSG <= pbuf_seg_cnt)
              || (pos >= size)))
        {
          pbuf_seg_cnt = 0;
          err = sbr_handle_tcp_send (sk, size, p_head, api_flag);
          if (ERR_OK != err)
            {
              NSSBR_LOGERR ("sbr_handle_tcp_send error]err(%d)", err);
              goto err_ref_buf;
            }

          p_head = NULL;
        }
    }

  *written = size;

  (void) conn;
  return ERR_OK;

err_ref_buf:
  sbr_tcp_write_rel_buf (sk, p_head, thread_index);
  (void) conn;
  return -1;
}

NSTACK_STATIC int
sbr_tcp_writev (sbr_socket_t * sk, const struct iovec *iov, int iovcnt)
{
  err_t err = -1;
  int idx = 0;
  size_t pos = 0, left, seglen, optlen = 0;
  u32 pbuf_seg_cnt = 0;
  u32 thread_index = 0;
  size_t size = 0;
  size_t iov_pos = 0;
  int iov_var = 0;
  struct spl_pbuf *seg_buf = NULL;
  struct spl_pbuf *p_head = NULL;
  struct spl_pbuf *p_tail = p_head;
  struct spl_netconn *conn = sbr_get_conn (sk);
  u32 mss = ss_get_mss (sbr_get_conn (sk));

  if (mss <= optlen)
    {
      NSSBR_LOGERR ("mss invalid]mss=%u,optlen=%zu,fd=%d", mss, optlen,
                    sk->fd);
      sbr_set_sk_io_errno (sk, EINVAL);
      return -1;
    }

  /* mss dose't include the tcp options length */
  mss -= optlen;

  while (idx < iovcnt)
    {
      if (SBR_MAX_INTEGER - iov[idx].iov_len <= size)
        {
          size = SBR_MAX_INTEGER;
          break;
        }

      size += iov[idx].iov_len;
      idx++;
    }

  struct timespec starttm;
  if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &starttm)))
    {
      NSSBR_LOGERR ("Failed to get time, errno = %d", errno);
    }

  while (pos < size)
    {
      left = size - pos;

      seglen = left > mss ? mss : left;
      u8 errno_set = 0;
      seg_buf =
        sbr_tcp_write_alloc_buf (sk, seglen + optlen, SPL_NETCONN_COPY,
                                 &starttm, &errno_set);
      if (NULL == seg_buf)
        {
          if (NULL != p_head)
            {
              err = sbr_handle_tcp_send (sk, size, p_head, SPL_NETCONN_COPY);
              /*If errno is already set in sbr_tcp_write_alloc_buf, do not overwrite here */
              if (err != ERR_OK)
                {
                  NSSBR_LOGERR ("sbr_handle_tcp_send error]err(%d)", err);
                  goto err_ref_buf;
                }
            }

          /* [Start]
             1)Set SO_SNDTIMEO to 10
             2)Send a msg of larger buff size.and let the timeout happen for send (dont receive at peer side.)
             3)iRet will be 0 and  errno received will be 11 (EAGAIN).

             Above issue is fixed.
           */
          if (0 == pos)
            {
              if (!errno_set)
                {
                  sbr_set_sk_io_errno (sk, EWOULDBLOCK);
                }
              return -1;
            }
          /* [End] */

          NSSBR_LOGDBG ("sent size %zu", pos);

          return pos;
        }

      if (0 !=
          sbr_tcp_writev_fill_buf (iov, &iov_pos, &iov_var, &pos, seg_buf,
                                   seglen, optlen))
        {
          sbr_set_sk_io_errno (sk, EFAULT);
          NSSBR_LOGERR ("sbr_tcp_writev_fill_buf error]");
          goto err_ref_buf;
        }

      sbr_tcp_write_add_buf_to_list (&p_head, &p_tail, seg_buf, seglen,
                                     optlen);

      /* @todo: for non-blocking write, check if 'size' would ever fit into
         snd_queue or snd_buf */
      ++pbuf_seg_cnt;
      if (p_head
          && ((SPL_TCP_SEND_MAX_SEG_PER_MSG <= pbuf_seg_cnt)
              || (pos >= size)))
        {
          pbuf_seg_cnt = 0;
          err = sbr_handle_tcp_send (sk, size, p_head, SPL_NETCONN_COPY);
          if (ERR_OK != err)
            {
              NSSBR_LOGERR ("sbr_handle_tcp_send error]err(%d)", err);
              goto err_ref_buf;
            }

          p_head = NULL;
        }
    }
  (void) conn;
  return size;

err_ref_buf:
  sbr_tcp_write_rel_buf (sk, p_head, thread_index);
  (void) conn;
  return -1;
}

NSTACK_STATIC int
sbr_tcp_sendto (sbr_socket_t * sk, const void *data, size_t size, int flags,
                const struct sockaddr *to, socklen_t tolen)
{
  return sk->fdopt->send (sk, data, size, flags);
}

static inline int
sbr_tcp_send_state_check (sbr_socket_t * sk)
{
  if ((SPL_SHUT_WR == ss_get_shut_status (sbr_get_conn (sk)))
      || (SPL_SHUT_RDWR == ss_get_shut_status (sbr_get_conn (sk))))
    {
      sbr_set_sk_io_errno (sk, EPIPE);
      return -1;
    }

  spl_tcp_state_t state = ss_get_tcp_state (sbr_get_conn (sk));
  if ((SPL_ESTABLISHED != state) && (SPL_CLOSE_WAIT != state))
    {
      /* after all data retrnasmission, connection is active */
      /* patch solution as last_err is not maintained properly */
      if ((SPL_CLOSED == state)
          && (ERR_TIMEOUT == ss_get_last_errno (sbr_get_conn (sk))))
        {
          sbr_set_sk_io_errno (sk, ETIMEDOUT);
        }
      else if ((SPL_CLOSED == state)
               && (ERR_RST == ss_get_last_errno (sbr_get_conn (sk))))
        {
          sbr_set_sk_io_errno (sk, ECONNRESET);
        }
      else
        {
          sbr_set_sk_io_errno (sk, EPIPE);
        }

      return -1;
    }

  return 0;
}

NSTACK_STATIC int
sbr_tcp_send (sbr_socket_t * sk, const void *data, size_t size, int flags)
{
  int err;
  size_t written = 0;
  u8 write_flags;

  if (0 != sbr_tcp_send_state_check (sk))
    {
      NSSBR_LOGDBG ("tcp state not correct]fd=%d, err=%d", sk->fd,
                    sbr_get_sk_errno (sk));
      return -1;
    }

  write_flags = SPL_NETCONN_COPY |
    ((flags & MSG_MORE) ? SPL_NETCONN_MORE : 0) |
    ((flags & MSG_DONTWAIT) ? SPL_NETCONN_DONTBLOCK : 0);

  NSSBR_LOGINF ("Sbr tcp write start");
  err = sbr_tcp_write (sk, data, size, write_flags, &written);
  NSSBR_LOGINF ("Sbr tcp write end written %d", written);

  return (err == ERR_OK ? written : -1);
}

NSTACK_STATIC int
sbr_tcp_sendmsg (sbr_socket_t * sk, const struct msghdr *pmsg, int flags)
{
  if (0 != sbr_tcp_send_state_check (sk))
    {
      NSSBR_LOGDBG ("tcp state not correct]fd=%d, err=%d", sk->fd,
                    sbr_get_sk_errno (sk));
      return -1;
    }

  return sbr_tcp_writev (sk, pmsg->msg_iov, pmsg->msg_iovlen);
}

NSTACK_STATIC int
sbr_tcp_fcntl (sbr_socket_t * sk, int cmd, long arg)
{
  int ret = 0;

  switch (cmd)
    {
    case F_GETFL:
      ret = ss_get_nonblock_flag (sbr_get_conn (sk));
      NSSBR_LOGDBG ("F_GETFL]fd=%d,ret=%d", sk->fd, ret);
      break;

    case F_SETFL:
      if (arg & O_NONBLOCK)
        {
          NSSBR_LOGDBG ("F_SETFL set O_NONBLOCK val]fd=%d,arg=%ld", sk->fd,
                        arg);
          ss_set_nonblock_flag (sbr_get_conn (sk), (arg & O_NONBLOCK));
        }
      else
        {
          NSSBR_LOGDBG ("F_SETFL clean O_NONBLOCK val]fd=%d,arg=%ld", sk->fd,
                        arg);
          ss_set_nonblock_flag (sbr_get_conn (sk), 0);
        }

      break;

    default:
      NSSBR_LOGERR ("cmd is not support]fd=%d,cmd=%d", sk->fd, cmd);
      ret = -1;
      sbr_set_sk_errno (sk, EINVAL);
      break;
    }

  return ret;
}

NSTACK_STATIC int
sbr_tcp_ioctl (sbr_socket_t * sk, unsigned long cmd, void *arg)
{
  int ret = 0;
  int recv_avail;

  switch (cmd)
    {
    case FIONREAD:
      {
        if (ss_is_listen_state (sbr_get_conn (sk)))
          {
            ret = -1;
            sbr_set_sk_errno (sk, EINVAL);
            break;
          }

        recv_avail = ss_get_recv_avail (sbr_get_conn (sk));
        *((u32 *) arg) = recv_avail >= 0 ? recv_avail : 0;
        if (sbr_get_fd_share (sk)->lastdata)
          {
            struct spl_pbuf *buf =
              ADDR_SHTOL (sbr_get_fd_share (sk)->lastdata);
            *((u32 *) arg) +=
              (buf->tot_len - sbr_get_fd_share (sk)->lastoffset);
          }
      }

      break;

    case FIONBIO:
      {
        u8 val = 0;

        if (arg && *(u32 *) arg)
          {
            val = 1;
          }

        ss_set_nonblock_flag (sbr_get_conn (sk), val);
        NSSBR_LOGDBG ("FIONBIO]fd=%d,val=%u", sk->fd, val);
      }

      break;

    default:
      {
        NSSBR_LOGERR ("cmd is not support]fd=%d,cmd=%lu", sk->fd, cmd);
        ret = -1;
        sbr_set_sk_errno (sk, ENOTTY);
      }
      break;
    }

  return ret;
}

NSTACK_STATIC int
sbr_tcp_close (sbr_socket_t * sk)
{
  if (sbr_get_fd_share (sk)->lastdata)
    {
      struct spl_netbuf *buf = ADDR_SHTOL (sbr_get_fd_share (sk)->lastdata);
      struct spl_pbuf *p = (struct spl_pbuf *) ADDR_SHTOL (buf->p);
      sbr_tcp_free_recvbuf (sk, p);
    }

  return sbr_handle_close (sk, 0);
}

sbr_fdopt tcp_fdopt = {
  .socket = sbr_tcp_socket,
  .bind = sbr_tcp_bind,
  .listen = sbr_tcp_listen,
  .accept = sbr_tcp_accept,
  .accept4 = sbr_tcp_accept4,
  .connect = sbr_tcp_connect,
  .shutdown = sbr_tcp_shutdown,
  .getsockname = sbr_tcp_getsockname,
  .getpeername = sbr_tcp_getpeername,
  .getsockopt = sbr_tcp_getsockopt,
  .setsockopt = sbr_tcp_setsockopt,
  .recvfrom = sbr_tcp_recvfrom,
  .readv = sbr_tcp_readv,
  .recvmsg = sbr_tcp_recvmsg,
  .send = sbr_tcp_send,
  .sendto = sbr_tcp_sendto,
  .sendmsg = sbr_tcp_sendmsg,
  .writev = sbr_tcp_writev,
  .fcntl = sbr_tcp_fcntl,
  .ioctl = sbr_tcp_ioctl,
  .close = sbr_tcp_close,
  .peak = sbr_com_peak,
  .lock_common = sbr_com_lock_common,
  .unlock_common = sbr_com_unlock_common,
  .ep_getevt = stackx_eventpoll_getEvt,
  .ep_ctl = stackx_eventpoll_triggle,
  .set_close_stat = NULL,
};
