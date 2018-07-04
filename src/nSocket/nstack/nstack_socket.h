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

#ifndef __NSTACK_SOCKET_H__
#define __NSTACK_SOCKET_H__
#include <sys/socket.h>
#include <poll.h>
#include <sys/epoll.h>

#undef NSTACK_MK_DECL
#define NSTACK_MK_DECL(ret, fn, args)   extern ret nstack_##fn args
#include "declare_syscalls.h"

int release_fd (int fd, nstack_fd_local_lock_info_t * local_lock);
int nstack_posix_api_init ();
int nstack_close (int fd);
int nstack_socket (int domain, int itype, int protocol);

#define NSTACK_EPOLL_FD_CHECK_RET_UNLOCK(fdVal, fun, inf, err, local_lock, unlock) \
    if ((NULL == inf) || ((inf)->attr & NSTACK_FD_ATTR_EPOLL_SOCKET)) \
    {  \
        nstack_set_errno(err); \
        NSSOC_LOGERR("nstack [%s] call, fd=0x%x is epoll fd return fail errno=%d [return]", #fun, fdVal, err); \
        unlock(fdVal, local_lock);\
        return -1;\
    }

#define NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_COMMON(fdVal, fun, inf, err, local_lock) \
  NSTACK_EPOLL_FD_CHECK_RET_UNLOCK(fdVal, fun, inf, err, local_lock, UNLOCK_COMMON)

#define LOCK_RECV(fd, fd_inf, local_lock)\
   LOCK_BASE_WITHOUT_KERNEL(fd, fd_inf, local_lock)

#define UNLOCK_RECV(fd, local_lock) \
    UNLOCK_BASE(fd, local_lock)

#define NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_RECV(fdVal, fun, inf, err, local_lock) \
   NSTACK_EPOLL_FD_CHECK_RET_UNLOCK(fdVal, fun, inf, err, local_lock, UNLOCK_RECV)

#define LOCK_SEND(fd, fd_inf, local_lock) \
    INC_FD_REF(fd, fd_inf, local_lock)

static inline void
UNLOCK_SEND (int fd, nstack_fd_local_lock_info_t * local_lock)
{
  if ((NULL != local_lock) && atomic_dec (&local_lock->fd_ref) == 0)
    {
      release_fd (fd, local_lock);
    }
}

#define NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_SEND(fdVal, fun, inf, err, local_lock) \
   NSTACK_EPOLL_FD_CHECK_RET_UNLOCK(fdVal, fun, inf, err, local_lock, UNLOCK_SEND)

#define LOCK_CONNECT(fd, fd_inf, local_lock) \
  LOCK_BASE(fd, fd_inf, local_lock)

#define UNLOCK_CONNECT(fd, local_lock) \
    UNLOCK_BASE(fd, local_lock)

#define NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_CONNECT(fdVal, fun, inf, err, local_lock) \
    NSTACK_EPOLL_FD_CHECK_RET_UNLOCK(fdVal, fun, inf, err, local_lock, UNLOCK_CONNECT)

#define LOCK_ACCEPT(fd, fd_inf, local_lock) \
   LOCK_BASE(fd, fd_inf, local_lock)

#define UNLOCK_ACCEPT(fd, local_lock) \
    UNLOCK_BASE(fd, local_lock)

#define NSTACK_EPOLL_FD_CHECK_RET_UNLOCK_ACCEPT(fdVal, fun, inf, err, local_lock) \
   NSTACK_EPOLL_FD_CHECK_RET_UNLOCK(fdVal, fun, inf, err, local_lock, UNLOCK_ACCEPT)

#define LOCK_EPOLL(fd, fd_inf, local_lock) INC_FD_REF(fd, fd_inf, local_lock)

static inline void
UNLOCK_EPOLL (int fd, nstack_fd_local_lock_info_t * local_lock)
{
  if ((NULL != local_lock) && atomic_dec (&local_lock->fd_ref) == 0)
    {
      release_fd (fd, local_lock);
    }
}

#define LOCK_EPOLL_CTRL(fd_val, local_lock, epoll_fd, epoll_local_lock){\
    if (local_lock)\
    {\
        atomic_inc(&local_lock->fd_ref);\
        common_mem_spinlock_lock(&local_lock->close_lock);\
        nstack_fd_Inf* fd_inf = nstack_getValidInf(fd_val);\
        if (fd_inf)\
        {\
            if (local_lock->fd_status != FD_OPEN)\
            {\
                NSSOC_LOGWAR("fd %d is not open [return]", fd_val);\
                nstack_set_errno(EBADF);\
                common_mem_spinlock_unlock(&local_lock->close_lock);\
                if(atomic_dec(&local_lock->fd_ref)==0){   \
                    release_fd(fd_val, local_lock);\
                }\
                UNLOCK_EPOLL(epoll_fd, epoll_local_lock);\
                return -1;\
            }\
        }\
    }\
}

