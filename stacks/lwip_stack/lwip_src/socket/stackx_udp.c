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
#include "stackx_err.h"
#include "nstack_securec.h"
#include "common_pal_bitwide_adjust.h"
#include "stackx_cfg.h"
#include <netinet/in.h>
#ifdef HAL_LIB
#else
#include "rte_memcpy.h"
#endif

#define SPL_PBUF_UDP_LEN (SPL_FRAME_MTU + SPL_PBUF_LINK_HLEN)
#define L2_L3_ROOM_LEN (SPL_PBUF_LINK_HLEN + SPL_PBUF_IP_HLEN)
#define L4_ROOM_LEN SPL_PBUF_UDP_HLEN

/*****************************************************************************
*   Prototype    : sbr_udp_socket
*   Description  : create socket
*   Input        : sbr_socket_t * sk
*                  int domain
*                  int type
*                  int protocol
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_socket (sbr_socket_t * sk, int domain, int type, int protocol)
{
  if (sbr_malloc_conn_for_sk (sk, SPL_NETCONN_UDP) != 0)
    {
      return -1;
    }

  int ret = sbr_handle_socket (sk, SPL_NETCONN_UDP, 0);
  if (ret != 0)
    {
      sbr_free_conn_from_sk (sk);
      return ret;
    }

  ss_set_nonblock_flag (sbr_get_conn (sk), (type & O_NONBLOCK));
  ss_set_send_event (sbr_get_conn (sk), 1);
  return ret;
}

/*****************************************************************************
*   Prototype    : sbr_udp_bind
*   Description  : udp bind
*   Input        : sbr_socket_t * sk
*                  const struct sockaddr * name
*                  socklen_t namelen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_udp_bind (sbr_socket_t * sk, const struct sockaddr *name,
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

/*****************************************************************************
*   Prototype    : sbr_udp_listen
*   Description  : unsupport
*   Input        : sbr_socket_t * sk
*                  int backlog
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_udp_listen (sbr_socket_t * sk, int backlog)
{
  NSSBR_LOGERR ("type is not TCP]fd=%d", sk->fd);
  sbr_set_sk_errno (sk, EOPNOTSUPP);
  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_udp_accept
*   Description  : unsupport
*   Input        : sbr_socket_t * sk
*                  sbr_socket_t * new_sk
*                  struct sockaddr * addr
*                  socklen_t * addrlen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_udp_accept (sbr_socket_t * sk, sbr_socket_t * new_sk,
                struct sockaddr *addr, socklen_t * addrlen)
{
  NSSBR_LOGERR ("type is not TCP]fd=%d", sk->fd);
  sbr_set_sk_errno (sk, EOPNOTSUPP);
  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_udp_accept4
*   Description  : unsupport
*   Input        : sbr_socket_t * sk
*                  sbr_socket_t * new_sk
*                  struct sockaddr * addr
*                  socklen_t * addrlen
*                  int flags
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_udp_accept4 (sbr_socket_t * sk, sbr_socket_t * new_sk,
                 struct sockaddr *addr, socklen_t * addrlen, int flags)
{
  NSSBR_LOGERR ("type is not TCP]fd=%d", sk->fd);
  sbr_set_sk_errno (sk, EOPNOTSUPP);
  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_udp_connect
*   Description  : udp connect
*   Input        : sbr_socket_t * sk
*                  const struct sockaddr * name
*                  socklen_t namelen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_udp_connect (sbr_socket_t * sk, const struct sockaddr *name,
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

  return sbr_handle_connect (sk, &remote_addr, ntohs (remote_port),
                             &local_addr);
}

/*****************************************************************************
*   Prototype    : sbr_udp_shutdown
*   Description  : udp shutdown
*   Input        : sbr_socket_t * sk
*                  int how
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_shutdown (sbr_socket_t * sk, int how)
{
  ss_set_shut_status (sbr_get_conn (sk), how);
  NSSBR_LOGERR ("type is not TCP]fd=%d", sk->fd);
  sbr_set_sk_errno (sk, EOPNOTSUPP);
  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_udp_getsockname
*   Description  : get sock name
*   Input        : sbr_socket_t * sk
*                  struct sockaddr * name
*                  socklen_t * namelen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_udp_getsockname (sbr_socket_t * sk, struct sockaddr *name,
                     socklen_t * namelen)
{
  return sbr_handle_get_name (sk, name, namelen, SBR_GET_SOCK_NAME);
}

/*****************************************************************************
*   Prototype    : sbr_udp_getpeername
*   Description  : get peer name
*   Input        : sbr_socket_t * sk
*                  struct sockaddr * name
*                  socklen_t * namelen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_udp_getpeername (sbr_socket_t * sk, struct sockaddr *name,
                     socklen_t * namelen)
{
  return sbr_handle_get_name (sk, name, namelen, SBR_GET_PEER_NAME);
}

/*****************************************************************************
*   Prototype    : sbr_udp_getsockopt
*   Description  : get sockopt
*   Input        : sbr_socket_t * sk
*                  int level
*                  int optname
*                  void * optval
*                  socklen_t * optlen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_udp_getsockopt (sbr_socket_t * sk, int level, int optname, void *optval,
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

    case NSTACK_SOCKOPT:
      if ((optname == NSTACK_SEM_SLEEP) || (*optlen < sizeof (u32_t)))
        {
          *(u32_t *) optval =
            sbr_get_fd_share (sk)->block_polling_time / 1000;
          NSSOC_LOGINF
            ("udp get recv sleep time success]usleep time=%d,fd=%d",
             sbr_get_fd_share (sk)->block_polling_time, sk->fd);
          return ERR_OK;
        }
      else
        {
          NSSOC_LOGINF ("get recv sleep time failed]fd=%d", sk->fd);
          sbr_set_sk_io_errno (sk, EINVAL);
          return -1;
        }

    default:
      err = ENOPROTOOPT;
      break;
    }

  if (err != 0)
    {
      NSSBR_LOGERR ("fail]fd=%d,level=%d,optname=%d,err=%d", sk->fd, level,
                    optname, err);
      /* for option not support ,getsockopt() should return fail */
      sbr_set_sk_errno (sk, err);
      return -1;
    }

  return sbr_handle_getsockopt (sk, level, optname, optval, optlen);
}

