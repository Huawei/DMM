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

#include <dlfcn.h>
#include "sbr_protocol_api.h"
#include "sbr_res_mgr.h"
#include "nstack_log.h"
#include "nstack_dmm_api.h"

#define SBR_INTERCEPT(ret, name, args) ret sbr_ ## name args
#define CALL_SBR_INTERCEPT(name, args) sbr_ ## name args
#define GET_SBR_INTERCEPT(name) sbr_ ## name

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : create socket
*   Input        : int
*                  socket
*                  (int domain
*                  int type
*                  int protocol)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, socket, (int domain, int type, int protocol))
{
  NSSBR_LOGDBG ("socket]domain=%d,type=%d,protocol=%d", domain, type,
                protocol);
  sbr_fdopt *fdopt = sbr_get_fdopt (domain, type, protocol);

  if (!fdopt)
    {
      return -1;
    }

  sbr_socket_t *sk = sbr_malloc_sk ();

  if (!sk)
    {
      return -1;
    }

  sk->fdopt = fdopt;

  int ret = sk->fdopt->socket (sk, domain, type, protocol);

  if (ret != 0)
    {
      sbr_free_sk (sk);
      return ret;
    }

  return sk->fd;
}

/*****************************************************************************
*   Prototype    : sbr_check_addr
*   Description  : check addr
*   Input        : int s
*                  struct sockaddr * addr
*                  socklen_t * addrlen
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_check_addr (int s, struct sockaddr *addr, socklen_t * addrlen)
{
  if (addr)
    {
      if (!addrlen)
        {
          NSSBR_LOGERR ("addrlen is null]fd=%d", s);
          sbr_set_errno (EFAULT);
          return -1;
        }

      if (0 > (int) (*addrlen))
        {
          NSSBR_LOGERR ("addrlen is negative]fd=%d,addrlen=%d", s, *addrlen);
          sbr_set_errno (EINVAL);
          return -1;
        }
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : accept4
*   Input        : int
*                  accept4
*                  (int s
*                  struct sockaddr * addr
*                  socklen_t * addrlen
*                  int flags)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, accept4,
               (int s, struct sockaddr * addr, socklen_t * addrlen,
                int flags))
{
  NSSBR_LOGDBG ("accept4]fd=%d,addr=%p,addrlen=%p,flags=%d", s, addr, addrlen,
                flags);
  int ret = sbr_check_addr (s, addr, addrlen);

  if (ret != 0)
    {
      return ret;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  sbr_socket_t *new_sk = sbr_malloc_sk ();
  if (!new_sk)
    {
      return -1;
    }

  new_sk->fdopt = sk->fdopt;

  ret = sk->fdopt->accept4 (sk, new_sk, addr, addrlen, flags);
  if (-1 == ret)
    {
      sbr_free_sk (new_sk);
    }

  return ret;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : accept
*   Input        : int
*                  accept
*                  (int s
*                  struct sockaddr * addr
*                  socklen_t * addrlen)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, accept,
               (int s, struct sockaddr * addr, socklen_t * addrlen))
{
  NSSBR_LOGDBG ("accept]fd=%d,addr=%p,addrlen=%p", s, addr, addrlen);
  int ret = sbr_check_addr (s, addr, addrlen);

  if (ret != 0)
    {
      return ret;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  sbr_socket_t *new_sk = sbr_malloc_sk ();
  if (!new_sk)
    {
      return -1;
    }

  new_sk->fdopt = sk->fdopt;

  ret = sk->fdopt->accept (sk, new_sk, addr, addrlen);
  if (-1 == ret)
    {
      sbr_free_sk (new_sk);
    }

  return ret;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : bind
*   Input        : int
*                  bind
*                  (int s
*                  const struct sockaddr * name
*                  socklen_t namelen)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, bind,
               (int s, const struct sockaddr * name, socklen_t namelen))
{
  NSSBR_LOGDBG ("bind]fd=%d,name=%p,namelen=%d", s, name, namelen);
  if (!name)
    {
      NSSBR_LOGERR ("name is not ok]fd=%d,name=%p", s, name);
      sbr_set_errno (EFAULT);
      return -1;
    }

  if ((name->sa_family) != AF_INET)
    {
      NSSBR_LOGERR ("domain is not AF_INET]fd=%d,name->sa_family=%u", s,
                    name->sa_family);
      sbr_set_errno (EAFNOSUPPORT);
      return -1;
    }

  if (namelen != sizeof (struct sockaddr_in))
    {
      NSSBR_LOGERR ("namelen is invalid]fd=%d,namelen=%d", s, namelen);
      sbr_set_errno (EINVAL);
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->bind (sk, name, namelen);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : listen
*   Input        : int
*                  listen
*                  (int s
*                  int backlog)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, listen, (int s, int backlog))
{
  NSSBR_LOGDBG ("listen]fd=%d,backlog=%d", s, backlog);
  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  /* limit the "backlog" parameter to fit in an u8_t */
  if (backlog < 0)
    {
      backlog = 0;
    }

  if (backlog > 0xff)
    {
      backlog = 0xff;
    }

  return sk->fdopt->listen (sk, backlog);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : connect