static inline void
UNLOCK_EPOLL_CTRL (int fd, nstack_fd_local_lock_info_t * local_lock)
{
  if (local_lock)
    {
      common_mem_spinlock_unlock (&local_lock->close_lock);
      if (atomic_dec (&local_lock->fd_ref) == 0)
        {
          release_fd (fd, local_lock);
        }
    }
}

#define INC_FD_REF(fd, fd_inf, local_lock){ \
    if (local_lock)\
    {\
        atomic_inc(&local_lock->fd_ref);\
        if (local_lock->fd_status != FD_OPEN)\
        {\
            nstack_set_errno(EBADF);\
            NSSOC_LOGERR("nstack call, fd_status=%d [return]", local_lock->fd_status); \
            if(atomic_dec(&local_lock->fd_ref) == 0){ \
                release_fd(fd, local_lock);\
            }\
            return -1;\
        }\
    } \
}

#define LOCK_BASE(fd, fd_inf, local_lock){\
    INC_FD_REF(fd, fd_inf, local_lock);\
}

/* if is kernel fd, don't need to lock */
#define LOCK_BASE_WITHOUT_KERNEL(fd, fd_inf, local_lock){\
    INC_FD_REF(fd, fd_inf, local_lock);\
}

static inline void
UNLOCK_BASE (int fd, nstack_fd_local_lock_info_t * local_lock)
{
  if ((NULL != local_lock) && (atomic_dec (&local_lock->fd_ref) == 0))
    {
      release_fd (fd, local_lock);
    }
}

#define LOCK_COMMON(fd, fd_inf, local_lock) \
    LOCK_BASE(fd, fd_inf, local_lock)

#define UNLOCK_COMMON(fd, local_lock) \
    UNLOCK_BASE(fd, local_lock)

#define LOCK_CLOSE(local_lock){\
    if (local_lock)\
    {\
        common_mem_spinlock_lock(&local_lock->close_lock);\
    }\
}

static inline void
UNLOCK_CLOSE (nstack_fd_local_lock_info_t * local_lock)
{
  if (local_lock)
    {
      common_mem_spinlock_unlock (&local_lock->close_lock);
    }
}

#define LOCK_FOR_EP(local_lock) LOCK_CLOSE(local_lock)

#define UNLOCK_FOR_EP(local_lock) UNLOCK_CLOSE(local_lock)

#define NSTACK_INIT_CHECK_RET(fun, args...) \
    do { \
        if (NSTACK_MODULE_INITING == g_nStackInfo.fwInited) { \
            NSSOC_LOGINF ("call kernel func %s", #fun); \
            return nsfw_base_##fun(args); \
            } \
        if (nstack_fw_init()) { \
            NSSOC_LOGERR("nstack %s call, but initial not finished yet [return]", #fun); \
            nstack_set_errno(ENOSYS); \
            return -1; \
        } \
    }while(0)

#define NSTACK_MODULE_ERROR_SET(Index)

#endif /* __NSTACK_SOCKET_H__ */
