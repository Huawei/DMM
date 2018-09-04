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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "stackx_msg_handler.h"
#include "stackx_prot_com.h"
#include "common_pal_bitwide_adjust.h"
#include "stackx_err.h"
#include "nstack_securec.h"
#include "nsfw_rti.h"
//#include "stackx_custom.h"

#define FAST_SLEEP_TIME 10000
#define FAST_RETRY_COUNT 100
#define MAX_WAIT_TIMEOUT 0x7FFFFFFF

/*****************************************************************************
*   Prototype    : sbr_getsockopt_sol_socket
*   Description  : get sol socket
*   Input        : sbr_socket_t * sk
*                  int optname
*                  void * optval
*                  socklen_t optlen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_getsockopt_sol_socket (sbr_socket_t * sk, int optname, void *optval,
                           socklen_t optlen)
{
  int err = 0;

  switch (optname)
    {
    case SO_ERROR:
      {
        if (optlen < sizeof (int))
          {
            return EINVAL;
          }

        /* only overwrite ERR_OK or tempoary errors */
        err = sbr_get_sk_errno (sk);

        if ((0 == err) || (EINPROGRESS == err))
          {
            err =
              sbr_spl_err_to_errno (ss_get_last_errno (sbr_get_conn (sk)));
            sbr_set_sk_errno (sk, err);
          }
        else
          {
            sbr_set_sk_errno (sk, 0);
          }

        *(int *) optval = sbr_get_sk_errno (sk);

        return 0;
      }
    case SO_BROADCAST:
    case SO_KEEPALIVE:
    case SO_RCVBUF:
    case SO_SNDBUF:
    case SO_REUSEADDR:
      if (optlen < sizeof (int))
        {
          err = EINVAL;
        }

      break;
    case SO_RCVTIMEO:
    case SO_SNDTIMEO:
      if (optlen < sizeof (struct timeval))
        {
          err = EINVAL;
        }

      break;
    case SO_LINGER:
      if (optlen < sizeof (struct linger))
        {
          err = EINVAL;
        }

      break;
    default:
      err = ENOPROTOOPT;
      break;
    }

  return err;
}

