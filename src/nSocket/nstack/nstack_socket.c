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

#include <time.h>
#include <stdarg.h>

#ifndef __USE_GNU
#define __USE_GNU
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE             /* define RTLD_NEXT */
#endif

#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include "nstack.h"
#include "nstack_socket.h"
#include "nstack_fd_mng.h"
#include "nstack_dmm_api.h"
#include "nstack_sockops.h"
#include "nstack_module.h"
#include "common_mem_spinlock.h"
#include "nstack_securec.h"
#include "nsfw_init.h"
#include "nsfw_recycle_api.h"
#include "nsfw_base_linux_api.h"
#include "nstack_rd_data.h"
#include "nstack_rd.h"
#include "select_adapt.h"
#include "nstack_select.h"
#include "nstack_share_res.h"
#include "nstack_trace.h"
#ifndef F_SETFL
#define    F_SETFL        4
#endif

#define NSTACK_LOOP_IP  0x100007f
#define NSTACK_ANY_IP   0x0

#ifndef REPLAYS_POSIX_API
#define REPLAYS_POSIX_API 1
#endif

#if REPLAYS_POSIX_API
#define strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));
#undef NSTACK_MK_DECL
#define NSTACK_MK_DECL(ret, fn, args)        strong_alias(nstack_##fn, fn)
#include <declare_syscalls.h>
#endif

#define NSTACK_FUN_CHECK_RET(fdops, mod, fun) \
    if (!(fdops) || !(fdops)->pf##fun) \
    { \
       nstack_set_errno(ENOSYS); \
       NSSOC_LOGERR("nstack module:%d ops:%p, ops->pf%s:%p error [return]", mod, fdops, #fun, (fdops)->pf##fun); \
       return -1; \
    }

#define NSTACK_DOMAIN_CHEKRET(domainVal, fn, para) { \
    if ((domainVal != AF_INET)  \
        && (domainVal != PF_INET))  \
    { \
        int _ret_ = 0; \
        if (nstack_fix_fd_check() && nstack_fix_fd_check()(-1, STACK_FD_FUNCALL_CHECK)) \
        { \
            NSTACK_CAL_FUN(nstack_fix_mid_ops(), fn, para, _ret_); \
        }  \
        return _ret_; \
    }  \
}

#define NSTACK_FD_LINUX_CHECK(fdVal, fn, fdInf, para) { \
    if (!(fdInf = nstack_getValidInf(fdVal))) \
    { \
        if (NSTACK_THREAD_LOADING()) \
        { \
            return nsfw_base_##fn para;\
        }  \
        if (0 == nstack_stack_module_load()) \
        {   \
            if (nstack_fix_fd_check() && nstack_fix_fd_check()(fdVal, STACK_FD_FUNCALL_CHECK)) \
            {  \
                int _ret_ = 0; \
                NSTACK_CAL_FUN(nstack_fix_mid_ops(), fn, para, _ret_); \
                return _ret_; \
            }  \
        }  \
        nstack_set_errno(ENOSYS); \
        return -1; \
    } \
}

#define NSTACK_SELECT_LINUX_CHECK()     (get_select_module()->inited)

/* Supports multi-threaded and multi-process */
NSTACK_STATIC inline void
set_fd_status (int fd, FD_STATUS status)
{
  nstack_fd_local_lock_info_t *local_lock = get_fd_local_lock_info (fd);
  if (local_lock)
    {
      if (FD_OPEN == status)
        {
          atomic_inc (&local_lock->fd_ref);
        }
      local_lock->fd_status = status;
    }
}

void
set_fd_status_lock_fork (int fd, FD_STATUS status)
{
  common_mem_rwlock_read_lock (get_fork_lock ());
  set_fd_status (fd, status);
  common_mem_rwlock_read_unlock (get_fork_lock ());
}

/*****************************************************************
Parameters    :  domain
                    type
                    protocol
Return        :
Description   :  create a nstack fd for application
*****************************************************************/
int
nstack_socket (int domain, int itype, int protocol)
{
  int ret = -1;                 //tmp ret of a Single proctol mode.
  int rdret = -1;               //the final Return judge vale.
  int modInx;
  nstack_socket_ops *ops;
  int ret_fd = -1;
  int protoFD[NSTACK_MAX_MODULE_NUM];
  nstack_rd_key rdkey = { 0 };
  int selectmod = -1;

  nstack_set_tracing_contex (0, 0, -1);

  /*check whether module init finish or not */
  NSTACK_INIT_CHECK_RET (socket);

  NSSOC_LOGINF ("(domain=%d, type=%d, protocol=%d) [Caller]", domain, itype,
                protocol);

  /*if domain don't equal AF_INET , just call linux */
  NSTACK_DOMAIN_CHEKRET (domain, socket, (domain, itype, protocol));

  nstack_each_modInx (modInx)
  {
    protoFD[modInx] = -1;
  }

  /*create the fix socket first, and check it wether ok */
  NSTACK_CAL_FUN (nstack_fix_mid_ops (), socket, (domain, itype, protocol),
                  ret_fd);
  if (!nstack_is_nstack_sk (ret_fd))
    {
      if (ret_fd >= 0)
        {
          NSTACK_CAL_FUN (nstack_fix_mid_ops (), close, (ret_fd), ret);
          nstack_set_errno (EMFILE);
        }
      NSSOC_LOGERR
        ("[nstack_linux]domain=%d,type=%d protocol=%d linux fd=%d is too big and return fail [return]",
         domain, itype, protocol, ret_fd);
      return ns_fail;
    }

  protoFD[nstack_get_fix_mid ()] = ret_fd;

  nstack_fd_local_lock_info_t *lock_info = get_fd_local_lock_info (ret_fd);
  LOCK_FOR_EP (lock_info);

  /*check wether select stack create success, if no return fail */
  rdkey.type = RD_DATA_TYPE_PROTO;
  rdkey.proto_type = itype;
  rdret = nstack_rd_get_stackid (&rdkey, &selectmod);
  if ((0 != rdret) || (selectmod < 0) || (selectmod >= nstack_get_modNum ()))
    {
      NSSOC_LOGERR ("protocol:%d select stack fail", protocol);
      selectmod = -1;
    }
  else
    {
      NSSOC_LOGINF ("Create socket of]select modName=%s",
                    nstack_get_module_name_by_idx (selectmod));
    }

  /*create socket by calling select module or all module */
  nstack_each_modOps (modInx, ops)
  {
    if (modInx == nstack_get_fix_mid ())
      {
        continue;
      }
    /*if no module selected then create all or just create selected stack */
    if ((selectmod == -1) || (modInx == selectmod))
      {
        NSTACK_CAL_FUN (ops, socket, (domain, itype, protocol), ret);
        protoFD[modInx] = ret;
        NSSOC_LOGINF ("Create socket of]modName=%s:%d",
                      nstack_get_module_name_by_idx (modInx),
                      protoFD[modInx]);
        if ((modInx == selectmod) && (-1 == ret))
          {
            goto SOCKET_ERR;
          }
      }
  }

  /* alloc nstack fd info */
  nstack_fd_Inf *fdInf = nstack_lk_fd_alloc_with_kernel (ret_fd);
  if (NULL == fdInf)
    {
      /*if alloc failed */
      nstack_set_errno (EMFILE);
      NSSOC_LOGERR ("have no available nstack_fd_Inf [return]");
      goto SOCKET_ERR;
    }

  /*save the fd into fdinfo */
  fdInf->type = itype;
  nstack_each_modInx (modInx)
  {
    if (-1 != protoFD[modInx])
      {
        nstack_set_ret (fdInf, modInx, 0);
        nstack_set_protoFd (fdInf, modInx, protoFD[modInx]);
        nstack_set_app_info (fdInf, modInx);
      }
  }
  set_fd_status_lock_fork (ret_fd, FD_OPEN);

  /*if some stack was choose, just set the chose stack */
  if (-1 != selectmod)
    {
      fdInf->ops = nstack_module_ops (selectmod);
      nstack_set_router_protocol (fdInf, selectmod);
      nstack_set_routed_fd (fdInf, protoFD[selectmod]);
    }
  NSSOC_LOGINF ("create fix stack[%s] fd=%d,fdInf->fd=%d,ret=%d [return]",
                nstack_fix_stack_name (), ret_fd, fdInf->fd, ret_fd);
  UNLOCK_FOR_EP (lock_info);
  return ret_fd;

SOCKET_ERR:
  nstack_each_modOps (modInx, ops)
  {
    if (-1 != protoFD[modInx])
      {
        NSTACK_CAL_FUN (ops, close, (protoFD[modInx]), ret);
      }
  }
  UNLOCK_FOR_EP (lock_info);
  return -1;
}

/*****************************************************************
Parameters    :  fd
                 addr
                 len
Return        :
Description   :  the code of bind&listen are same, if there are another same api we should extract
*****************************************************************/
int
nstack_bind (int fd, const struct sockaddr *addr, socklen_t addrlen)
{
  nstack_fd_Inf *fdInf;
  int retval = ns_fail;
  int tem = -1;
  int modIdx = 0;
  int tfd;
  int selectmod = -1;
  struct sockaddr_in *iaddr = NULL;
  nstack_rd_key rdkey = { 0 };

  nstack_set_tracing_contex (0, 0, fd);

  NSSOC_LOGINF ("(sockfd=%d, addr=%p, addrlen=%u) [Caller]", fd, addr,
                addrlen);

  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("invalid input]fd=%d,addr=%p,len=0x%x [return]", fd, addr,
                    addrlen);
      return -1;
    }
  if ((NULL == addr) || (addrlen < 2))
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR ("invalid input]fd=%d,addr=%p,len=0x%x [return]", fd, addr,
                    addrlen);
      return -1;
    }
  iaddr = (struct sockaddr_in *) addr;

  if ((addrlen >= sizeof (struct sockaddr_in))
      && ((addr->sa_family) == AF_INET))
    {
      NSSOC_LOGINF ("fd=%d,addr=%s,port=%d", fd, inet_ntoa (iaddr->sin_addr),
                    ntohs (iaddr->sin_port));
    }
  else
    {
      NSSOC_LOGINF ("addrlen = %d ,fd=%d", (int) addrlen, fd);
    }

  NSTACK_FD_LINUX_CHECK (fd, bind, fdInf, (fd, addr, addrlen));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_COMMON (fd, fdInf, local_lock);

  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_COMMON (fd, bind, fdInf, ENOTSOCK,
                                           local_lock);

  /*bind repeat, first time success, other return fail */
  if (fdInf->isBound)
    {
      nstack_set_errno (EINVAL);
      NSPOL_LOGERR ("error, alread bind]fd=%d", fd);
      UNLOCK_COMMON (fd, local_lock);
      return -1;
    }

  /*just support af_inet and pf_inet */
  if (iaddr->sin_family != AF_INET && iaddr->sin_family != PF_INET)
    {
      nstack_set_errno (EAFNOSUPPORT);
      NSSOC_LOGERR ("not surport]fd=%d,domain=%d,[return]", fd,
                    iaddr->sin_family);
      UNLOCK_COMMON (fd, local_lock);
      return -1;
    }

  /* need check addrlen's validity, will visite iaddr->sin_addr.s_addr following code
     for visite iaddr->sin_addr.s_addr is 8 byte  */
  if (addrlen < 8)
    {
      nstack_set_errno (EINVAL);
      NSPOL_LOGERR ("addrlen<sizeof(struct sockaddr_in)]addrlen=%u", addrlen);
      UNLOCK_COMMON (fd, local_lock);
      return -1;
    }

  /* for custom socket, choose stack after creating socket. */
  if (fdInf->ops)
    {
      NSTACK_CAL_FUN (fdInf->ops, bind, (fdInf->rlfd, addr, addrlen), tem);
      if (ns_success == tem)
        {
          retval = ns_success;
          nstack_set_bind_ret (fdInf, fdInf->rmidx, NSTACK_BIND_SUCCESS);
        }
      else
        {
          nstack_set_bind_ret (fdInf, fdInf->rmidx, NSTACK_BIND_FAIL);
        }
      goto bind_over;
    }

  /*loop ip call linux */
  rdkey.type = RD_DATA_TYPE_IP;
  rdkey.ip_addr = iaddr->sin_addr.s_addr;
  retval = nstack_rd_get_stackid (&rdkey, &selectmod);
  if ((ns_success != retval) && (-1 != selectmod))
    {
      NSSOC_LOGWAR ("fd Can't select any module for]fd=%d,IP==%s", fd,
                    inet_ntoa ((iaddr->sin_addr)));
      selectmod = -1;
    }
  else
    {
      NSSOC_LOGINF ("fd addr Select module]fd=%d,addr=%s,module=%s",
                    fd, inet_ntoa (iaddr->sin_addr),
                    nstack_get_module_name_by_idx (selectmod));
    }

  retval = -1;
  nstack_each_modInx (modIdx)
  {
    tfd = nstack_get_protoFd (fdInf, modIdx);
    if ((-1 == tfd) || (selectmod != modIdx))   // for INADDR_ANY, need try to bind on both stack-x and linux
      {
        /*tfd is -1, but is the select module */
        if (selectmod == modIdx)
          {
            retval = -1;
            nstack_set_errno (ENOSYS);
            NSSOC_LOGDBG
              ("fd tfd=-1, but is the select module]fd=%d,tfd=-1,modIdx=%d",
               fd, modIdx);
          }
        nstack_set_bind_ret (fdInf, modIdx, NSTACK_BIND_FAIL);
        continue;
      }

    NSTACK_CAL_FUN (nstack_module_ops (modIdx), bind, (tfd, addr, addrlen),
                    tem);

    if (ns_success == tem)
      {
        fdInf->ops = nstack_module_ops (modIdx);
        nstack_set_router_protocol (fdInf, modIdx);
        nstack_set_routed_fd (fdInf, tfd);
        retval = ns_success;
        nstack_set_bind_ret (fdInf, modIdx, NSTACK_BIND_SUCCESS);
      }
    else
      {
        NSSOC_LOGWAR ("bind fail]module=%s,fd=%d",
                      nstack_get_module_name_by_idx (modIdx), tfd);
        nstack_set_bind_ret (fdInf, modIdx, NSTACK_BIND_FAIL);
      }
  }

  if (-1 == selectmod)
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR ("failed for no module selected]fd=%d", fd);
    }