/*****************************************************************************
*   Prototype    : sbr_udp_setsockopt
*   Description  : set sockopt
*   Input        : sbr_socket_t * sk
*                  int level
*                  int optname
*                  const void * optval
*                  socklen_t optlen
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_setsockopt (sbr_socket_t * sk, int level, int optname,
                    const void *optval, socklen_t optlen)
{
  NSSBR_LOGDBG ("udp setsockopt]fd=%d,level=%d,optname=%d", sk->fd, level,
                optname);
  int err = 0;
  switch (level)
    {
    case SOL_SOCKET:
      err =
        sbr_setsockopt_sol_socket (sk, optname, optval, optlen,
                                   SPL_NETCONN_UDP);
      break;
    case IPPROTO_IP:
      err =
        sbr_setsockopt_ipproto_ip (optname, optval, optlen, SPL_NETCONN_UDP);
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
              ("udp set recv sleep time success]usleep time=%d,fd=%d",
               sbr_get_fd_share (sk)->block_polling_time, sk->fd);
            return ERR_OK;
          }
        else
          {
            NSSOC_LOGINF ("set recv sleep time failed]fd=%d", sk->fd);
            sbr_set_sk_io_errno (sk, EINVAL);
            return -1;
          }
      }

    default:
      err = ENOPROTOOPT;
      break;
    }

  if (err != 0)
    {
      NSSBR_LOGERR ("fail]fd=%d,level=%d,optname=%d,err=%d", sk->fd, level,
                    optname, err);

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

  return sbr_handle_setsockopt (sk, level, optname, optval, optlen);
}

static inline int
sbr_udp_get_from_addr (sbr_socket_t * sk, struct sockaddr *from,
                       socklen_t * fromlen, struct spl_netbuf *buf)
{
  int ret;
  u16 port = netbuf_fromport (buf);
  spl_ip_addr_t *addr = netbuf_fromaddr (buf);

  ret = (from
         && fromlen) ? sbr_get_sockaddr_and_len (port, addr, from,
                                                 fromlen) : 0;
  if (0 != ret)
    {
      sbr_set_sk_io_errno (sk, EINVAL);
      NSSBR_LOGERR ("sbr_udp_get_from_addr]fd=%d", sk->fd);
      return -1;
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_udp_recv_from_ring
*   Description  : recv buf from ring
*   Input        : sbr_socket_t * sk
*                  struct spl_netbuf ** buf
*                  i32 timeout
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_udp_recv_from_ring (sbr_socket_t * sk, struct spl_netbuf **buf,
                        i32 timeout)
{
  void *p = NULL;
  spl_netconn_t *conn = sbr_get_conn (sk);

  if (sbr_dequeue_buf (sk, &p, timeout) != 0)
    {
      return -1;
    }

  *buf = (struct spl_netbuf *) ((char *) p + sizeof (struct spl_pbuf));
  ss_sub_recv_event (conn);
  return 0;
}

/*****************************************************************************
*   Prototype    : _sbr_udp_recvfrom
*   Description  : base recvfrom,without lock
*   Input        : sbr_socket_t * sk
*                  void * mem
*                  size_t len
*                  int flags
*                  struct sockaddr * from
*                  socklen_t * fromlen
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
_sbr_udp_recvfrom (sbr_socket_t * sk, void *mem, size_t len, int flags,
                   struct sockaddr *from, socklen_t * fromlen)
{
  struct spl_netbuf *buf = NULL;
  struct spl_pbuf *p;
  u32 buflen;
  u32 copylen;
  u32 off = 0;

  if (sbr_get_fd_share (sk)->lastdata)
    {
      buf = ADDR_SHTOL (sbr_get_fd_share (sk)->lastdata);
    }
  else
    {
      if ((flags & MSG_DONTWAIT) || ss_is_nonblock_flag (sbr_get_conn (sk)))
        {
          /*
           * return with last err when
           * some fatal err occurs, for example, spl just recovered from a fault.
           */
          int err = ss_get_last_errno (sbr_get_conn (sk));
          if (SPL_ERR_IS_FATAL (err))
            {
              NSSBR_LOGDBG ("connection fatal error]sk->fd=%d,errno=%d",
                            sk->fd, err);
              sbr_set_sk_io_errno (sk, sbr_spl_err_to_errno (err));
              return -1;
            }

          if (ss_get_recv_event (sbr_get_conn (sk)) <= 0)
            {
              NSSBR_LOGDBG ("no recv event]fd=%d", sk->fd);
              sbr_set_sk_io_errno (sk, EWOULDBLOCK);
              return -1;
            }
        }

      if (sbr_udp_recv_from_ring
          (sk, &buf, sbr_get_fd_share (sk)->recv_timeout) != 0)
        {
          return -1;
        }

      sbr_get_fd_share (sk)->lastdata = (void *) ADDR_LTOSH (buf);
    }

  p = (struct spl_pbuf *) ADDR_SHTOL (buf->p);
  buflen = p->tot_len;

  if (mem)
    {
      copylen = len > buflen ? buflen : len;

      if ((copylen > 0) && 0 == spl_pbuf_copy_partial (p, mem, copylen, 0))
        {
          NSSBR_LOGERR ("copy failed]fd=%d", sk->fd);
          sbr_set_sk_io_errno (sk, EFAULT);
          return -1;
        }

      off += copylen;
    }

  if (sbr_udp_get_from_addr (sk, from, fromlen, buf) != 0)
    {
      return -1;
    }

  if (!(flags & MSG_PEEK))
    {
      sbr_get_fd_share (sk)->lastdata = NULL;
      sbr_com_free_recv_buf (sk, (struct spl_pbuf *) ADDR_SHTOL (buf->p));
    }

  return off;
}