/*****************************************************************************
*   Prototype    : sbr_getsockopt_ipproto_ip
*   Description  : get ipproto ip
*   Input        : int optname
*                  void * optval
*                  socklen_t optlen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_getsockopt_ipproto_ip (int optname, void *optval, socklen_t optlen)
{
  int err = 0;

  switch (optname)
    {
    case IP_TOS:
      if (optlen < sizeof (u8))
        {
          err = EINVAL;
        }

      break;
    default:
      err = ENOPROTOOPT;
      break;
    }

  return err;
}

/*****************************************************************************
*   Prototype    : sbr_pick_timeout
*   Description  : pick time
*   Input        : const void * optval
*                  socklen_t optlen
*                  i32* timeout
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_pick_timeout (const void *optval, socklen_t optlen, i32 * timeout)
{
  if (optlen < sizeof (struct timeval))
    {
      return EINVAL;
    }

  struct timeval *time_val = (struct timeval *) optval;
  if ((time_val->tv_usec < 0) || (time_val->tv_usec > USEC_TO_SEC))
    {
      return EDOM;
    }
  else
    {
      if (time_val->tv_sec < 0)
        {
          *timeout = 0;
        }
      else
        {
          *timeout = MAX_WAIT_TIMEOUT;
          if ((time_val->tv_sec != 0) || (time_val->tv_usec != 0))
            {
              if (time_val->tv_sec < ((MAX_WAIT_TIMEOUT / 1000) - 1))
                {
                  *timeout =
                    time_val->tv_sec * 1000 + time_val->tv_usec / 1000;
                }
            }
        }
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_setsockopt_sol_socket
*   Description  : set sol socket
*   Input        : sbr_socket_t * sk
*                  int optname
*                  const void * optval
*                  socklen_t optlen
*                  netconn_type_t type
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_setsockopt_sol_socket (sbr_socket_t * sk, int optname, const void *optval,
                           socklen_t optlen, spl_netconn_type_t type)
{
  int err = 0;

  switch (optname)
    {
    case SO_REUSEADDR:
    case SO_BROADCAST:
    case SO_KEEPALIVE:
    case SO_RCVBUF:
    case SO_SNDBUF:
      if (optlen < sizeof (int))
        {
          err = EINVAL;
        }

      break;
    case SO_RCVTIMEO:
      err =
        sbr_pick_timeout (optval, optlen,
                          &sbr_get_fd_share (sk)->recv_timeout);
      break;
    case SO_SNDTIMEO:
      err =
        sbr_pick_timeout (optval, optlen,
                          &sbr_get_fd_share (sk)->send_timeout);
      break;
    case SO_LINGER:
      if (optlen < sizeof (struct linger))
        {
          err = EINVAL;
        }

      break;
    default:
      err = ENOPROTOOPT;
      break;
    }

  return err;
}

/*****************************************************************************
*   Prototype    : sbr_setsockopt_ipproto_ip
*   Description  : set ipproto ip
*   Input        : int optname
*                  const void * optval
*                  socklen_t optlen
*                  netconn_type_t type
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_setsockopt_ipproto_ip (int optname, const void *optval, socklen_t optlen,
                           spl_netconn_type_t type)
{
  int err = 0;

  switch (optname)
    {
    case IP_TOS:
      if (optlen < sizeof (u8))
        {
          err = EINVAL;
        }

      break;
    case IP_MULTICAST_TTL:
      if (optlen < sizeof (u8))
        {
          err = EINVAL;
          break;
        }

      if (type != SPL_NETCONN_UDP)
        {
          err = EAFNOSUPPORT;
          break;
        }

      break;
    case IP_MULTICAST_IF:
      if (optlen < sizeof (struct in_addr))
        {
          err = EINVAL;
          break;
        }

      if (type != SPL_NETCONN_UDP)
        {
          err = EAFNOSUPPORT;
          break;
        }

      break;
    case IP_MULTICAST_LOOP:
      if (optlen < sizeof (u8))
        {
          err = EINVAL;
          break;
        }

      if (type != SPL_NETCONN_UDP)
        {
          err = EAFNOSUPPORT;
          break;
        }

      break;
    default:
      err = ENOPROTOOPT;
      break;
    }

  return err;
}

/*****************************************************************************
*   Prototype    : sbr_dequeue_buf
*   Description  : dequeue buf
*   Input        : sbr_socket_t * sk
*                  void **buf
*                  i32 timeout
*                  u8 use_l4_ring
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_dequeue_buf (sbr_socket_t * sk, void **buf, i32 timeout)
{
  mring_handle ring = ss_get_recv_ring (sbr_get_conn (sk));

  struct timespec start, end;
  long timediff;
  long timediff_sec;
  long timeout_sec = (long) (timeout / 1000);
  unsigned int retry_count = 0;

  if (timeout > 0)
    {
      if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &start)))
        {
          NSSBR_LOGERR ("Failed to get time, errno = %d", errno);
        }
    }

  if (!ss_recv_ring_valid (sbr_get_conn (sk)))
    {
      NSSBR_LOGDBG ("ring is invalid]fd=%d", sk->fd);
      sbr_set_sk_io_errno (sk, ENOTCONN);
      return -1;
    }

  int dequeue_ret = 0;
  pid_t pid = get_sys_pid ();

  while (1)
    {
      if (ss_is_shut_rd (sbr_get_conn (sk)))
        {
          NSSBR_LOGDBG ("is shut rd]fd=%d", sk->fd);
          sbr_set_sk_io_errno (sk, EINVAL);
          return -1;
        }

      dequeue_ret = nsfw_mem_ring_dequeue (ring, buf);
      if (1 == dequeue_ret)
        {
          pbuf_set_recycle_flg ((struct spl_pbuf *) *buf, pid); /*release buf hold by app on abnormal exit */
          return 0;
        }
      else if (0 == dequeue_ret)
        {
          /*If the peer reset connect, try to receive data only once */
          if (ss_can_not_recv (sbr_get_conn (sk)))
            {
              NS_LOG_CTRL (LOG_CTRL_RECV_QUEUE_FULL, LOGSBR, "NSSBR",
                           NSLOG_WAR, "try to fetch one more time]fd=%d",
                           sk->fd);
                /**
                 * l4_ring will not be processed here as can_not_recv flag is
                 * set by TCP only.
                 */
              if (1 == nsfw_mem_ring_dequeue (ring, buf))
                {
                  pbuf_set_recycle_flg ((struct spl_pbuf *) *buf, pid);
                  return 0;
                }

              sbr_set_sk_io_errno (sk, ENOTCONN);
              return -1;
            }

          int err = ss_get_last_errno (sbr_get_conn (sk));
          if (SPL_ERR_IS_FATAL (err) || err == ERR_TIMEOUT)     /* have to handle ERR_TIMEOUT here, when TCP keepalive timeout. */
            {
              NS_LOG_CTRL (LOG_CTRL_RECV_QUEUE_FULL, LOGSBR, "NSSBR",
                           NSLOG_ERR, "connection fatal error!err=%d", err);

              /* l4_ring need to be handled in the future */
              if (1 == nsfw_mem_ring_dequeue (ring, buf))
                {
                  pbuf_set_recycle_flg ((struct spl_pbuf *) *buf, pid);
                  return 0;
                }

              sbr_set_sk_io_errno (sk, sbr_spl_err_to_errno (err));
              return -1;
            }

          if (0 > timeout)
            {
              sbr_set_sk_io_errno (sk, EWOULDBLOCK);
              return -1;
            }

          if (retry_count < FAST_RETRY_COUNT)
            {
              sys_sleep_ns (0, FAST_SLEEP_TIME);
              retry_count++;
            }
          else
            {
              sys_sleep_ns (0, sbr_get_fd_share (sk)->block_polling_time);
            }

          if (timeout > 0)
            {
              if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &end)))
                {
                  NSSBR_LOGERR ("Failed to get time, errno = %d", errno);
                }
              timediff_sec = end.tv_sec - start.tv_sec;
              if (timediff_sec >= timeout_sec)
                {
                  timediff = end.tv_nsec > start.tv_nsec ?
                    (timediff_sec * 1000) + (end.tv_nsec -
                                             start.tv_nsec) /
                    USEC_TO_SEC : (timediff_sec * 1000) -
                    ((start.tv_nsec - end.tv_nsec) / USEC_TO_SEC);
                  if (timediff > timeout)
                    {
                      NSSBR_LOGDBG ("recv timeout]fd=%d", sk->fd);
                      sbr_set_sk_io_errno (sk, EWOULDBLOCK);
                      return -1;
                    }
                }
            }
        }
      else
        {
          NSSBR_LOGERR ("dequeue failed]fd=%d", sk->fd);
          sbr_set_sk_io_errno (sk, EINVAL);
          return -1;
        }
    }
}