bind_over:
  if (ns_success == retval)
    {
      fdInf->isBound = 1;
    }
  NSSOC_LOGINF ("appfd=%d,prot_fd=%d,rmidx=%d, retVal=%d [return]", fd,
                fdInf->rlfd, fdInf->rmidx, retval);
  UNLOCK_COMMON (fd, local_lock);
  return retval;
}

int
nstack_listen (int fd, int backlog)
{
  nstack_fd_Inf *fdInf;
  int retval = -1;
  int tem = -1;
  int modIdx = 0;
  int tfd;
  int func_called = 0;

  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGINF ("(sockfd=%d, backlog=%d) [Caller]", fd, backlog);
  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("invalid input]fd=%d,backlog=%d [return]", fd, backlog);
      return -1;
    }

  NSTACK_FD_LINUX_CHECK (fd, listen, fdInf, (fd, backlog));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_COMMON (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_COMMON (fd, listen, fdInf, ENOTSOCK,
                                           local_lock);

  /*listen:use all mode we support */
  nstack_each_modInx (modIdx)
  {
    tfd = nstack_get_protoFd (fdInf, modIdx);

    if ((-1 == tfd)
        || (NSTACK_BIND_FAIL == nstack_get_bind_ret (fdInf, modIdx)))
      {
        continue;
      }

    func_called = 1;
    NSTACK_CAL_FUN (nstack_module_ops (modIdx), listen, (tfd, backlog), tem);
    if (ns_success == tem)
      {
        nstack_set_listen_state (fdInf, modIdx, NSTACK_LISENING);
        NSTACK_SET_FD_LISTEN_SOCKET (fdInf);
        retval = ns_success;
        nstack_set_listen_ret (fdInf, modIdx, NSTACK_LISTEN_SUCCESS);
      }
    else
      {
        NSSOC_LOGWAR ("listen fail]fd=%d,module=%s,tfd=%d", fd,
                      nstack_get_module_name_by_idx (modIdx), tfd);
        nstack_set_listen_ret (fdInf, modIdx, NSTACK_LISTEN_FAIL);
        nstack_set_listen_state (fdInf, modIdx, NSTACK_NO_LISENING);
      }
  }

  if (0 == func_called)
    {
      retval = -1;
      nstack_set_errno (ENOSYS);
      NSSOC_LOGERR ("listen fail for no module called]fd=%d", fd);
    }

  NSSOC_LOGINF ("fd=%d,ret=%d [return]", fd, retval);
  UNLOCK_COMMON (fd, local_lock);
  return retval;
}

int
nstack_accept (int fd, struct sockaddr *addr, socklen_t * addr_len)
{
  nstack_fd_Inf *apstfdInf = NULL;
  int tfd = -1;
  int accfd = -1;
  int fix_fd;
  int ret_fd = -1;
  nstack_fd_Inf *accInf;
  int ret = -1;
  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGINF ("(sockfd=%d, addr=%p, addrlen=%p) [Caller]", fd, addr,
                addr_len);
  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("fd is invalid]fd=%d [return]", fd);
      return -1;
    }
  NSTACK_FD_LINUX_CHECK (fd, accept, apstfdInf, (fd, addr, addr_len));

  nstack_fd_local_lock_info_t *local_lock = &apstfdInf->local_lock;
  LOCK_ACCEPT (fd, apstfdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_ACCEPT (fd, accept, apstfdInf, ENOTSOCK,
                                           local_lock);

  if (addr)
    {
      if ((!addr_len) || (*addr_len == NSTACK_MAX_U32_NUM))
        {
          nstack_set_errno (EINVAL);
          NSSOC_LOGERR ("addr_len inpurt error [return]");
          UNLOCK_ACCEPT (fd, local_lock);
          return -1;
        }
    }

  /*if no module select or listen / bind fail, just return fail */
  if ((!apstfdInf->ops)
      || (NSTACK_LISTEN_FAIL ==
          nstack_get_listen_ret (apstfdInf, apstfdInf->rmidx))
      || (NSTACK_BIND_FAIL ==
          nstack_get_bind_ret (apstfdInf, apstfdInf->rmidx)))
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR
        ("nstack accept fd=%d no mudle select, or bind/listen fail [return]",
         fd);
      UNLOCK_ACCEPT (fd, local_lock);
      return -1;
    }
  tfd = nstack_get_protoFd (apstfdInf, apstfdInf->rmidx);
  NSTACK_CAL_FUN (nstack_module_ops (apstfdInf->rmidx), accept,
                  (tfd, addr, addr_len), accfd);
  NSSOC_LOGINF ("nstack fd=%d:%d accept fd=%d from module=%s", fd, tfd, accfd,
                nstack_get_module_name_by_idx (apstfdInf->rmidx));
  if (-1 == accfd)
    {
      if (errno != EAGAIN)
        {
          NSSOC_LOGERR ("appfd=%d,module=%s,ret=%d,errno=%d [return]", fd,
                        nstack_get_module_name_by_idx (apstfdInf->rmidx),
                        accfd, errno);
        }
      UNLOCK_ACCEPT (fd, local_lock);
      return -1;
    }

  // If it is not from kernel, need to create one kernel socket
  if (apstfdInf->rmidx != nstack_get_fix_mid ())
    {
      /*err num is same with linux */
      fix_fd = nstack_extern_deal (nstack_get_fix_mid ()).stack_alloc_fd ();
      if (fix_fd < 0)
        {
          NSSOC_LOGERR
            ("nstack accept fd=%d return fd=%d kernelFD fd create fail [return]",
             fd, accfd);
          NSTACK_CAL_FUN (nstack_module_ops (apstfdInf->rmidx), close,
                          (accfd), ret);
          UNLOCK_ACCEPT (fd, local_lock);
          return -1;
        }
    }
  else
    {
      fix_fd = accfd;
    }

  if (fix_fd >= (int) NSTACK_KERNEL_FD_MAX)
    {
      /* nstack not support kernel fd >= NSTACK_MAX_SOCK_NUM.
       * close it and nstack_accept() return failed
       */
      NSSOC_LOGERR ("kernelFD fd too big, close it. kernelFD=%d [return]",
                    accfd);
      NSTACK_CAL_FUN (nstack_module_ops (apstfdInf->rmidx), close, (accfd),
                      ret);
      if (apstfdInf->rmidx != nstack_get_fix_mid ())
        {
          NSTACK_CAL_FUN (nstack_fix_mid_ops (), close, (fix_fd), ret);
        }
      nstack_set_errno (EMFILE);
      UNLOCK_ACCEPT (fd, local_lock);
      return -1;
    }

  nstack_fd_local_lock_info_t *lock_info = get_fd_local_lock_info (fix_fd);
  LOCK_FOR_EP (lock_info);

  accInf = nstack_lk_fd_alloc_with_kernel (fix_fd);
  ret_fd = fix_fd;

  if (NULL == accInf)
    {
      NSSOC_LOGERR ("Can't alloc nstack fdInf [return]");
      NSTACK_CAL_FUN (nstack_module_ops (apstfdInf->rmidx), close, (accfd),
                      ret);
      if (apstfdInf->rmidx != nstack_get_fix_mid ())
        {
          NSTACK_CAL_FUN (nstack_fix_mid_ops (), close, (fix_fd), ret);
        }
      UNLOCK_FOR_EP (lock_info);
      nstack_set_errno (EMFILE);
      UNLOCK_ACCEPT (fd, local_lock);

      return -1;
    }

  nstack_set_routed_fd (accInf, accfd);
  accInf->ops = nstack_module_ops (apstfdInf->rmidx);
  /*donot include SOCK_CLOEXEC SOCK_NONBLOCK */
  accInf->type =
    apstfdInf->type & (~((ns_int32) SOCK_CLOEXEC | (ns_int32) SOCK_NONBLOCK));
  nstack_set_router_protocol (accInf, apstfdInf->rmidx);
  nstack_set_protoFd (accInf, apstfdInf->rmidx, accfd);
  nstack_set_app_info (accInf, apstfdInf->rmidx);
  /* Set the linux kernel fd also in accInf for kernel module (0) */
  if (apstfdInf->rmidx != nstack_get_fix_mid ())
    {
      nstack_set_protoFd (accInf, nstack_get_fix_mid (), fix_fd);
    }
  NSSOC_LOGINF ("listenfd=%d,acceptfd=%d,module=%s(rlfd=%d),ret=%d [return]",
                fd, ret_fd, nstack_get_module_name_by_idx (apstfdInf->rmidx),
                accfd, ret_fd);

  set_fd_status_lock_fork (ret_fd, FD_OPEN);
  UNLOCK_FOR_EP (lock_info);
  UNLOCK_ACCEPT (fd, local_lock);
  return ret_fd;
}