*   Input        : int
*                  connect
*                  (int s
*                  const struct sockaddr * name
*                  socklen_t namelen)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, connect,
               (int s, const struct sockaddr * name, socklen_t namelen))
{
  NSSBR_LOGDBG ("connect]fd=%d,name=%p,namelen=%d", s, name, namelen);
  if (!name)
    {
      NSSBR_LOGERR ("name is null]fd=%d", s);
      sbr_set_errno (EFAULT);
      return -1;
    }

  if (!
      (namelen == sizeof (struct sockaddr_in)
       && (name->sa_family == AF_INET)))
    {
      NSSBR_LOGERR ("parameter invalid]fd=%d,domain=%u,namelen=%d", s,
                    name->sa_family, namelen);
      sbr_set_errno (EINVAL);
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      NSSBR_LOGERR ("get socket failed]fd=%d", s);
      return -1;
    }

  return sk->fdopt->connect (sk, name, namelen);
}

/*****************************************************************************
*   Prototype    : sbr_check_sock_name
*   Description  : check name
*   Input        : int s
*                  struct sockaddr * name
*                  socklen_t * namelen
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_check_sock_name (int s, struct sockaddr *name, socklen_t * namelen)
{
  if (!name || !namelen)
    {
      NSSBR_LOGERR ("name or namelen is null]fd=%d", s);
      sbr_set_errno (EINVAL);
      return -1;
    }

  if (*namelen & 0x80000000)
    {
      NSSBR_LOGERR ("namelen is not ok]fd=%d,namelen=%d", s, *namelen);
      sbr_set_errno (EINVAL);
      return -1;
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : getpeername
*   Input        : int
*                  getpeername
*                  (int s
*                  struct sockaddr * name
*                  socklen_t * namelen)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, getpeername,
               (int s, struct sockaddr * name, socklen_t * namelen))
{
  NSSBR_LOGDBG ("getpeername]fd=%d", s);
  int ret = sbr_check_sock_name (s, name, namelen);

  if (ret != 0)
    {
      return ret;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  sk->fdopt->lock_common (sk);
  ret = sk->fdopt->getpeername (sk, name, namelen);
  sk->fdopt->unlock_common (sk);
  return ret;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : getsockname
*   Input        : int
*                  getsockname
*                  (int s
*                  struct sockaddr * name
*                  socklen_t * namelen)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, getsockname,
               (int s, struct sockaddr * name, socklen_t * namelen))
{
  NSSBR_LOGDBG ("getsockname]fd=%d", s);
  int ret = sbr_check_sock_name (s, name, namelen);

  if (ret != 0)
    {
      return ret;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  sk->fdopt->lock_common (sk);
  ret = sk->fdopt->getsockname (sk, name, namelen);
  sk->fdopt->unlock_common (sk);
  return ret;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : setsockopt
*   Input        : int
*                  setsockopt
*                  (int s
*                  int level
*                  int optname
*                  const void * optval
*                  socklen_t optlen)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, setsockopt,
               (int s, int level, int optname, const void *optval,
                socklen_t optlen))
{
  NSSBR_LOGDBG ("setsockopt]fd=%d,level=%d,optname=%d,optval=%p,optlen=%d", s,
                level, optname, optval, optlen);
  if (!optval)
    {
      NSSBR_LOGERR ("optval is null]fd=%d", s);
      sbr_set_errno (EFAULT);
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  sk->fdopt->lock_common (sk);
  int ret = sk->fdopt->setsockopt (sk, level, optname, optval, optlen);
  sk->fdopt->unlock_common (sk);
  return ret;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : getsockopt
*   Input        : int
*                  getsockopt
*                  (int s
*                  int level
*                  int optname
*                  void * optval
*                  socklen_t * optlen)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, getsockopt,
               (int s, int level, int optname, void *optval,
                socklen_t * optlen))
{
  NSSBR_LOGDBG ("getsockopt]fd=%d,level=%d,optname=%d,optval=%p,optlen=%p", s,
                level, optname, optval, optlen);
  if (!optval || !optlen)
    {
      NSSBR_LOGERR ("optval or optlen is NULL]fd=%d", s);
      sbr_set_errno (EFAULT);
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  sk->fdopt->lock_common (sk);
  int ret = sk->fdopt->getsockopt (sk, level, optname, optval, optlen);
  sk->fdopt->unlock_common (sk);
  return ret;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : ioctl
*   Input        : int
*                  ioctl
*                  (int s
*                  unsigned long cmd
*                  ...)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, ioctl, (int s, unsigned long cmd, ...))
{
  NSSBR_LOGDBG ("ioctl]fd=%d,cmd=%lu", s, cmd);
  va_list va;
  va_start (va, cmd);
  void *arg = va_arg (va, void *);
  va_end (va);

  if (!arg)
    {
      NSSBR_LOGERR ("parameter is not ok]fd=%d", s);
      sbr_set_errno (EINVAL);
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  sk->fdopt->lock_common (sk);
  int ret = sk->fdopt->ioctl (sk, cmd, arg);
  sk->fdopt->unlock_common (sk);
  return ret;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : fcntl
*   Input        : int
*                  fcntl
*                  (int s
*                  int cmd
*                  ...)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, fcntl, (int s, int cmd, ...))
{
  NSSBR_LOGDBG ("fcntl]fd=%d,cmd=%d", s, cmd);
  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  va_list va;
  va_start (va, cmd);
  long arg = va_arg (va, long);
  va_end (va);

  sk->fdopt->lock_common (sk);
  int ret = sk->fdopt->fcntl (sk, cmd, arg);
  sk->fdopt->unlock_common (sk);
  return ret;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : recvfrom
*   Input        : int
*                  recvfrom
*                  (int s
*                  void * mem
*                  size_t len
*                  int flags
*                  struct sockaddr * from
*                  socklen_t * fromlen)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, recvfrom,
               (int s, void *mem, size_t len, int flags,
                struct sockaddr * from, socklen_t * fromlen))
{
  NSSBR_LOGDBG ("recvfrom]fd=%d,mem=%p,len=%d,flags=%d,from=%p,fromlen=%p", s,
                mem, len, flags, from, fromlen);

  if (0 == len)
    {
      NSSBR_LOGDBG ("len is zero]fd=%d,len=%u", s, (u32) len);
      return 0;                 //return directly, don't change the last errno.
    }

  if (!mem)
    {
      NSSBR_LOGERR ("mem is NULL]fd=%d", s);
      sbr_set_errno (EFAULT);
      return -1;
    }

  if (fromlen && (*((int *) fromlen) < 0))
    {
      NSSBR_LOGERR ("fromlen is not ok]fd=%d,fromlen=%d", s, *fromlen);
      sbr_set_errno (EINVAL);
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->recvfrom (sk, mem, len, flags, from, fromlen);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : read
*   Input        : int
*                  read
*                  (int s
*                  void * mem
*                  size_t len)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, read, (int s, void *mem, size_t len))
{
  NSSBR_LOGDBG ("read]fd=%d,mem=%p,len=%d", s, mem, len);
  return CALL_SBR_INTERCEPT (recvfrom, (s, mem, len, 0, NULL, NULL));
}

/*****************************************************************************
*   Prototype    : sbr_check_iov
*   Description  : check iov
*   Input        : int s
*                  const struct iovec * iov
*                  int iovcnt
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_check_iov (int s, const struct iovec *iov, int iovcnt)
{
  if ((!iov) || (iovcnt <= 0))
    {
      NSSBR_LOGERR ("iov is NULL or iovcn <=0]fd=%d", s);
      sbr_set_errno (EFAULT);
      return -1;
    }

  int i;
  for (i = 0; i < iovcnt; ++i)
    {
      if (!iov[i].iov_base && (iov[i].iov_len != 0))
        {
          NSSBR_LOGERR ("iov is not ok]fd=%d,iov_base=%p,iov_len=%d", s,
                        iov[i].iov_base, iov[i].iov_len);
          sbr_set_errno (EFAULT);
          return -1;
        }
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : readv
*   Input        : int
*                  readv
*                  (int s
*                  const struct iovec * iov
*                  int iovcnt)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, readv, (int s, const struct iovec * iov, int iovcnt))
{
  NSSBR_LOGDBG ("readv]fd=%d,iov=%p,iovcnt=%d", s, iov, iovcnt);

  if (sbr_check_iov (s, iov, iovcnt) != 0)
    {
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->readv (sk, iov, iovcnt);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : recv
*   Input        : int
*                  recv
*                  (int s
*                  void * mem
*                  size_t len
*                  int flags)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, recv, (int s, void *mem, size_t len, int flags))
{
  NSSBR_LOGDBG ("recv]fd=%d,mem=%p,len=%d,flags=%d", s, mem, len, flags);
  return CALL_SBR_INTERCEPT (recvfrom, (s, mem, len, flags, NULL, NULL));
}

/*****************************************************************************
*   Prototype    : sbr_check_msg
*   Description  : check msg
*   Input        : int s
*                  const struct msghdr * msg
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_check_msg (int s, const struct msghdr *msg)
{
  if (!msg)
    {
      NSSBR_LOGERR ("msg is NULL]fd=%d", s);
      sbr_set_errno (EFAULT);
      return -1;
    }

  if (msg->msg_name && ((int) msg->msg_namelen < 0))
    {
      NSSBR_LOGERR ("msg_namelen is not ok]fd=%d,msg_namelen=%d", s,
                    msg->msg_namelen);
      sbr_set_errno (EINVAL);
      return -1;
    }

  return sbr_check_iov (s, msg->msg_iov, msg->msg_iovlen);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : recvmsg
*   Input        : int
*                  recvmsg
*                  (int s
*                  struct msghdr * msg
*                  int flags)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, recvmsg, (int s, struct msghdr * msg, int flags))
{
  NSSBR_LOGDBG ("recvmsg]fd=%d,msg=%p,flags=%d", s, msg, flags);

  if (sbr_check_msg (s, msg) != 0)
    {
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->recvmsg (sk, msg, flags);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : send
*   Input        : int
*                  send
*                  (int s
*                  const void * data
*                  size_t size
*                  int flags)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, send, (int s, const void *data, size_t size, int flags))
{
  NSSBR_LOGDBG ("send]fd=%d,data=%p,size=%zu,flags=%d", s, data, size, flags);
  if (!data)
    {
      NSSBR_LOGERR ("data is NULL]fd=%d", s);
      sbr_set_errno (EFAULT);
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->send (sk, data, size, flags);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : sendmsg
*   Input        : int
*                  sendmsg
*                  (int s
*                  const struct msghdr * msg
*                  int flags)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, sendmsg, (int s, const struct msghdr * msg, int flags))
{
  NSSBR_LOGDBG ("sendmsg]fd=%d,msg=%p,flags=%d", s, msg, flags);

  if (sbr_check_msg (s, msg) != 0)
    {
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->sendmsg (sk, msg, flags);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : sendto
*   Input        : int
*                  sendto
*                  (int s
*                  const void * data
*                  size_t size
*                  int flags
*                  const struct sockaddr * to
*                  socklen_t tolen)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, sendto,
               (int s, const void *data, size_t size, int flags,
                const struct sockaddr * to, socklen_t tolen))
{
  NSSBR_LOGDBG ("sendto]fd=%d,data=%p,size=%zu,flags=%d,to=%p,tolen=%d", s,
                data, size, flags, to, tolen);
  if ((data == NULL) || (flags < 0))
    {
      NSSBR_LOGERR ("parameter is not ok]fd=%d,data=%p,size=%zu,flags=%d", s,
                    data, size, flags);
      sbr_set_errno (EINVAL);
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->sendto (sk, data, size, flags, to, tolen);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : write
*   Input        : int
*                  write
*                  (int s
*                  const void * data
*                  size_t size)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, write, (int s, const void *data, size_t size))
{
  NSSBR_LOGDBG ("write]fd=%d,data=%p,size=%zu", s, data, size);
  return CALL_SBR_INTERCEPT (send, (s, data, size, 0));
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : writev
*   Input        : int
*                  writev
*                  (int s
*                  const struct iovec * iov
*                  int iovcnt)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, writev, (int s, const struct iovec * iov, int iovcnt))
{
  NSSBR_LOGDBG ("writev]fd=%d,iov=%p,iovcnt=%d", s, iov, iovcnt);

  if (sbr_check_iov (s, iov, iovcnt) != 0)
    {
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->writev (sk, iov, iovcnt);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : shutdown
*   Input        : int
*                  shutdown
*                  (int s
*                  int how)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, shutdown, (int s, int how))
{
  NSSBR_LOGDBG ("shutdown]fd=%d,how=%d", s, how);
  if ((how != SHUT_RD) && (how != SHUT_WR) && (how != SHUT_RDWR))
    {
      sbr_set_errno (EINVAL);
      return -1;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  sk->fdopt->lock_common (sk);
  int ret = sk->fdopt->shutdown (sk, how);
  sk->fdopt->unlock_common (sk);
  return ret;
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : close
*   Input        : int
*                  close
*                  (int s)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (int, close, (int s))
{
  NSSBR_LOGDBG ("close]fd=%d", s);
  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  int ret = sk->fdopt->close (sk);
  sbr_free_sk (sk);
  return ret;
}

SBR_INTERCEPT (int, select,
               (int nfds, fd_set * readfd, fd_set * writefd,
                fd_set * exceptfd, struct timeval * timeout))
{
  return lwip_try_select (nfds, readfd, writefd, exceptfd, timeout);
}

SBR_INTERCEPT (unsigned int, ep_getevt,
               (int epfd, int profd, unsigned int events))
{
  NSSBR_LOGDBG ("epfd= %d, proFD=%d,epi=0x%x", epfd, profd, events);
  sbr_socket_t *sk = sbr_lookup_sk (profd);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->ep_getevt (sk, events);
}

SBR_INTERCEPT (unsigned int, ep_ctl,
               (int epfd, int profd, int triggle_ops,
                struct epoll_event * event, void *pdata))
{
  NSSBR_LOGDBG ("epfd = %d, proFD=%d,triggle_ops=%d,pdata=%p", epfd, profd,
                triggle_ops, pdata);
  sbr_socket_t *sk = sbr_lookup_sk (profd);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->ep_ctl (sk, triggle_ops, event, pdata);
}

SBR_INTERCEPT (int, peak, (int s))
{
  NSSBR_LOGDBG ("]fd=%d", s);
  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return -1;
    }

  return sk->fdopt->peak (sk);
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : set_app_info
*   Input        : int
*                  set_app_info
*                  (int s
*                  void* app_info)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (void, set_app_info, (int s, void *app_info))
{
  NSSBR_LOGDBG ("set_app_info]fd=%d", s);

  if (!app_info)
    {
      NSSBR_LOGERR ("invalid param, app_info is NULL]");
      return;
    }

  sbr_socket_t *sk = sbr_lookup_sk (s);
  if (!sk)
    {
      return;
    }

  sk->fdopt->set_app_info (sk, app_info);
}

/* app send its version info to nStackMain */
/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : sbr_app_touch
*   Input        : int
*                  sbr_app_touch
*                  ()
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (void, app_touch, (void))
{
  NSSBR_LOGDBG ("sbr_app_touch() is called]");

  sbr_app_touch_in ();
}