int
sbr_com_peak (sbr_socket_t * sk)
{
  NSSBR_LOGERR ("not implement]fd=%d", sk->fd);
  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_com_try_lock_recv
*   Description  : try lock recv
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_com_try_lock_recv (sbr_socket_t * sk)
{
#ifdef SBR_USE_LOCK
  return common_spinlock_try_lock_with_pid (&sbr_get_fd_share (sk)->recv_lock,
                                            get_sys_pid ());
#else
  return 1;
#endif
}

/*****************************************************************************
*   Prototype    : sbr_com_lock_common
*   Description  : lock common
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
sbr_com_lock_common (sbr_socket_t * sk)
{
#ifdef SBR_USE_LOCK
  while (!common_spinlock_try_lock_with_pid
         (&sbr_get_fd_share (sk)->common_lock, get_sys_pid ()))
    {
      sys_sleep_ns (0, 0);
    }
#endif

}

void
sbr_com_fork_parent (sbr_socket_t * sk, pid_t p)
{
  i32 ref = ss_inc_fork_ref (sbr_get_conn (sk));
  NSSBR_LOGINF ("inc fork ref] fd=%d, p=%d, ref=%d, conn=%p, private_data=%p",
                sk->fd, p, ref, sbr_get_conn (sk),
                sbr_get_conn (sk)->private_data);
}

void
sbr_com_fork_child (sbr_socket_t * sk, pid_t p, pid_t c)
{
  if (ss_add_pid (sbr_get_conn (sk), c) != 0)
    {
      NSSBR_LOGERR
        ("add pid failed] fd=%d, p=%d, c=%d, ref=%d, conn=%p, private_data=%p",
         sk->fd, p, c, ss_get_fork_ref (sbr_get_conn (sk)), sbr_get_conn (sk),
         sbr_get_conn (sk)->private_data);
    }
  else
    {
      NSSBR_LOGINF
        ("add pid ok] fd=%d, p=%d, c=%d, ref=%d, conn=%p, private_data=%p",
         sk->fd, p, c, ss_get_fork_ref (sbr_get_conn (sk)), sbr_get_conn (sk),
         sbr_get_conn (sk)->private_data);
    }
}

/*****************************************************************************
*   Prototype    : sbr_com_unlock_common
*   Description  : unlock common
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
sbr_com_unlock_common (sbr_socket_t * sk)
{
#ifdef SBR_USE_LOCK
  common_spinlock_unlock (&sbr_get_fd_share (sk)->common_lock);
#endif
}

/*****************************************************************************
*   Prototype    : sbr_com_free_recv_buf
*   Description  : free recv buf,can't free buf in app
*   Input        : sbr_socket_t * sk
*                  struct spl_pbuf *p
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
sbr_com_free_recv_buf (sbr_socket_t * sk, struct spl_pbuf *p)
{
  struct spl_pbuf *p_orig = p;
  if (p)
    {
      p->freeNext = NULL;
      p = (struct spl_pbuf *) ADDR_LTOSH (p);

      if (sbr_get_fd_share (sk)->recoder.totalLen > 0)
        {
          ((struct spl_pbuf *)
           ADDR_SHTOL (sbr_get_fd_share (sk)->recoder.tail))->freeNext = p;
          sbr_get_fd_share (sk)->recoder.tail = p;
        }
      else
        {
          sbr_get_fd_share (sk)->recoder.head = p;
          sbr_get_fd_share (sk)->recoder.tail = p;
        }

      sbr_get_fd_share (sk)->recoder.totalLen++;
    }

  /* send MSG only if it's a big packet or number of packets larger than 32 */
  if ((p_orig && p_orig->tot_len > MAX_RECV_FREE_LEN) ||
      (sbr_get_fd_share (sk)->recoder.totalLen >= MAX_RECV_FREE_BUF))
    {
      sbr_handle_free_recv_buf (sk);
    }

}