int
nstack_accept4 (int fd, struct sockaddr *addr,
                socklen_t * addr_len, int flags)
{
  nstack_fd_Inf *pstfdInf = NULL;
  int tfd = -1;
  int accfd = -1;
  int fix_fd = -1;
  int ret_fd = -1;
  int ret = -1;
  nstack_fd_Inf *accInf;
  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGINF ("(sockfd=%d, addr=%p, addrlen=%p, flags=%d) [Caller]", fd,
                addr, addr_len, flags);
  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("nstack accept4,fd=%d invalid [return]", fd);
      return -1;
    }
  NSTACK_FD_LINUX_CHECK (fd, accept4, pstfdInf, (fd, addr, addr_len, flags));

  nstack_fd_local_lock_info_t *local_lock = &pstfdInf->local_lock;
  LOCK_ACCEPT (fd, pstfdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_ACCEPT (fd, accept4, pstfdInf, ENOTSOCK,
                                           local_lock);

  if (addr)
    {
      if ((!addr_len) || (*addr_len == NSTACK_MAX_U32_NUM))
        {
          nstack_set_errno (EINVAL);
          NSSOC_LOGERR ("nstack accept4 addr_len inpurt error [return]");
          UNLOCK_ACCEPT (fd, local_lock);
          return -1;
        }
    }

  /*if no module select or listen / bind fail, just return fail */
  if ((!pstfdInf->ops)
      || (NSTACK_LISTEN_FAIL ==
          nstack_get_listen_ret (pstfdInf, pstfdInf->rmidx))
      || (NSTACK_BIND_FAIL ==
          nstack_get_bind_ret (pstfdInf, pstfdInf->rmidx)))
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR
        ("nstack accept4 fd:%d no mudle select, or bind/listen fail [return]",
         fd);
      UNLOCK_ACCEPT (fd, local_lock);
      return -1;
    }

  tfd = nstack_get_protoFd (pstfdInf, pstfdInf->rmidx);
  NSTACK_CAL_FUN (nstack_module_ops (pstfdInf->rmidx), accept4,
                  (tfd, addr, addr_len, flags), accfd);
  if (-1 == accfd)
    {
      if (errno != EAGAIN)
        {
          NSSOC_LOGERR ("appfd=%d,module=%s,ret=%d,errno=%d [return]", fd,
                        nstack_get_module_name_by_idx (pstfdInf->rmidx),
                        accfd, errno);
        }
      UNLOCK_ACCEPT (fd, local_lock);
      return -1;
    }

  // If it is not from kernel, need to create one kernel socket
  if (pstfdInf->rmidx != nstack_get_fix_mid ())
    {
      fix_fd = nstack_extern_deal (nstack_get_fix_mid ()).stack_alloc_fd ();
    }
  else
    {
      fix_fd = accfd;
    }

  if (!nstack_is_nstack_sk (fix_fd))
    {
      /* nstack not support kernel fd >= NSTACK_MAX_SOCK_NUM.
       * close it and nstack_accept() return failed
       */
      NSSOC_LOGERR
        ("nstack accept4 fd=%d kernelFD fd too big, close it. kernelFD=%d [return]",
         fd, fix_fd);
      if (fix_fd >= 0)
        {
          NSTACK_CAL_FUN (nstack_module_ops (pstfdInf->rmidx), close, (accfd),
                          ret);
        }

      if (pstfdInf->rmidx != nstack_get_fix_mid ())
        {
          NSTACK_CAL_FUN (nstack_module_ops (nstack_get_fix_mid ()), close,
                          (fix_fd), ret);
        }
      nstack_set_errno (EMFILE);
      UNLOCK_ACCEPT (fd, local_lock);
      return -1;
    }

  nstack_fd_local_lock_info_t *lock_info = get_fd_local_lock_info (fix_fd);
  LOCK_FOR_EP (lock_info);
  accInf = nstack_lk_fd_alloc_with_kernel (fix_fd);
  ret_fd = fix_fd;

  if (NULL == accInf)
    {
      NSTACK_CAL_FUN (nstack_module_ops (pstfdInf->rmidx), close, (accfd),
                      ret);
      if (pstfdInf->rmidx != nstack_get_fix_mid ())
        {
          NSTACK_CAL_FUN (nstack_module_ops (nstack_get_fix_mid ()), close,
                          (fix_fd), ret);
        }
      UNLOCK_FOR_EP (lock_info);
      NSSOC_LOGERR ("nstack accept fd alloc is NULL [return]");
      UNLOCK_ACCEPT (fd, local_lock);
      return -1;
    }

  nstack_set_routed_fd (accInf, accfd);
  accInf->ops = nstack_module_ops (pstfdInf->rmidx);
  accInf->type =
    (pstfdInf->type & (~((ns_int32) SOCK_CLOEXEC | (ns_int32) SOCK_NONBLOCK)))
    | (ns_int32) flags;
  nstack_set_router_protocol (accInf, pstfdInf->rmidx);
  nstack_set_protoFd (accInf, pstfdInf->rmidx, accfd);
  nstack_set_app_info (accInf, pstfdInf->rmidx);
  /* Set the linux kernel fd also in accInf for kernel module (0) */
  if (pstfdInf->rmidx != nstack_get_fix_mid ())
    {
      nstack_set_protoFd (accInf, nstack_get_fix_mid (), fix_fd);
    }
  NSSOC_LOGINF
    ("listenfd=%d,acceptfd=%d,accInf->fd=%d,module=%s(rlfd:%d),ret=%d [return]",
     fd, ret_fd, accInf->fd, nstack_get_module_name_by_idx (pstfdInf->rmidx),
     accfd, ret_fd);
  set_fd_status_lock_fork (ret_fd, FD_OPEN);
  UNLOCK_FOR_EP (lock_info);
  UNLOCK_ACCEPT (fd, local_lock);
  return ret_fd;
}

/*****************************************************************
Parameters    :  fd
                 addr
                 len
Return        :
Description   :  use the rd select rlfd or default rlfd to Estblsh connection. the unused fd should closed
*****************************************************************/
int
nstack_connect (int fd, const struct sockaddr *addr, socklen_t addrlen)
{
  int retval = -1;
  nstack_fd_Inf *fdInf;
  struct sockaddr_in *iaddr = NULL;
  iaddr = (struct sockaddr_in *) addr;
  int selectmod = -1;
  nstack_rd_key rdkey = { 0 };

  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGINF ("(sockfd=%d, addr=%p, addrlen=%u) [Caller]", fd, addr,
                addrlen);

  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR
        ("nstack connect, fd=%d invalid input: addr=%p,len=0x%x [return]", fd,
         addr, addrlen);
      return -1;
    }
  if ((NULL == addr) || (addrlen < 2))
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR
        ("nstack connect, fd=%d invalid input: addr=%p,len=0x%x [return]", fd,
         addr, addrlen);
      return -1;
    }

  if ((addrlen >= sizeof (struct sockaddr_in))
      && ((addr->sa_family) == AF_INET))
    {
      NSSOC_LOGINF ("fd=%d,addr=%s,port=%d", fd, inet_ntoa (iaddr->sin_addr),
                    ntohs (iaddr->sin_port));
    }
  else
    {
      NSSOC_LOGINF ("addrlen = %d ,fd=%d", (int) addrlen, fd);
    }

  /* need check addrlen's validity, will visite iaddr->sin_addr.s_addr following code,for visite iaddr->sin_addr.s_addr is 8 byte  */
  if (addrlen < 8)
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR
        ("nstack connect, fd=%d invalid addrlen input: addr=%p,len=0x%x [return]",
         fd, addr, addrlen);
      return -1;
    }

  if (NSTACK_ANY_IP == iaddr->sin_addr.s_addr)
    {
      nstack_set_errno (ECONNREFUSED);
      NSSOC_LOGERR
        ("nstack connect, fd=%d invalid input: 0==addr_in->sin_addr.s_addr [return]",
         fd);
      return -1;
    }
  else if (NSTACK_MAX_U32_NUM == iaddr->sin_addr.s_addr)
    {
      nstack_set_errno (ENETUNREACH);
      NSSOC_LOGERR
        ("nstack connect, fd=%d invalid input: 0xffffffff==addr_in->sin_addr.s_addr [return]",
         fd);
      return -1;
    }
  NSTACK_FD_LINUX_CHECK (fd, connect, fdInf, (fd, addr, addrlen));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_CONNECT (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_CONNECT (fd, connect, fdInf, ENOTSOCK,
                                            local_lock);

  /*if no module select, according to dest ip */
  if (!fdInf->ops)
    {
      rdkey.type = RD_DATA_TYPE_IP;
      rdkey.ip_addr = iaddr->sin_addr.s_addr;
      retval = nstack_rd_get_stackid (&rdkey, &selectmod);
      if (ns_success == retval && selectmod >= 0)
        {
          NSSOC_LOGINF ("fd=%d addr=%s Select module=%s",
                        fd, inet_ntoa (iaddr->sin_addr),
                        nstack_get_module_name_by_idx (selectmod));
          /*in case of that multi-thread connect. if route was chosed by one thread, the other just use the first one */
          fdInf->rmidx = selectmod;
          fdInf->ops = nstack_module_ops (selectmod);
          nstack_set_routed_fd (fdInf, nstack_get_protoFd (fdInf, selectmod));
          nstack_set_router_protocol (fdInf, selectmod);
        }
      else
        {
          NSSOC_LOGERR ("fd=%d Callback select module=%d ret=0x%x", fd,
                        selectmod, retval);
          nstack_set_errno (ENETUNREACH);
          UNLOCK_CONNECT (fd, local_lock);
          return -1;
        }
      NSSOC_LOGINF ("fd=%d addr=%s Select module=%s",
                    fd, inet_ntoa (iaddr->sin_addr),
                    nstack_get_module_name_by_idx (selectmod));
    }

  NSTACK_CAL_FUN (fdInf->ops, connect, (fdInf->rlfd, addr, addrlen), retval);
  if (-1 == retval && errno != EINPROGRESS)
    {
      NSSOC_LOGERR ("appfd=%d,module=%s,proto_fd=%d,ret=%d,errno=%d [return]",
                    fd, nstack_get_module_name_by_idx (fdInf->rmidx),
                    fdInf->rlfd, retval, errno);
    }
  else
    {
      NSSOC_LOGINF ("appfd=%d,module=%s,proto_fd=%d,ret=%d,errno=%d [return]",
                    fd, nstack_get_module_name_by_idx (fdInf->rmidx),
                    fdInf->rlfd, retval, errno);
    }
  UNLOCK_CONNECT (fd, local_lock);
  return retval;
}

int
nstack_shutdown (int fd, int how)
{
  nstack_fd_Inf *fdInf = NULL;
  int retval = -1;
  int tfd;

  nstack_set_tracing_contex (0, 0, fd);
  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("fd=%d invalid input [return]", fd);
      return -1;
    }

  NSSOC_LOGINF ("(fd=%d, how=%d) [Caller]", fd, how);

  NSTACK_FD_LINUX_CHECK (fd, shutdown, fdInf, (fd, how));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_COMMON (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_COMMON (fd, shutdown, fdInf, ENOTSOCK,
                                           local_lock);

  if (!fdInf->ops || -1 == fdInf->rlfd)
    {
      NSSOC_LOGWAR ("fd=%d,how=%d, shutdown fail [return]", fd, how);
      nstack_set_errno (ENOTCONN);
      UNLOCK_COMMON (fd, local_lock);
      return -1;
    }
  tfd = fdInf->rlfd;
  NSTACK_CAL_FUN (fdInf->ops, shutdown, (tfd, how), retval);
  NSSOC_LOGINF ("fd=%d,ret=%d [return]", fd, retval);
  UNLOCK_COMMON (fd, local_lock);
  return retval;
}