/*****************************************************************************
*   Prototype    : sbr_udp_recvfrom
*   Description  : recv from
*   Input        : sbr_socket_t * sk
*                  void * mem
*                  size_t len
*                  int flags
*                  struct sockaddr * from
*                  socklen_t * fromlen
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_recvfrom (sbr_socket_t * sk, void *mem, size_t len, int flags,
                  struct sockaddr *from, socklen_t * fromlen)
{
  sbr_com_lock_recv (sk);
  int ret = _sbr_udp_recvfrom (sk, mem, len, flags, from, fromlen);
  sbr_com_unlock_recv (sk);
  return ret;
}

/*****************************************************************************
*   Prototype    : sbr_udp_recvdata
*   Description  : recv data
*   Input        : sbr_socket_t * sk
*                  const struct iovec* iov
*                  int iovcnt
*                  struct spl_netbuf *buf
*   Output       : None
*   Return Value : static inline
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_udp_recvdata (sbr_socket_t * sk, const struct iovec *iov, int iovcnt,
                  int flags, struct sockaddr *from, socklen_t * fromlen)
{
  sbr_com_lock_recv (sk);
  if (-1 == _sbr_udp_recvfrom (sk, NULL, 0, MSG_PEEK, from, fromlen))
    {
      sbr_com_unlock_recv (sk);
      return -1;
    }

  struct spl_netbuf *buf = ADDR_SHTOL (sbr_get_fd_share (sk)->lastdata);
  struct spl_pbuf *p = (struct spl_pbuf *) ADDR_SHTOL (buf->p);
  u32 buflen = p->tot_len;
  u32 copylen = 0;
  u32 offset = 0;

  int i;

  for (i = 0; i < iovcnt; ++i)
    {
      if (!iov[i].iov_base || (0 == iov[i].iov_len))
        {
          continue;
        }

      copylen = buflen > iov[i].iov_len ? iov[i].iov_len : buflen;
      if ((copylen > 0)
          && 0 == spl_pbuf_copy_partial (p, iov[i].iov_base, copylen, offset))
        {
          NSSBR_LOGERR ("copy failed]fd=%d", sk->fd);
          goto done;
        }

      offset += copylen;
      buflen -= copylen;

      if (0 == buflen)
        {
          goto done;
        }
    }

done:
  if (!(flags & MSG_PEEK))
    {
      sbr_get_fd_share (sk)->lastdata = NULL;
      sbr_com_free_recv_buf (sk, (struct spl_pbuf *) ADDR_SHTOL (buf->p));
    }

  sbr_com_unlock_recv (sk);
  return offset;
}

/*****************************************************************************
*   Prototype    : sbr_udp_readv
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
sbr_udp_readv (sbr_socket_t * sk, const struct iovec *iov, int iovcnt)
{
  return sbr_udp_recvdata (sk, iov, iovcnt, 0, NULL, NULL);
}

/*****************************************************************************
*   Prototype    : sbr_udp_recvmsg
*   Description  : recv msg
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
sbr_udp_recvmsg (sbr_socket_t * sk, struct msghdr *msg, int flags)
{
  return sbr_udp_recvdata (sk, msg->msg_iov, msg->msg_iovlen, flags,
                           (struct sockaddr *) msg->msg_name,
                           &msg->msg_namelen);
}

/*****************************************************************************
*   Prototype    : sbr_copy_iov
*   Description  : copy iov
*   Input        : sbr_socket_t * sk
*                  const struct iovec * iov
*                  int iovcnt
*                  struct spl_pbuf* buf
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_copy_iov (sbr_socket_t * sk, const struct iovec *iov, int iovcnt,
              struct spl_pbuf *buf)
{
  u32 buf_left = buf->len;
  i8 *buf_data = (i8 *) ADDR_SHTOL (buf->payload);
  u32 iov_left;
  i8 *iov_data;
  u32 copy_len;

  int i;

  for (i = 0; i < iovcnt; ++i)
    {
      if (!iov[i].iov_base || (0 == iov[i].iov_len))
        {
          continue;
        }

      iov_left = (u32) iov[i].iov_len;  /* to u32 is ok,len is checked in sbr_udp_senddata */
      iov_data = (i8 *) iov[i].iov_base;
      while (iov_left)
        {
          copy_len = buf_left > iov_left ? iov_left : buf_left;

          if (NULL == common_memcpy (buf_data, iov_data, copy_len))
            {
              NSSBR_LOGERR ("common_memcpy error]fd=%d", sk->fd);
              sbr_set_sk_errno (sk, EFAULT);
              return -1;
            }

          buf_data += copy_len;
          buf_left -= copy_len;
          iov_data += copy_len;
          iov_left -= copy_len;
          if (0 == buf_left)
            {
              buf = (struct spl_pbuf *) ADDR_SHTOL (buf->next);
              if (buf)
                {
                  buf_left = buf->len;
                  buf_data = (i8 *) ADDR_SHTOL (buf->payload);
                }
              else
                {
                  return 0;
                }
            }
        }
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_udp_senddata
*   Description  : send data
*   Input        : sbr_socket_t * sk
*                  const struct iovec * iov
*                  int iovcnt
*                  int flags
*                  const struct sockaddr * to
*                  socklen_t tolen
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_udp_senddata (sbr_socket_t * sk, const struct iovec *iov, int iovcnt,
                  int flags, const struct sockaddr *to, socklen_t tolen)
{
  size_t size = 0;
  int iov_idx;

  for (iov_idx = 0; iov_idx < iovcnt; ++iov_idx)
    {
      if ((SPL_MAX_UDP_MSG_LEN - size) < iov[iov_idx].iov_len)
        {
          NSSBR_LOGERR
            ("size > SPL_MAX_UDP_MSG_LEN]fd=%d,SPL_MAX_UDP_MSG_LEN=%u",
             sk->fd, SPL_MAX_UDP_MSG_LEN);
          sbr_set_sk_io_errno (sk, EMSGSIZE);
          return -1;
        }

      size += iov[iov_idx].iov_len;
    }

  if (to == NULL)
    {
      /* if not bind , then dest address should not be NULL */
      if (IPADDR_ANY == ss_get_remote_ip (sbr_get_conn (sk))->addr)
        {
          NSSBR_LOGERR ("dest address is null]fd=%d", sk->fd);
          sbr_set_sk_io_errno (sk, EDESTADDRREQ);
          return -1;
        }
    }
  else if (to->sa_family != AF_INET)
    {
      NSSBR_LOGERR ("invalid address family]fd=%d,family=%d", sk->fd,
                    to->sa_family);
      sbr_set_sk_io_errno (sk, EAFNOSUPPORT);
      return -1;
    }
  else if (tolen != sizeof (struct sockaddr_in))
    {
      NSSBR_LOGERR ("invalid address len]fd=%d,tolen=%u", sk->fd, tolen);
      sbr_set_sk_io_errno (sk, EINVAL);
      return -1;
    }

  struct spl_netbuf buf;
  const struct sockaddr_in *to_in = (const struct sockaddr_in *) to;
  buf.p = NULL;
  if (to_in)
    {
      NSSBR_LOGDBG ("fd=%d,addr=%s,port=%d,conn=%p,private_data=%p", sk->fd,
                    spl_inet_ntoa (to_in->sin_addr), ntohs (to_in->sin_port),
                    sbr_get_conn (sk),
                    ss_get_private_data (sbr_get_conn (sk)));
      inet_addr_to_ipaddr (&buf.addr, &to_in->sin_addr);
      netbuf_fromport (&buf) = ntohs (to_in->sin_port);
    }
  else
    {
      spl_ip_addr_set_any (&buf.addr);
      netbuf_fromport (&buf) = 0;
    }

  spl_ip_addr_t local_ip;
  if (IPADDR_ANY == ss_get_local_ip (sbr_get_conn (sk))->addr)
    {
      if (sbr_get_src_ip (buf.addr.addr, &local_ip.addr) != 0)
        {
          sbr_set_sk_io_errno (sk, EHOSTUNREACH);
          NSSBR_LOGERR ("get src ip failed]fd=%d", sk->fd);
          return -1;
        }
    }

  int err = ss_get_last_errno (sbr_get_conn (sk));
  if (SPL_ERR_IS_FATAL (err))
    {
      NS_LOG_CTRL (LOG_CTRL_SEND, LOGSBR, "NSSBR", NSLOG_ERR,
                   "connection fatal error!err=%d", err);
      sbr_set_sk_errno (sk, sbr_spl_err_to_errno (err));
      return -1;
    }

  u16 remain_len = size;        //+ head_room_len;
  struct spl_pbuf *p = NULL;
  PRIMARY_ADDR struct spl_pbuf *header = NULL;
  struct spl_pbuf **tail = &header;
  u16 head_len = L2_L3_ROOM_LEN + L4_ROOM_LEN;
  u16 copy_len;
  u16 alloc_len;

  do
    {
      copy_len =
        remain_len >
        (SPL_PBUF_UDP_LEN - head_len) ? (SPL_PBUF_UDP_LEN -
                                         head_len) : remain_len;
      alloc_len = head_len + copy_len;
      p = sbr_malloc_tx_pbuf (alloc_len, head_len);
      if (unlikely (!p))
        {
          NSSBR_LOGDBG ("malloc pbuf failed]fd=%d", sk->fd);
          sbr_set_sk_io_errno (sk, ENOMEM);
          sbr_handle_free_send_buf (sk,
                                    (struct spl_pbuf *) ADDR_SHTOL (header));
          // ss_set_send_event(sbr_get_conn(sk), 0);
          return -1;
        }

      struct spl_pbuf *tmp = (struct spl_pbuf *) ADDR_SHTOL (header);
      while (tmp)
        {
          tmp->tot_len += p->len;
          tmp = (struct spl_pbuf *) ADDR_SHTOL (tmp->next);
        }

      *tail = (struct spl_pbuf *) ADDR_LTOSH (p);       /* header will changed to share */
      tail = &p->next;

      remain_len -= copy_len;
      head_len = L2_L3_ROOM_LEN;
    }
  while (remain_len);

  /*udp support len=0 */
  if (size != 0)
    {
      if (sbr_copy_iov
          (sk, iov, iovcnt, (struct spl_pbuf *) ADDR_SHTOL (header)) != 0)
        {
          sbr_handle_free_send_buf (sk,
                                    (struct spl_pbuf *) ADDR_SHTOL (header));
          return -1;
        }
    }

  buf.p = header;
  err = sbr_handle_udp_send (sk, &buf, &local_ip);
  if (0 == err)
    {
      epoll_triggle_event_from_api (sk, EPOLL_API_OP_SEND);
      //ss_set_send_event(sbr_get_conn(sk), 1);
      return size;
    }
  else
    {
      sbr_handle_free_send_buf (sk, (struct spl_pbuf *) ADDR_SHTOL (buf.p));
      return -1;
    }

}