/*****************************************************************************
*   Prototype    : SBR_INTERCEPT
*   Description  : set_close_status
*   Input        : void
*                  set_close_stat
*                  (int s
*                  int flag)
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
SBR_INTERCEPT (void, set_close_stat, (int s, int flag))
{
  NSSBR_LOGDBG ("]fd=%d", s);
  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return;
    }
  if (sk->fdopt->set_close_stat)
    {
      sk->fdopt->set_close_stat (sk, flag);
    }

}

SBR_INTERCEPT (int, fd_alloc, ())
{
  return sbr_socket (AF_INET, SOCK_STREAM, 0);
}

SBR_INTERCEPT (int, fork_init_child, (pid_t p, pid_t c))
{
  NSSBR_LOGDBG ("fork_init_child() is called]");
  return sbr_fork_protocol ();
}

SBR_INTERCEPT (void, fork_parent_fd, (int s, pid_t p))
{
  NSSBR_LOGDBG ("fork_parent_fd() is called]");
  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return;
    }

  sk->fdopt->fork_parent (sk, p);
}

SBR_INTERCEPT (void, fork_child_fd, (int s, pid_t p, pid_t c))
{
  NSSBR_LOGDBG ("fork_child_fd() is called]");
  sbr_socket_t *sk = sbr_lookup_sk (s);

  if (!sk)
    {
      return;
    }

  sk->fdopt->fork_child (sk, p, c);

}

SBR_INTERCEPT (void, fork_free_fd, (int s, pid_t p, pid_t c))
{
  NSSBR_LOGDBG ("fork_free_fd() is called]");
  sbr_free_fd (s);
}

/*****************************************************************************
*   Prototype    : nstack_stack_register
*   Description  : reg api to nsocket
*   Input        : nstack_socket_ops* ops
*                  nstack_event_ops *val
*                  nstack_proc_ops *deal
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
nstack_stack_register (nstack_proc_cb * ops, nstack_event_cb * val)
{
  if (!ops || !val || !val->handle)
    {
      return -1;
    }

#undef NSTACK_MK_DECL
#define NSTACK_MK_DECL(ret, fn, args) \
    (ops->socket_ops).pf ## fn = (typeof(((nstack_socket_ops*)0)->pf ## fn))dlsym(val->handle, "sbr_" # fn);
#include "declare_syscalls.h"

  (ops->extern_ops).module_init = sbr_init_res;
  (ops->extern_ops).ep_getevt = GET_SBR_INTERCEPT (ep_getevt);
  (ops->extern_ops).ep_ctl = GET_SBR_INTERCEPT (ep_ctl);
  (ops->extern_ops).peak = GET_SBR_INTERCEPT (peak);
  (ops->extern_ops).stack_alloc_fd = GET_SBR_INTERCEPT (fd_alloc);      /*alloc a fd id for epoll */
  (ops->extern_ops).fork_init_child = GET_SBR_INTERCEPT (fork_init_child);
  (ops->extern_ops).fork_parent_fd = GET_SBR_INTERCEPT (fork_parent_fd);
  (ops->extern_ops).fork_child_fd = GET_SBR_INTERCEPT (fork_child_fd);
  (ops->extern_ops).fork_free_fd = GET_SBR_INTERCEPT (fork_free_fd);
  return 0;
}