int
release_fd (int fd, nstack_fd_local_lock_info_t * local_lock)
{
  nstack_fd_Inf *fdInf = NULL;
  nstack_module *pMod = NULL;
  int retval = 0;
  int curRet = -1;
  int modInx, tfd;

  if (!local_lock)
    {
      return -1;
    }

  LOCK_CLOSE (local_lock);

  /* if fd is used by others, just pass, delay close it */
  if (local_lock->fd_status != FD_CLOSING || local_lock->fd_ref.counter > 0)
    {
      UNLOCK_CLOSE (local_lock);
      return 0;
    }
  local_lock->fd_status = FD_CLOSE;

  fdInf = nstack_fd2inf (fd);
  if (NULL == fdInf)
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR ("pstfdInf is NULL");
      UNLOCK_CLOSE (local_lock);
      return -1;
    }

  (void) nsep_epoll_close (fd);

  nstack_each_module (modInx, pMod)
  {
    tfd = nstack_get_protoFd (fdInf, modInx);

    if (nstack_get_minfd_id (modInx) > tfd
        || tfd > nstack_get_maxfd_id (modInx))
      {
        continue;
      }

    NSSOC_LOGINF ("fd=%d,module=%s,tfd=%d", fd,
                  nstack_get_module_name_by_idx (modInx), tfd);

    if (0 ==
        strcmp (nstack_fix_stack_name (),
                nstack_get_module_name_by_idx (modInx)))
      {
        continue;
      }

    nssct_close (fd, modInx);
    NSTACK_CAL_FUN ((&pMod->mops.socket_ops), close, (tfd), curRet);

    if (-1 == curRet)
      {
        NSSOC_LOGERR ("failed]module=%s,tfd=%d,errno=%d",
                      nstack_get_module_name_by_idx (modInx), tfd, errno);
      }

    retval &= curRet;
  }
  retval &= nstack_fd_free_with_kernel (fdInf);
  if (-1 == retval)
    {
      NSSOC_LOGWAR ("fd=%d,ret=%d [return]", fd, retval);
    }
  else
    {
      NSSOC_LOGINF ("fd=%d,ret=%d [return]", fd, retval);
    }

  UNLOCK_CLOSE (local_lock);
  return retval;
}

/*not support fork now,  to support fork the module must provide gfdt & refer cnt
  while fork the frame use callback fun to add refer*/
int
nstack_close (int fd)
{
  nstack_fd_Inf *fdInf;
  int ret = -1;
  nstack_set_tracing_contex (0, 0, fd);

  /*linux fd check */
  if (!(fdInf = nstack_getValidInf (fd)))
    {
      if (NSTACK_THREAD_LOADING ())
        {
          return nsfw_base_close (fd);
        }
      if (0 != nstack_stack_module_load ())
        {
          return -1;
        }
      if (nstack_fix_fd_check ()
          && nstack_fix_fd_check ()(fd, STACK_FD_FUNCALL_CHECK))
        {
          /*free epoll resouce */
          nsep_epoll_close (fd);
          nssct_close (fd, nstack_get_fix_mid ());

          NSTACK_CAL_FUN (nstack_fix_mid_ops (), close, (fd), ret);
          return ret;
        }
      nstack_set_errno (ENOSYS);
      return -1;
    }

  NSSOC_LOGINF ("Caller]fd=%d", fd);

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_CLOSE (local_lock);
  if (local_lock->fd_status != FD_OPEN)
    {
      NSSOC_LOGERR ("return]fd_status=%d,fd=%d", local_lock->fd_status, fd);
      nstack_set_errno (EBADF);
      UNLOCK_CLOSE (local_lock);
      return -1;
    }

  set_fd_status_lock_fork (fd, FD_CLOSING);

  UNLOCK_CLOSE (local_lock);
  ret =
    (atomic_dec (&local_lock->fd_ref) > 0 ? 0 : release_fd (fd, local_lock));

  if (-1 == ret)
    {
      NSSOC_LOGWAR ("return]fd=%d,retVal=%d", fd, ret);
    }
  else
    {
      NSSOC_LOGINF ("return]fd=%d,retVal=%d", fd, ret);
    }

  return ret;
}

ssize_t
nstack_send (int fd, const void *buf, size_t len, int flags)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_SEND, NSOCKET, "NSSOC", NSLOG_DBG,
               "(sockfd=%d, buf=%p, len=%zu, flags=%d) [Caller]", fd, buf,
               len, flags);

  NSTACK_FD_LINUX_CHECK (fd, send, fdInf, (fd, buf, len, flags));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_SEND (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_SEND (fd, send, fdInf, ENOTSOCK,
                                         local_lock);

  if ((!fdInf->ops) || (-1 == fdInf->rlfd))
    {
      NSSOC_LOGINF ("fd Fail: Not select any module yet!]fd=%d [return]", fd);
      nstack_set_errno (ENOTCONN);
      UNLOCK_SEND (fd, local_lock);
      return -1;
    }

    /* *INDENT-OFF* */
    CPUB(stack_x_send)
    NSTACK_CAL_FUN(fdInf->ops, send, (fdInf->rlfd, buf, len, flags), size);
    CPUE(stack_x_send)
    /* *INDENT-ON* */
  NSSOC_LOGDBG ("fd=%d,ret=%d [Return]", fd, size);

  UNLOCK_SEND (fd, local_lock);
  return size;
}

ssize_t
nstack_recv (int fd, void *buf, size_t len, int flags)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_RECV, NSOCKET, "NSSOC", NSLOG_DBG,
               "(sockfd=%d, buf=%p, len=%zu, flags=%d) [Caller]", fd, buf,
               len, flags);

  NSTACK_FD_LINUX_CHECK (fd, recv, fdInf, (fd, buf, len, flags));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_RECV (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_RECV (fd, recv, fdInf, ENOTSOCK,
                                         local_lock);

  if ((!fdInf->ops) || (-1 == fdInf->rlfd))
    {
      NSSOC_LOGINF ("Not select any module yet!]fd=%d [Return]", fd);
      nstack_set_errno (ENOTCONN);
      UNLOCK_RECV (fd, local_lock);
      return -1;
    }

    /* *INDENT-OFF* */
    CPUB(stack_x_recv)
    NSTACK_CAL_FUN(fdInf->ops, recv, (fdInf->rlfd, buf, len, flags), size);
    CPUE(stack_x_recv)
    /* *INDENT-ON* */
  NSSOC_LOGDBG ("fd=%d,ret=%d [Return]", fd, size);

  UNLOCK_RECV (fd, local_lock);
  return size;
}

ssize_t
nstack_write (int fd, const void *buf, size_t count)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;

  NSTACK_FD_LINUX_CHECK (fd, write, fdInf, (fd, buf, count));

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_WRITE, NSOCKET, "NSSOC", NSLOG_DBG,
               "(fd=%d, buf=%p, count=%zu) [Caller]", fd, buf, count);

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_SEND (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_SEND (fd, write, fdInf, EINVAL,
                                         local_lock);

  if ((!fdInf->ops) || (-1 == fdInf->rlfd))
    {
      NSSOC_LOGINF ("Not select any module yet!]fd=%d [Return]", fd);
      nstack_set_errno (ENOTCONN);
      UNLOCK_SEND (fd, local_lock);
      return -1;
    }

    /* *INDENT-OFF* */
    CPUB(stack_x_write)
    NSTACK_CAL_FUN(fdInf->ops, write, (fdInf->rlfd, buf, count), size);
    CPUE(stack_x_write)
    /* *INDENT-ON* */

  NSSOC_LOGDBG ("fd=%d,ret=%d [Return]", fd, size);

  UNLOCK_SEND (fd, local_lock);
  return size;
}

ssize_t
nstack_read (int fd, void *buf, size_t count)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_READ, NSOCKET, "NSSOC", NSLOG_DBG,
               "(fd=%d, buf=%p, count=%zu) [Caller]", fd, buf, count);

  NSTACK_FD_LINUX_CHECK (fd, read, fdInf, (fd, buf, count));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_RECV (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_RECV (fd, read, fdInf, EINVAL, local_lock);

  if ((!fdInf->ops) || (-1 == fdInf->rlfd))
    {
      NSSOC_LOGINF ("Not select any module yet!]fd=%d [Return]", fd);
      nstack_set_errno (ENOTCONN);
      UNLOCK_RECV (fd, local_lock);
      return -1;
    }

    /* *INDENT-OFF* */
    CPUB(stack_x_read)
    NSTACK_CAL_FUN(fdInf->ops, read, (fdInf->rlfd, buf, count), size);
    CPUE(stack_x_read)
    /* *INDENT-ON* */

  NSSOC_LOGDBG ("fd=%d,ret=%d [Return]", fd, size);

  UNLOCK_RECV (fd, local_lock);
  return size;
}

ssize_t
nstack_writev (int fd, const struct iovec * iov, int iovcnt)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_WRITEV, NSOCKET, "NSSOC", NSLOG_DBG,
               "(fd=%d, iov=%p, count=%d) [Caller]", fd, iov, iovcnt);

  NSTACK_FD_LINUX_CHECK (fd, writev, fdInf, (fd, iov, iovcnt));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_SEND (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_SEND (fd, writev, fdInf, EINVAL,
                                         local_lock);

  if ((!fdInf->ops) || (-1 == fdInf->rlfd))
    {
      NSSOC_LOGERR ("Not select any module yet!]fd=%d [Return]", fd);
      nstack_set_errno (ENOTCONN);
      UNLOCK_SEND (fd, local_lock);
      return -1;
    }

    /* *INDENT-OFF* */
    CPUB(stack_x_writev)
    NSTACK_CAL_FUN(fdInf->ops, writev, (fdInf->rlfd, iov, iovcnt), size);
    CPUE(stack_x_writev)
    /* *INDENT-ON* */

  NSSOC_LOGDBG ("fd=%d,ret=%d [Return]", fd, size);

  UNLOCK_SEND (fd, local_lock);
  return size;
}

ssize_t
nstack_readv (int fd, const struct iovec * iov, int iovcnt)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_READV, NSOCKET, "NSSOC", NSLOG_DBG,
               "(fd=%d, iov=%p, count=%d) [Caller]", fd, iov, iovcnt);

  NSTACK_FD_LINUX_CHECK (fd, readv, fdInf, (fd, iov, iovcnt));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_RECV (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_RECV (fd, readv, fdInf, EINVAL,
                                         local_lock);

  if ((!fdInf->ops) || (-1 == fdInf->rlfd))
    {
      NSSOC_LOGERR ("Not select any module yet!]fd=%d [Return]", fd);
      nstack_set_errno (ENOTCONN);
      UNLOCK_RECV (fd, local_lock);
      return -1;
    }

    /* *INDENT-OFF* */
    CPUB(stack_x_readv)
    NSTACK_CAL_FUN(fdInf->ops, readv, (fdInf->rlfd, iov, iovcnt), size);
    CPUE(stack_x_readv)
    /* *INDENT-ON* */

  NSSOC_LOGDBG ("fd=%d,ret=%d [Return]", fd, size);

  UNLOCK_RECV (fd, local_lock);
  return size;
}