/*****************************************************************************
*   Prototype    : sbr_udp_sendto
*   Description  : sendto
*   Input        : sbr_socket_t * sk
*                  const void * data
*                  size_t size
*                  int flags
*                  const struct sockaddr * to
*                  socklen_t tolen
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_sendto (sbr_socket_t * sk, const void *data, size_t size, int flags,
                const struct sockaddr *to, socklen_t tolen)
{
  struct iovec iov;

  iov.iov_base = (void *) data;
  iov.iov_len = size;
  return sbr_udp_senddata (sk, &iov, 1, flags, to, tolen);
}

/*****************************************************************************
*   Prototype    : sbr_udp_send
*   Description  : send
*   Input        : sbr_socket_t * sk
*                  const void * data
*                  size_t size
*                  int flags
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_send (sbr_socket_t * sk, const void *data, size_t size, int flags)
{
  return sk->fdopt->sendto (sk, data, size, flags, NULL, 0);
}

/*****************************************************************************
*   Prototype    : sbr_udp_sendmsg
*   Description  : send msg
*   Input        : sbr_socket_t * sk
*                  const struct msghdr * pmsg
*                  int flags
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_sendmsg (sbr_socket_t * sk, const struct msghdr *pmsg, int flags)
{
  return sbr_udp_senddata (sk, pmsg->msg_iov, pmsg->msg_iovlen, flags,
                           (struct sockaddr *) pmsg->msg_name,
                           pmsg->msg_namelen);
}

/*****************************************************************************
*   Prototype    : sbr_udp_writev
*   Description  : writev
*   Input        : sbr_socket_t * sk
*                  const struct iovec * iov
*                  int iovcnt
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_writev (sbr_socket_t * sk, const struct iovec *iov, int iovcnt)
{
  return sbr_udp_senddata (sk, iov, iovcnt, 0, NULL, 0);
}

/*****************************************************************************
*   Prototype    : sbr_udp_fcntl
*   Description  : fcntl
*   Input        : sbr_socket_t * sk
*                  int cmd
*                  long arg
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_fcntl (sbr_socket_t * sk, int cmd, long arg)
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

/*****************************************************************************
*   Prototype    : sbr_udp_ioctl
*   Description  : ioctl
*   Input        : sbr_socket_t * sk
*                  unsigned long cmd
*                  void * arg
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_ioctl (sbr_socket_t * sk, unsigned long cmd, void *arg)
{
  int ret = 0;

  switch (cmd)
    {
    case FIONREAD:
      {
        if (!sbr_com_try_lock_recv (sk))
          {
            return 0;
          }

        struct spl_pbuf *p = NULL;
        struct spl_netbuf *buf = NULL;
        if (!sbr_get_fd_share (sk)->lastdata)
          {
            ret = sbr_udp_recv_from_ring (sk, &buf, -1);
            if (ret != 0)
              {
                sbr_com_unlock_recv (sk);
                return EWOULDBLOCK == errno ? 0 : -1;
              }

            sbr_get_fd_share (sk)->lastdata = (void *) ADDR_LTOSH (buf);
            p = (struct spl_pbuf *) ADDR_SHTOL (buf->p);
          }
        else
          {
            buf =
              (struct spl_netbuf *)
              ADDR_SHTOL (sbr_get_fd_share (sk)->lastdata);
            p = (struct spl_pbuf *) ADDR_SHTOL (buf->p);
          }

        *((u32 *) arg) = p->tot_len;
        sbr_com_unlock_recv (sk);
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

/*****************************************************************************
*   Prototype    : sbr_udp_close
*   Description  : close
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
sbr_udp_close (sbr_socket_t * sk)
{
  if (sbr_get_fd_share (sk)->lastdata)
    {
      struct spl_netbuf *buf = ADDR_SHTOL (sbr_get_fd_share (sk)->lastdata);
      struct spl_pbuf *p = (struct spl_pbuf *) ADDR_SHTOL (buf->p);
      sbr_com_free_recv_buf (sk, p);
    }

  /* if failed,free it in recycle */
  return sbr_handle_close (sk, 0);
}