/*****************************************************************************
*   Prototype    : sbr_get_sockaddr_and_len
*   Description  : get addr and len
*   Input        : u16 port
*                  spl_ip_addr_t * ipaddr
*                  struct sockaddr * addr
*                  socklen_t * addrlen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_get_sockaddr_and_len (u16 port, spl_ip_addr_t * ipaddr,
                          struct sockaddr *addr, socklen_t * addrlen)
{
  int ret;
  struct sockaddr_in sin;

  ret = MEMSET_S (&sin, sizeof (sin), 0, sizeof (sin));
  if (0 != ret)
    {
      NSSBR_LOGERR ("MEMSET_S failed]ret=%d.", ret);
      return -1;
    }

  sin.sin_family = AF_INET;
  sin.sin_port = htons (port);
  inet_addr_from_ipaddr (&sin.sin_addr, ipaddr);
  if (*addrlen > sizeof (struct sockaddr))
    {
      *addrlen = sizeof (struct sockaddr);
    }

  if (*addrlen > 0)
    {
      ret = MEMCPY_S (addr, sizeof (struct sockaddr), &sin, *addrlen);
      if (0 != ret)
        {
          NSSBR_LOGERR ("MEMCPY_S failed]ret=%d", ret);

          return -1;
        }
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_com_set_app_info
*   Description  : set app info to netconn
*   Input        : sbr_socket_t * sk
*                  void* appinfo
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
sbr_com_set_app_info (sbr_socket_t * sk, void *appinfo)
{
  return;
}