/*we assumed that the connect allready called, if not call, we must try many sok*/
ssize_t
nstack_sendto (int fd, const void *buf, size_t len, int flags,
               const struct sockaddr * dest_addr, socklen_t addrlen)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;
  struct sockaddr_in *iaddr = NULL;
  int selectmod = -1;
  int retval = 0;
  nstack_rd_key rdkey = { 0 };
  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGDBG
    ("(sockfd=%d, buf=%p, len=%zu, flags=%d, dest_addr=%p, addrlen=%u) [Caller]",
     fd, buf, len, flags, dest_addr, addrlen);

  NSTACK_FD_LINUX_CHECK (fd, sendto, fdInf,
                         (fd, buf, len, flags, dest_addr, addrlen));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_SEND (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_SEND (fd, sendto, fdInf, ENOTSOCK,
                                         local_lock);

  if (fdInf->ops)
    {
        /* *INDENT-OFF* */
        CPUB(stack_x_sendto)
        NSTACK_CAL_FUN(fdInf->ops, sendto, (fdInf->rlfd, buf, len, flags, dest_addr, addrlen), size);
        CPUE(stack_x_sendto)
        /* *INDENT-ON* */

      NSSOC_LOGDBG
        ("fdInf->ops:fd=%d buf=%p,len=%zu,size=%d,addr=%p [Return]", fd, buf,
         len, size, dest_addr);
      UNLOCK_SEND (fd, local_lock);
      return size;
    }
  /*invalid ip, just return */
  /*validity check for addrlen: for visite iaddr->sin_addr.s_addr is 8 byte */
  if ((NULL == dest_addr) || (addrlen < 8))
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR ("invalid input]fd=%d,buf=%p,len=%zu,addr=%p [Return]", fd,
                    buf, len, dest_addr);
      UNLOCK_SEND (fd, local_lock);
      return -1;
    }

  iaddr = (struct sockaddr_in *) dest_addr;
  rdkey.type = RD_DATA_TYPE_IP;
  rdkey.ip_addr = iaddr->sin_addr.s_addr;
  retval = nstack_rd_get_stackid (&rdkey, &selectmod);
  if ((ns_success == retval) && (selectmod >= 0))
    {
      NSSOC_LOGINF ("fd=%d,addr=%s,select_module=%s",
                    fd, inet_ntoa (iaddr->sin_addr),
                    nstack_get_module_name_by_idx (selectmod));
      fdInf->rmidx = selectmod;
      nstack_set_routed_fd (fdInf, nstack_get_protoFd (fdInf, selectmod));
      nstack_set_router_protocol (fdInf, selectmod);
      fdInf->ops = nstack_module_ops (selectmod);
    }
  else
    {
      NSSOC_LOGERR ("fd=%d Callback select module=%d,ret=0x%x", fd, selectmod,
                    retval);
      nstack_set_errno (ENETUNREACH);
      UNLOCK_SEND (fd, local_lock);
      return -1;
    }

  NSSOC_LOGDBG ("fd=%d,addr=%s,select_module=%s",
                fd, inet_ntoa (iaddr->sin_addr),
                nstack_get_module_name_by_idx (selectmod));

  NSTACK_CAL_FUN (fdInf->ops, sendto,
                  (fdInf->rlfd, buf, len, flags, dest_addr, addrlen), size);

  NSSOC_LOGDBG ("fd=%d,module=%s,ret=%d [Return]", fd,
                nstack_get_module_name_by_idx (fdInf->rmidx), size);

  UNLOCK_SEND (fd, local_lock);
  return size;
}

ssize_t
nstack_sendmsg (int fd, const struct msghdr * msg, int flags)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;
  struct sockaddr_in *iaddr = NULL;
  int selectmod = -1;
  int retval = 0;
  nstack_rd_key rdkey = { 0 };

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_SENDMSG, NSOCKET, "NSSOC", NSLOG_DBG,
               "(sockfd=%d, msg=%p, flags=%d) [Caller]", fd, msg, flags);

  if (NULL == msg)
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR ("invalid input]fd=%d,msg=%p [Return]", fd, msg);
      return -1;
    }

  NSTACK_FD_LINUX_CHECK (fd, sendmsg, fdInf, (fd, msg, flags));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_SEND (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_SEND (fd, sendmsg, fdInf, ENOTSOCK,
                                         local_lock);

  /*if some module select, just connect */
  if (fdInf->ops)
    {
        /* *INDENT-OFF* */
        CPUB(stack_x_sendmsg)
        NSTACK_CAL_FUN(fdInf->ops, sendmsg, (fdInf->rlfd, msg, flags), size);
        CPUE(stack_x_sendmsg)
        /* *INDENT-ON* */
      NSSOC_LOGDBG ("]fd=%d,size=%d msg=%p [Return]", fd, size, msg);
      UNLOCK_SEND (fd, local_lock);
      return size;
    }

  iaddr = (struct sockaddr_in *) msg->msg_name;
  /* validity check for msg->msg_namelen,for visite iaddr->sin_addr.s_addr is 8 byte  */
  if ((NULL == iaddr) || (msg->msg_namelen < 8))
    {
      NSSOC_LOGINF ("fd addr is null and select linux module]fd=%d", fd);
      fdInf->ops = nstack_module_ops (nstack_get_fix_mid ());
      nstack_set_routed_fd (fdInf,
                            nstack_get_protoFd (fdInf,
                                                nstack_get_fix_mid ()));
      nstack_set_router_protocol (fdInf, nstack_get_fix_mid ());
    }
  else
    {
      rdkey.type = RD_DATA_TYPE_IP;
      rdkey.ip_addr = iaddr->sin_addr.s_addr;
      retval = nstack_rd_get_stackid (&rdkey, &selectmod);
      if (ns_success == retval)
        {
          NSSOC_LOGINF ("fd=%d,addr=%s,select_module=%s",
                        fd, inet_ntoa (iaddr->sin_addr),
                        nstack_get_module_name_by_idx (selectmod));
          fdInf->rmidx = selectmod;
          nstack_set_routed_fd (fdInf, nstack_get_protoFd (fdInf, selectmod));
          nstack_set_router_protocol (fdInf, selectmod);
          fdInf->ops = nstack_module_ops (selectmod);
        }
      else
        {
          NSSOC_LOGERR ("fd=%d Callback select_module=%d,ret=0x%x", fd,
                        selectmod, retval);
          nstack_set_errno (ENETUNREACH);
          UNLOCK_SEND (fd, local_lock);
          return -1;
        }
      NSSOC_LOGDBG ("fd=%d,addr=%s,select_module=%s",
                    fd, inet_ntoa (iaddr->sin_addr),
                    nstack_get_module_name_by_idx (selectmod));
    }

  NSTACK_CAL_FUN (fdInf->ops, sendmsg, (fdInf->rlfd, msg, flags), size);

  NSSOC_LOGDBG ("fd=%d,module=%s,ret=%d [Return]", fd,
                nstack_get_module_name_by_idx (fdInf->rmidx), size);

  UNLOCK_SEND (fd, local_lock);
  return size;
}

/*we assumed that the connect allready called, if not call, we must try many sok*/
ssize_t
nstack_recvfrom (int fd, void *buf, size_t len, int flags,
                 struct sockaddr * src_addr, socklen_t * addrlen)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;

  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGDBG
    ("(sockfd=%d, buf=%p, len=%zu, flags=%d, src_addr=%p, addrlen=%p) [Caller]",
     fd, buf, len, flags, src_addr, addrlen);

  if (NULL == buf)
    {
      nstack_set_errno (EFAULT);
      NSSOC_LOGERR ("invalid input]fd=%d,buf=%p [Return]", fd, buf);
      return -1;
    }

  NSTACK_FD_LINUX_CHECK (fd, recvfrom, fdInf,
                         (fd, buf, len, flags, src_addr, addrlen));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_RECV (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_RECV (fd, recvfrom, fdInf, ENOTSOCK,
                                         local_lock);

  if ((!fdInf->ops) || (-1 == fdInf->rlfd))
    {
      NSSOC_LOGINF ("Not select any module yet!]fd=%d [Return]", fd);
      nstack_set_errno (ENOTCONN);
      UNLOCK_RECV (fd, local_lock);
      return -1;
    }

    /* *INDENT-OFF* */
    CPUB(stack_x_recvfrom)
    NSTACK_CAL_FUN(fdInf->ops, recvfrom, (fdInf->rlfd, buf, len, flags, src_addr, addrlen), size);
    CPUE(stack_x_recvfrom)
    /* *INDENT-ON* */

  NSSOC_LOGDBG ("fd=%d,retVal=%d [Return]", fd, size);

  UNLOCK_RECV (fd, local_lock);
  return size;
}

ssize_t
nstack_recvmsg (int fd, struct msghdr * msg, int flags)
{
  nstack_fd_Inf *fdInf = NULL;
  int size = -1;

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_RECVMSG, NSOCKET, "NSSOC", NSLOG_DBG,
               "(sockfd=%d, msg=%p, flags=%d) [Caller]", fd, msg, flags);

  NSTACK_FD_LINUX_CHECK (fd, recvmsg, fdInf, (fd, msg, flags));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_RECV (fd, fdInf, local_lock);
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_RECV (fd, recvmsg, fdInf, ENOTSOCK,
                                         local_lock);

  if ((-1 == fdInf->rlfd) || (NULL == fdInf->ops))
    {
      NSSOC_LOGERR ("Not select any module yet!]fd=%d [Return]", fd);
      nstack_set_errno (ENOTCONN);
      UNLOCK_RECV (fd, local_lock);
      return -1;
    }

    /* *INDENT-OFF* */
    CPUB(stack_x_recvmsg)
    NSTACK_CAL_FUN(fdInf->ops, recvmsg, (fdInf->rlfd, msg, flags), size);
    CPUE(stack_x_recvmsg)
    /* *INDENT-ON* */

  NSSOC_LOGDBG ("fd=%d,ret=%d [Return]", fd, size);

  UNLOCK_RECV (fd, local_lock);
  return size;
}

/*****************************************************************
Parameters    :  fd
                 addr
                 len
Return        :
Description   : all module fd bind to same addr, so just use first rlfd to get bind addr.
*****************************************************************/
int
nstack_getsockname (int fd, struct sockaddr *addr, socklen_t * addrlen)
{
  nstack_fd_Inf *fdInf = NULL;
  int tfd = -1;
  int ret = -1;

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_GETSOCKNAME, NSOCKET, "NSSOC", NSLOG_INF,
               "(fd=%d, addr=%p, addrlen=%p) [Caller]", fd, addr, addrlen);

  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("invalid input]fd=%d. [return]", fd);
      return -1;
    }

  NSTACK_FD_LINUX_CHECK (fd, getsockname, fdInf, (fd, addr, addrlen));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_COMMON (fd, fdInf, local_lock);

  if ((NULL != fdInf->ops) && (fdInf->rlfd != -1))
    {
      tfd = fdInf->rlfd;
      NSTACK_CAL_FUN (fdInf->ops, getsockname, (tfd, addr, addrlen), ret);
      NSSOC_LOGINF ("fd=%d,module=%s,tfd=%d [return]", fd,
                    nstack_get_module_name_by_idx (fdInf->rmidx), tfd);
      if (-1 == ret)
        {
          NSSOC_LOGERR ("rmidx=%d,fd=%d return fail [return]", fdInf->rmidx,
                        tfd);
        }
      UNLOCK_COMMON (fd, local_lock);
      return ret;
    }

  if (NULL != g_nstack_modules.defMod)
    {
      tfd = nstack_get_protoFd (fdInf, nstack_defMod_inx ());
      if (tfd >= 0)
        {
          nstack_socket_ops *ops = nstack_def_ops ();
          NSTACK_CAL_FUN (ops, getsockname, (tfd, addr, addrlen), ret);
          NSSOC_LOGINF ("fd=%d,module=%s,tfd=%d [return]", fd,
                        nstack_defmod_name (), tfd);
          if (-1 == ret)
            {
              NSSOC_LOGERR ("return fail]mudle=%d,fd=%d  [return]",
                            nstack_defMod_inx (), tfd);
            }
          UNLOCK_COMMON (fd, local_lock);
          return ret;
        }
    }

  nstack_set_errno (ENOTSOCK);
  NSSOC_LOGINF ("fd=%d,ret=%d [Return]", fd, ret);
  UNLOCK_COMMON (fd, local_lock);
  return ret;

}