sbr_fdopt udp_fdopt = {
  .socket = sbr_udp_socket,
  .bind = sbr_udp_bind,
  .listen = sbr_udp_listen,
  .accept = sbr_udp_accept,
  .accept4 = sbr_udp_accept4,
  .connect = sbr_udp_connect,
  .shutdown = sbr_udp_shutdown,
  .getsockname = sbr_udp_getsockname,
  .getpeername = sbr_udp_getpeername,
  .getsockopt = sbr_udp_getsockopt,
  .setsockopt = sbr_udp_setsockopt,
  .recvfrom = sbr_udp_recvfrom,
  .readv = sbr_udp_readv,
  .recvmsg = sbr_udp_recvmsg,
  .send = sbr_udp_send,
  .sendto = sbr_udp_sendto,
  .sendmsg = sbr_udp_sendmsg,
  .writev = sbr_udp_writev,
  .fcntl = sbr_udp_fcntl,
  .ioctl = sbr_udp_ioctl,
  .close = sbr_udp_close,
  .peak = sbr_com_peak,
  .lock_common = sbr_com_lock_common,
  .unlock_common = sbr_com_unlock_common,
  .ep_getevt = stackx_eventpoll_getEvt,
  .ep_ctl = stackx_eventpoll_triggle,
  .set_close_stat = NULL,
};