/*****************************************************************
Parameters    :  fd
                 addr
                 len
Return        :
Description   : getpeername only used by the fd who already Estblsh connection, so use first rlfd.
*****************************************************************/
int
nstack_getpeername (int fd, struct sockaddr *addr, socklen_t * addrlen)
{
  nstack_fd_Inf *fdInf;
  int tfd;
  int ret = -1;

  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_GETPEERNAME, NSOCKET, "NSSOC", NSLOG_INF,
               "(fd=%d, addr=%p, addrlen=%p) [Caller]", fd, addr, addrlen);

  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("invalid input, fd=%d. [return]", fd);
      return -1;
    }

  NSTACK_FD_LINUX_CHECK (fd, getpeername, fdInf, (fd, addr, addrlen));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_COMMON (fd, fdInf, local_lock);

  /*?????????,????????????? */
  if (fdInf->ops)
    {
      tfd = fdInf->rlfd;
      NSTACK_CAL_FUN (fdInf->ops, getpeername, (tfd, addr, addrlen), ret);
      NSSOC_LOGINF ("fd=%d,module=%s,rlfd=%d,ret=%d [return]",
                    fd, nstack_get_module_name_by_idx (fdInf->rmidx),
                    fdInf->rlfd, ret);
      if (-1 == ret)
        {
          NSSOC_LOGERR ("return fail]mudle=%d,fd=%d [return]", fdInf->rmidx,
                        tfd);
        }
      UNLOCK_COMMON (fd, local_lock);
      return ret;
    }

  if (NULL != g_nstack_modules.defMod)
    {
      tfd = nstack_get_protoFd (fdInf, nstack_defMod_inx ());
      if (tfd >= 0)
        {
          nstack_socket_ops *ops = nstack_def_ops ();
          NSTACK_CAL_FUN (ops, getpeername, (tfd, addr, addrlen), ret);
          NSSOC_LOGINF ("fd=%d,module=%s,tfd=%d [return]", fd,
                        nstack_defmod_name (), tfd);
          if (-1 == ret)
            {
              NSSOC_LOGERR ("return fail] mudle=%d,fd=%d [return]",
                            nstack_defMod_inx (), tfd);
            }
          UNLOCK_COMMON (fd, local_lock);
          return ret;
        }
    }

  nstack_set_errno (ENOTSOCK);
  NSSOC_LOGINF ("fd=%d,ret=%d [Return]", fd, ret);
  UNLOCK_COMMON (fd, local_lock);
  return ret;
}

int
nstack_option_set (nstack_fd_Inf * fdInf, int optname, const void *optval,
                   socklen_t optlen)
{

#define SLEEP_MAX   10000000
  if ((!optval) || (optlen < sizeof (u32_t)))
    {
      NSSOC_LOGINF ("rong parmeter optname]=%d", optname);
      nstack_set_errno (EINVAL);
      return -1;
    }

  switch (optname)
    {
    case NSTACK_SEM_SLEEP:
      if ((*(u32_t *) optval) > SLEEP_MAX)
        {
          NSSOC_LOGWAR ("time overflow]epfd=%d", fdInf->fd);
          nstack_set_errno (EINVAL);
          return -1;
        }

      nsep_set_infoSleepTime (fdInf->fd, *(u32_t *) optval);
      NSSOC_LOGINF ("set sem wait option] g_sem_sleep_time=%ld",
                    *(u32_t *) optval);
      break;

    default:
      NSSOC_LOGINF ("rong parmeter optname]=%d", optname);
      nstack_set_errno (ENOPROTOOPT);
      return -1;
    }
  return 0;
}

int
nstack_option_get (nstack_fd_Inf * fdInf, int optname, const void *optval,
                   socklen_t * optlen)
{

  if ((!optval) || (!optlen) || (optlen && (*optlen < sizeof (u32_t))))
    {
      NSSOC_LOGINF ("rong parmeter optname]=%d", optname);
      nstack_set_errno (EINVAL);
      return -1;
    }

  switch (optname)
    {
    case NSTACK_SEM_SLEEP:
      *(long *) optval = nsep_get_infoSleepTime (fdInf->fd);
      NSSOC_LOGINF ("get sem wait option] g_sem_sleep_time=%ld",
                    *(long *) optval);
      break;

    default:
      NSSOC_LOGINF ("rong parmeter optname]=%d", optname);
      nstack_set_errno (ENOPROTOOPT);
      return -1;
    }
  return 0;
}

/* just use first rlfd to getsockopt,  this may not what app  really want.*/
/* Currently, if getsockopt is successfull either in kernel or stack-x, the below API returns SUCCESS */
int
nstack_getsockopt (int fd, int level, int optname, void *optval,
                   socklen_t * optlen)
{
  nstack_fd_Inf *fdInf;
  int tfd;
  int ret = -1;
  nstack_socket_ops *ops;
  nstack_set_tracing_contex (0, 0, fd);
  NS_LOG_CTRL (LOG_CTRL_GETSOCKOPT, NSOCKET, "NSSOC", NSLOG_INF,
               "(fd=%d, level=%d, optname=%d, optval=%p, optlen=%p) [Caller]",
               fd, level, optname, optval, optlen);

  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("invalid input]fd=%d [return]", fd);
      return -1;
    }

  NSTACK_FD_LINUX_CHECK (fd, getsockopt, fdInf,
                         (fd, level, optname, optval, optlen));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_COMMON (fd, fdInf, local_lock);

  if ((NSTACK_SOCKOPT == level) && NSTACK_IS_FD_EPOLL_SOCKET (fdInf))
    {
      ret = nstack_option_get (fdInf, optname, optval, optlen);
      UNLOCK_COMMON (fd, local_lock);
      return ret;
    }

  if (fdInf->ops)
    {
      tfd = fdInf->rlfd;
      NSTACK_CAL_FUN (fdInf->ops, getsockopt,
                      (tfd, level, optname, optval, optlen), ret);
      NSSOC_LOGINF
        ("fd=%d,module=%s,tfd=%d,level=%d,optname=%d,ret=%d [return]", fd,
         nstack_get_module_name_by_idx (fdInf->rmidx), tfd, level, optname,
         ret);
      if (-1 == ret)
        {
          NSSOC_LOGERR ("return fail]mudle=%d,fd=%d [return]", fdInf->rmidx,
                        tfd);
        }
      UNLOCK_COMMON (fd, local_lock);
      return ret;
    }

  if (NULL != g_nstack_modules.defMod)
    {
      tfd = nstack_get_protoFd (fdInf, nstack_defMod_inx ());
      if (tfd >= 0)
        {
          ops = nstack_def_ops ();
          NSTACK_CAL_FUN (ops, getsockopt,
                          (tfd, level, optname, optval, optlen), ret);
          NSSOC_LOGINF
            ("fd=%d,module=%s:%d,level=%d,optname=%d,ret=%d [return]", fd,
             nstack_defmod_name (), tfd, level, optname, ret);
          if (-1 == ret)
            {
              NSSOC_LOGERR ("return fail]mudle=%d,fd=%d [return]",
                            nstack_defMod_inx (), tfd);
            }
          UNLOCK_COMMON (fd, local_lock);
          return ret;
        }
    }

  nstack_set_errno (ENOTSOCK);
  NSSOC_LOGINF ("fd=%d,ret=%d [Return]", fd, ret);
  UNLOCK_COMMON (fd, local_lock);
  return ret;
}

/* all rlfd need setsockopt, set opt failed still can Estblsh connection. so we not care suc/fail */
/* Currently, if setsockopt is successfull either in kernel or stack-x, the below API returns SUCCESS */
int
nstack_setsockopt (int fd, int level, int optname, const void *optval,
                   socklen_t optlen)
{
  nstack_fd_Inf *fdInf;
  int ret = -1;
  nstack_socket_ops *ops;
  int itfd;
  int modInx = 0;
  int curRet = -1;
  int lerror = 0;
  int flag = 0;

  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGINF
    ("(fd=%d, level=%d, optname=%d, optval=%p, optlen=%u) [Caller]", fd,
     level, optname, optval, optlen);

  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("invalid input]fd=%d [return]", fd);
      return -1;
    }

  NSTACK_FD_LINUX_CHECK (fd, setsockopt, fdInf,
                         (fd, level, optname, optval, optlen));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_COMMON (fd, fdInf, local_lock);

  if ((NSTACK_SOCKOPT == level) && NSTACK_IS_FD_EPOLL_SOCKET (fdInf))
    {
      ret = nstack_option_set (fdInf, optname, optval, optlen);
      UNLOCK_COMMON (fd, local_lock);
      return ret;
    }

  if (fdInf->ops)
    {
      itfd = fdInf->rlfd;
      NSTACK_CAL_FUN (fdInf->ops, setsockopt,
                      (itfd, level, optname, optval, optlen), ret);
      NSSOC_LOGINF
        ("fd=%d,module=%s,tfd=%d,level=%d,optname=%d,ret=%d [return]", fd,
         nstack_get_module_name_by_idx (fdInf->rmidx), itfd, level, optname,
         ret);
      if (-1 == ret)
        {
          NSSOC_LOGERR ("return fail]mudle=%d,fd=%d [return]", fdInf->rmidx,
                        itfd);
        }
      UNLOCK_COMMON (fd, local_lock);
      return ret;
    }
  nstack_each_modOps (modInx, ops)
  {
    itfd = nstack_get_protoFd (fdInf, modInx);
    if (-1 == itfd)
      {
        continue;
      }
    flag = 1;
    NSTACK_CAL_FUN (ops, setsockopt, (itfd, level, optname, optval, optlen),
                    curRet);
    NSSOC_LOGDBG ("fd=%d,module=%s,tfd=%d,level=%d,optname=%d,ret=%d", fd,
                  nstack_get_module_name_by_idx (modInx), itfd, level,
                  optname, curRet);
    if (modInx == nstack_get_fix_mid ())
      {
        ret = curRet;
        /* errno is thread safe, but stack-x is not, so save it first */
        lerror = errno;
      }
  }
  /* errno is thread safe, but stack-x is not, so save it first begin */
  /*if all fd of stack is -1, the input fd maybe invalid */
  if (0 == flag)
    {
      nstack_set_errno (EBADF);
    }
  /*if linux return fail, and error is none zero, just reset it again */
  if ((lerror != 0) && (ns_success != ret))
    {
      nstack_set_errno (lerror);
    }
  /* errno is thread safe, but stack-x is not, so save it first end */
  NSSOC_LOGINF ("fd=%d,ret=%d [Return]", fd, ret);
  UNLOCK_COMMON (fd, local_lock);
  return ret;
}

int
nstack_ioctl (int fd, unsigned long request, unsigned long argp)
{
  nstack_fd_Inf *fdInf;
  int ret = -1;
  nstack_socket_ops *ops;
  int tfd;
  int modInx = 0;
  int curRet = -1;
  int lerror = 0;
  int flag = 0;

  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGINF ("(fd=%d, request=%lu) [Caller]", fd, request);
  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("invalid input]fd=%d [return]", fd);
      return -1;
    }

  NSTACK_FD_LINUX_CHECK (fd, ioctl, fdInf, (fd, request, argp));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;

  LOCK_COMMON (fd, fdInf, local_lock);
  if (fdInf->ops)
    {
      tfd = fdInf->rlfd;
      NSTACK_CAL_FUN (fdInf->ops, ioctl, (tfd, request, argp), ret);
      NSSOC_LOGINF ("fd=%d,module=%s,rlfd=%d,argp=0x%x,ret=%d [return]",
                    fd, nstack_get_module_name_by_idx (fdInf->rmidx),
                    fdInf->rlfd, argp, ret);
      if (-1 == ret)
        {
          NSSOC_LOGERR ("return fail]mudle=%d,fd=%d [return]", fdInf->rmidx,
                        tfd);
        }

      UNLOCK_COMMON (fd, local_lock);
      return ret;
    }

  nstack_each_modOps (modInx, ops)
  {
    tfd = nstack_get_protoFd (fdInf, modInx);
    if (-1 == tfd)
      {
        continue;
      }
    flag = 1;

    NSTACK_CAL_FUN (ops, ioctl, (tfd, request, argp), curRet);
    NSSOC_LOGINF ("fd=%d,module=%s,tfd=%d,argp=0x%x,ret=%d ",
                  fd, nstack_get_module_name_by_idx (modInx), tfd, argp,
                  curRet);
    if (modInx == nstack_get_fix_mid ())
      {
        ret = curRet;
        /* errno is thread safe, but stack-x is not, so save it first */
        lerror = errno;
      }
  }
  /* errno is thread safe, but stack-x is not, so save it first */
  if (0 == flag)
    {
      nstack_set_errno (EBADF);
    }
  if ((0 != lerror) && (ns_success != ret))
    {
      nstack_set_errno (lerror);
    }

  NSSOC_LOGINF ("fd=%d,ret=%d [return]", fd, ret);

  UNLOCK_COMMON (fd, local_lock);
  return ret;
}

int
nstack_fcntl (int fd, int cmd, unsigned long argp)
{
  nstack_fd_Inf *fdInf;
  nstack_socket_ops *ops = NULL;
  int ret = -1;
  int noProOpt = 0;
  int tfd;
  int modInx = 0;
  int curRet = -1;
  int lerror = 0;
  int flag = 0;

  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGINF ("(fd=%d, cmd=%d) [Caller]", fd, cmd);
  if (fd < 0)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGERR ("invalid input]fd=%d [return]", fd);
      return -1;
    }

  NSTACK_FD_LINUX_CHECK (fd, fcntl, fdInf, (fd, cmd, argp));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_COMMON (fd, fdInf, local_lock);

  /*have already bind */
  if (fdInf->ops)
    {
      tfd = fdInf->rlfd;
      NSTACK_CAL_FUN (fdInf->ops, fcntl, (tfd, cmd, argp), ret);
      NSSOC_LOGINF ("fd=%d,cmd=%d,mod=%s,tfd=%d,argp=0x%x,ret=%d",
                    fd, cmd, nstack_get_module_name_by_idx (fdInf->rmidx),
                    tfd, argp, ret);
      if (-1 == ret)
        {
          NSSOC_LOGERR ("return fail]mudle=%d,fd=%d", fdInf->rmidx, tfd);
        }
    }
  else
    {
      /*set cmd call all module, and return just linux */
      if (F_SETFL == cmd)
        {
          nstack_each_modOps (modInx, ops)
          {
            tfd = nstack_get_protoFd (fdInf, modInx);
            if (-1 == tfd)
              {
                continue;
              }
            flag = 1;
            noProOpt = 0;
            NSTACK_CAL_FUN (ops, fcntl, (tfd, cmd, argp), curRet);
            NSSOC_LOGINF ("fd=%d,module=%s,tfd=%d,argp=0x%x,ret=%d ",
                          fd, nstack_get_module_name_by_idx (modInx), tfd,
                          argp, curRet);
            if (modInx == nstack_get_fix_mid ())
              {
                ret = curRet;
                lerror = errno;
              }
          }
          /* errno is thread safe, but stack-x is not, so save it first  */
          if (0 == flag)
            {
              nstack_set_errno (EBADF);
            }
          if ((0 != lerror) && (ns_success != ret))
            {
              nstack_set_errno (lerror);
            }
        }                       /*other cmd call default */
      else if (g_nstack_modules.defMod)
        {
          tfd = nstack_get_protoFd (fdInf, g_nstack_modules.defMod->modInx);
          if (tfd >= 0)
            {
              ops = nstack_def_ops ();
              NSTACK_CAL_FUN (ops, fcntl, (tfd, cmd, argp), ret);
              NSSOC_LOGINF ("fd=%d,cmd=%d,mod=%s,tfd=%d,argp=0x%x,ret=%d",
                            fd, cmd, g_nstack_modules.defMod->modulename, tfd,
                            argp, ret);
              if (-1 == ret)
                {
                  NSSOC_LOGERR ("return fail]mudle=%d,fd=%d",
                                g_nstack_modules.defMod->modInx, tfd);
                }
            }
          else
            {
              noProOpt = 1;
            }
        }
      else
        {
          noProOpt = 1;
        }
    }

  if (noProOpt)
    {
      nstack_set_errno (EBADF);
      NSSOC_LOGINF ("fd=%d,ret=%d", fd, ret);
    }

  NSSOC_LOGINF ("fd=%d,cmd=%d,ret=%d [return]", fd, cmd, ret);
  UNLOCK_COMMON (fd, local_lock);
  return ret;
}

/*****************************************************************************
*   Prototype    : nstack_select
*   Description  : nstack_select
*   Input        : int nfds
*                  fd_set *readfds
*                  fd_set *writefds
*                  fd_set *exceptfds
*                  struct timeval *timeout
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nstack_select (int nfds, fd_set * readfds, fd_set * writefds,
               fd_set * exceptfds, struct timeval *timeout)
{
  int ret = -1;

  struct select_entry *entry = NULL;
  struct select_module_info *select_module = get_select_module ();
  u64_t msec;

  int i;

  nstack_set_tracing_contex (0, 0, -1);
  if ((nfds > __FD_SETSIZE) || (nfds < 0)
      || ((timeout) && ((timeout->tv_sec < 0) || (timeout->tv_usec < 0))))
    {
      NSSOC_LOGERR ("paremeter of nfds or timeout are no correct.]nfds = %d \
                                sec = %ld usec = %ld", nfds, timeout->tv_sec, timeout->tv_usec);
      errno = EINVAL;
      return -1;
    }

  for (i = 0; i < nfds; i++)
    {
      if ((readfds) && (FD_ISSET (i, readfds)))
        {
          NSSOC_LOGDBG ("input readfd set %d", i);
        }

      if ((writefds) && (FD_ISSET (i, writefds)))
        {
          NSSOC_LOGDBG ("input writefd set %d", i);
        }
      if ((exceptfds) && (FD_ISSET (i, exceptfds)))
        {
          NSSOC_LOGDBG ("input exceptfds set %d", i);
        }
    }

  /*check the module had regist or not */
  if (TRUE != NSTACK_SELECT_LINUX_CHECK ())
    {
      return nsfw_base_select (nfds, readfds, writefds, exceptfds, timeout);
    }

  /*nstack select not support timer function and not check nfds so calling dufault select */
  if ((nfds <= 0)
      || ((NULL == readfds) && (NULL == writefds) && (NULL == exceptfds)))
    {
      if ((select_module) && (select_module->default_fun))
        {
          return select_module->default_fun (nfds, readfds, writefds,
                                             exceptfds, timeout);
        }
      else
        {
          return nsfw_base_select (nfds, readfds, writefds, exceptfds,
                                   timeout);
        }
    }

  entry = (struct select_entry *) select_alloc (sizeof (struct select_entry));
  if (NULL == entry)
    {
      errno = ENOMEM;
      NSSOC_LOGERR ("select entry alloc failed");
      goto err_return;
    }

  /* fix dead-code type Codedex issue */
  /*split select fd to each modules fd and save to entry */
  (void) select_cb_split_by_mod (nfds, readfds, writefds, exceptfds, entry);

  /*if all fd in default module we just calling it */
  if (entry->info.set_num <= 1)
    {

      /*adapte linux */
      if ((select_module)
          && (entry->info.index == select_module->default_mod))
        {
          if (select_module->default_fun)
            {
              ret =
                select_module->default_fun (nfds, readfds, writefds,
                                            exceptfds, timeout);
            }
          else
            {
              ret =
                nsfw_base_select (nfds, readfds, writefds, exceptfds,
                                  timeout);
            }
          goto err_return;
        }
    }

  /*cheching if event ready or not */
  if (FALSE == select_scan (entry))
    {
      NSSOC_LOGERR ("select scan failed");
      goto err_return;
    }

  if (entry->ready.readyset != 0)
    {
      goto scan_return;
    }

  if ((timeout) && (timeout->tv_sec == 0) && (timeout->tv_usec == 0))
    {
      goto scan_return;
    }

  if (FALSE == select_add_cb (entry))
    {
      errno = ENOMEM;
      NSSOC_LOGERR ("select entry add failed");
      goto err_return;
    }

  if (NULL == timeout)
    {
      select_sem_wait (&entry->sem);
    }
  else
    {
      u64_t time_cost;
      if (nstack_timeval2msec (timeout, &msec))
        {
          nstack_set_errno (EINVAL);
          goto err_return;
        }
      time_cost = nstack_sem_timedwait (&entry->sem, msec);
      if (time_cost >= msec)
        {
          timeout->tv_sec = 0;
          timeout->tv_usec = 0;
        }
      else if (time_cost > 0)
        {
          msec = msec - time_cost;
          timeout->tv_sec = msec / 1000;
          timeout->tv_usec = (msec % 1000) * 1000;
        }
    }

  select_rm_cb (entry);

scan_return:
  if (readfds)
    {
      *readfds = entry->ready.readset;
    }
  if (writefds)
    {
      *writefds = entry->ready.writeset;
    }
  if (exceptfds)
    {
      *exceptfds = entry->ready.exceptset;
    }

  ret = entry->ready.readyset;
  if (ret < 0)
    {
      errno = entry->ready.select_errno;
    }

err_return:
  if (entry)
    {
      select_free ((char *) entry);
    }
  NSSOC_LOGDBG
    ("(nfds=%d,readfds=%p,writefds=%p,exceptfds=%p,timeout=%p),ret=%d errno = %d",
     nfds, readfds, writefds, exceptfds, timeout, ret, errno);
  return ret;
}

/* epfd?fd maybe is from kernel or stack-x,should take care */
int
nstack_epoll_ctl (int epfd, int op, int fd, struct epoll_event *event)
{
  int ret = ns_fail;
  struct eventpoll *ep = NULL;
  nstack_fd_Inf *epInf;
  struct epoll_event ep_event = { 0 };
  struct epitem *epi = NULL;

  nstack_set_tracing_contex (0, 0, fd);
  NSSOC_LOGINF ("(epfd=%d, op=%d, fd=%d, event=%p) [Caller]", epfd, op, fd,
                event);
  if (event)
    NSSOC_LOGINF ("event->data.fd=%d,event->events=%u", event->data.fd,
                  event->events);

  NSTACK_FD_LINUX_CHECK (epfd, epoll_ctl, epInf, (epfd, op, fd, event));

  nstack_fd_local_lock_info_t *epoll_local_lock = &epInf->local_lock;
  LOCK_EPOLL (epfd, epInf, epoll_local_lock);
  nstack_fd_local_lock_info_t *local_lock = get_fd_local_lock_info (fd);
  LOCK_EPOLL_CTRL (fd, local_lock, epfd, epoll_local_lock);

  if (!NSTACK_IS_FD_EPOLL_SOCKET (epInf) || fd == epfd) /* `man epoll_ctl` tells me to do this check :) */
    {
      NSSOC_LOGWAR ("epfd=%d is not a epoll fd [return]", epfd);
      errno = EINVAL;
      goto err_return;
    }

  if (!nstack_is_nstack_sk (fd))
    {
      NSSOC_LOGWAR ("epfd=%d ,fd %d is not a supported [return]", epfd, fd);
      errno = EBADF;
      goto err_return;
    }

  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (epfd);
  if (NULL == epInfo)
    {
      NSSOC_LOGWAR ("epInfo of epfd=%d is NULL [return]", epfd);
      errno = EINVAL;
      goto err_return;
    }

  ep = ADDR_SHTOL (epInfo->ep);
  if (NULL == ep)
    {
      NSSOC_LOGWAR ("ep of epfd=%d is NULL [return]", epfd);
      errno = EINVAL;
      goto err_return;
    }

  if (NULL != event)
    {
      ep_event.data = event->data;
      ep_event.events = event->events;
    }
  else
    {
      if (op != EPOLL_CTL_DEL)
        {
          NSSOC_LOGWAR ("events epfd=%d is NULL [return]", epfd);
          errno = EFAULT;
          goto err_return;
        }
    }

  sys_arch_lock_with_pid (&ep->sem);

  epi = nsep_find_ep (ep, fd);
  switch (op)
    {
    case EPOLL_CTL_ADD:
      if (!epi)
        {
          ep_event.events |= (EPOLLERR | EPOLLHUP);     // Check `man epoll_ctl` if you don't understand , smile :)
          common_mem_rwlock_read_lock (get_fork_lock ());       /* to ensure that there is no fd to create and close when fork. added by tongshaojun t00391048 */
          ret = nsep_epctl_add (ep, fd, &ep_event);
          common_mem_rwlock_read_unlock (get_fork_lock ());
        }
      else
        {
          NSSOC_LOGWAR ("fd already in eventpoll");
          errno = EEXIST;
          ret = -1;
        }
      break;
    case EPOLL_CTL_DEL:
      if (epi)
        {
          common_mem_rwlock_read_lock (get_fork_lock ());
          ret = nsep_epctl_del (ep, epi);
          common_mem_rwlock_read_unlock (get_fork_lock ());
        }
      else
        {
          NSSOC_LOGWAR ("fd not registed before");
          errno = ENOENT;
          ret = -1;
        }
      break;
    case EPOLL_CTL_MOD:
      if (epi)
        {
          ep_event.events |= (EPOLLERR | EPOLLHUP);     // Look up ?
          ret = nsep_epctl_mod (ep, nsep_get_infoBySock (fd), epi, &ep_event);
        }
      else
        {
          NSSOC_LOGWAR ("fd not registed before");
          errno = ENOENT;
          ret = -1;
        }
      break;
    default:
      NSSOC_LOGERR ("epfd=%d,fd=%d,opt=%d not supported", epfd, fd, op);
      errno = EINVAL;
      ret = -1;
    }

  sys_sem_s_signal (&ep->sem);
  NSSOC_LOGINF ("epfd=%d,op=%d,fd=%d,ret=%d [return]", epfd, op, fd, ret);

err_return:
  UNLOCK_EPOLL_CTRL (fd, local_lock);
  UNLOCK_EPOLL (epfd, epoll_local_lock);
  return ret;
}

int
nstack_epoll_create (int size)
{
  nstack_fd_Inf *fdInf = NULL;
  struct eventpoll *ep = NULL;
  int epfd = -1;
  int modInx = 0;
  int tfd = 0;
  nstack_socket_ops *ops;
  int ret = 0;

  nstack_set_tracing_contex (0, 0, -1);

  NSTACK_INIT_CHECK_RET (epoll_create);

  NSSOC_LOGINF ("(size=%d) [Caller]", size);

  if (size <= 0)
    {
      nstack_set_errno (EINVAL);
      NSSOC_LOGERR ("invalid input,param]size=%d [return]", size);
      return -1;
    }

  /*create a epfd */
  if (nstack_fix_mid_ops ()->pfepoll_create)
    {
      epfd = nstack_fix_mid_ops ()->pfepoll_create (size);
    }
  else
    {
      /*if not surport epfd create, just alloc a fd */
      if (nstack_extern_deal (nstack_get_fix_mid ()).stack_alloc_fd)
        {
          epfd = nstack_extern_deal (nstack_get_fix_mid ()).stack_alloc_fd ();
        }
      else
        {
          nstack_set_errno (ENOSYS);
          NSSOC_LOGERR ("not surport epoll create]size=%d [return]", size);
          return -1;
        }
    }
  if (!nstack_is_nstack_sk (epfd))
    {
      if (epfd >= 0)
        {
          NSTACK_CAL_FUN (nstack_fix_mid_ops (), close, (epfd), ret);
        }
      NSSOC_LOGERR ("kernel fd alloced is too larger]kernel_fd=%d [return]",
                    epfd);
      nstack_set_errno (EMFILE);
      return -1;
    }

  nstack_fd_local_lock_info_t *lock_info = get_fd_local_lock_info (epfd);
  LOCK_FOR_EP (lock_info);
  fdInf = nstack_lk_fd_alloc_with_kernel (epfd);
  if (NULL == fdInf)
    {
      NSSOC_LOGERR ("create fdInf fail [return]");
      nstack_set_errno (ENOMEM);
      NSTACK_CAL_FUN (nstack_fix_mid_ops (), close, (epfd), ret);
      UNLOCK_FOR_EP (lock_info);
      return -1;
    }

  if (nsep_alloc_eventpoll (&ep))
    {
      nsep_free_infoWithSock (epfd);
      NSSOC_LOGERR ("Alloc eventpoll fail [return]");
      nstack_fd_free_with_kernel (fdInf);
      nstack_set_errno (ENOMEM);
      UNLOCK_FOR_EP (lock_info);
      return -1;
    }

  ep->epfd = epfd;
  nsep_set_infoEp (epfd, ep);
  NSTACK_SET_FD_EPOLL_SOCKET (fdInf);

  /*if stack supply ep_create interface, just call create */
  nstack_each_modOps (modInx, ops)
  {

    if (modInx == nstack_get_fix_mid ())
      {
        nstack_set_protoFd (fdInf, modInx, epfd);
        continue;
      }
    if (ops->pfepoll_create)
      {
        tfd = ops->pfepoll_create (size);
        nstack_set_protoFd (fdInf, modInx, tfd);
      }
  }

  NSSOC_LOGINF ("fd=%d [return]", epfd);
  set_fd_status_lock_fork (epfd, FD_OPEN);
  UNLOCK_FOR_EP (lock_info);
  return epfd;
}

void
nstack_epoll_prewait_proc (int epfd, int *eventflag, int num)
{
  int modInx = 0;
  nsep_epollInfo_t *epInfo = NULL;
  epInfo = nsep_get_infoBySock (epfd);
  if (!epInfo)
    {
      return;
    }
  nstack_each_modInx (modInx)
  {
    if (nstack_extern_deal (modInx).ep_prewait_proc)
      {
        if (epInfo->protoFD[modInx] < 0)
          {
            continue;
          }
        /*if already report event, maybe next time to do this proc */
        if ((eventflag) && (modInx < num) && (eventflag[modInx] != 0))
          {
            continue;
          }
        nstack_extern_deal (modInx).ep_prewait_proc (epInfo->protoFD[modInx]);
      }
  }
  return;
}

int
nstack_epoll_wait (int epfd, struct epoll_event *events, int maxevents,
                   int timeout)
{
  nstack_fd_Inf *fdInf = NULL;
  nsep_epollInfo_t *epInfo = NULL;
  struct eventpoll *ep = NULL;
  int eventflag[NSEP_SMOD_MAX];
  int evt = 0;
  int ret = 0;

  nstack_set_tracing_contex (0, 0, epfd);
  NSTACK_FD_LINUX_CHECK (epfd, epoll_wait, fdInf,
                         (epfd, events, maxevents, timeout));

  nstack_fd_local_lock_info_t *local_lock = &fdInf->local_lock;
  LOCK_EPOLL (epfd, fdInf, local_lock);

  if (!NSTACK_IS_FD_EPOLL_SOCKET (fdInf))
    {
      NSSOC_LOGWAR ("epfd=%d is not a epoll fd, return -1", epfd);
      errno = EINVAL;
      UNLOCK_EPOLL (epfd, local_lock);
      return -1;
    }

  /* check input paramter's validity */
  if (NULL == events)
    {
      NSSOC_LOGWAR ("events is NULL, return -1 ");
      errno = EINVAL;
      UNLOCK_EPOLL (epfd, local_lock);
      return -1;
    }

  epInfo = nsep_get_infoBySock (epfd);
  if (NULL == epInfo)
    {
      NSSOC_LOGWAR ("epInfo is NULL]epinfo=%p,epfd=%d", epInfo, epfd);
      errno = EINVAL;
      UNLOCK_EPOLL (epfd, local_lock);
      return -1;
    }

  ep = ADDR_SHTOL (epInfo->ep);
  if (NULL == ep)
    {
      NSSOC_LOGWAR ("fdInf->ep is NULL, return -1,epinfo=%p,epfd=%d", epInfo,
                    epfd);
      errno = EINVAL;
      UNLOCK_EPOLL (epfd, local_lock);
      return -1;
    }

  if (maxevents <= 0)
    {
      NSSOC_LOGWAR ("maxevent less than zero maxevents=%d, return -1 ",
                    maxevents);
      errno = EINVAL;
      UNLOCK_EPOLL (epfd, local_lock);
      return -1;
    }

    /**
     *  For linux epoll
     *  Case 1: event is already there, means ks_ep_thread already reported, and epfd not in epoll.
     *  Case 2: event is not there, so we need to add epfd to ks_ep_thread
     *  Update : only try to add epfd to ks_ep_thread once, avoid ks_ep_thread reporting the same event again after app has got the event!
     */
  MEMSET_S (&eventflag[0], sizeof (eventflag), 0, sizeof (eventflag));
  evt = nsep_ep_poll (ep, events, maxevents, &eventflag[0], NSEP_SMOD_MAX);
  if (evt)
    {
      NSSOC_LOGDBG ("Got event]epfd=%d,maxevents=%d,ret=%d", epfd, maxevents,
                    evt);
      nstack_epoll_prewait_proc (epfd, &eventflag[0], NSEP_SMOD_MAX);
      goto out;
    }
  nstack_epoll_prewait_proc (epfd, NULL, 0);
  do
    {
      if (0 == timeout)
        {
          goto out;
        }
      else if (timeout < 0)
        {
          ret = sem_wait (&ep->waitSem);
          /* when sem_wait return -1, epoll_wait should also return sem_wait's return value and errno */
          if (ret < 0)
            {
              /* Change sem_wait EINTR log to WAR level */
              if (errno == EINTR)
                {
                  NSSOC_LOGWAR ("sem_wait return -1]errno=%d", errno);
                }
              else
                {
                  NSSOC_LOGERR ("sem_wait return -1]errno=%d", errno);
                }

              evt = ret;
              goto out;
            }
        }
      else
        {
          ret =
            nstack_epoll_sem_timedwait (&ep->waitSem, (u64_t) timeout,
                                        epInfo->sleepTime);
        }

      if (ret)
        {
          goto out;
        }

      evt = nsep_ep_poll (ep, events, maxevents, NULL, 0);
      if (evt)
        {
          NSSOC_LOGDBG ("Got event]epfd=%d,maxevents=%d,ret=%d", epfd,
                        maxevents, evt);
          goto out;
        }
    }
  while (1);

out:
  UNLOCK_EPOLL (epfd, local_lock);
  return evt;
}

pid_t
nstack_fork (void)
{
  pid_t pid;
  pid_t parent_pid = sys_get_hostpid_from_file (getpid ());

  nstack_set_tracing_contex (0, 0, -1);
  common_mem_rwlock_write_lock (get_fork_lock ());
  if (NSTACK_MODULE_SUCCESS == g_nStackInfo.fwInited)
    {
      nstack_fork_init_parent (parent_pid);
      dmm_spinlock_lock_with_pid (nstack_get_fork_share_lock (), parent_pid);
      pid = nsfw_base_fork ();
      if (pid == 0)
        {
          /* when fork, the child process need relese the lock in glog */
          nstack_log_lock_release ();
          nstack_fork_init_child (parent_pid);
          (void) nstack_for_epoll_init ();
          dmm_spinlock_lock_with_pid (nstack_get_fork_share_lock (),
                                      get_sys_pid ());
          nsep_fork_child_proc (parent_pid);
          common_mem_spinlock_unlock (nstack_get_fork_share_lock ());
        }
      else if (pid > 0)
        {
          pid_t child_pid = get_hostpid_from_file (pid);
          nsep_fork_parent_proc (parent_pid, child_pid);
          common_mem_spinlock_unlock (nstack_get_fork_share_lock ());
          sys_sleep_ns (0, 10000000);   /* wait child add pid for netconn */
        }
      else
        {
          NSSOC_LOGERR ("fork failed]parent_pid=%d", parent_pid);
          common_mem_spinlock_unlock (nstack_get_fork_share_lock ());
        }
    }
  else
    {
      pid = nsfw_base_fork ();
      if (pid == 0)
        {
          updata_sys_pid ();
        }
      NSSOC_LOGERR ("g_nStackInfo has not initialized]parent_pid=%d, pid=%d",
                    parent_pid, pid);
    }

  common_mem_rwlock_write_unlock (get_fork_lock ());
  return pid;
}
