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

#ifndef __NSTACK_FD_MNG_H__
#define __NSTACK_FD_MNG_H__

#include <semaphore.h>
#include "nstack_atomic.h"

#include "types.h"
#include "nstack_module.h"
#include "nstack_types.h"
#include "nstack_eventpoll.h"
#include "common_mem_spinlock.h"
#include "common_mem_api.h"
#include "common_func.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define NSTACK_FDT_BLOCK_NOFFSET       10
#define NSTACK_FDT_CONVERT_FLAG        0x40000000       /* flag set means nstack socket */

#define NSTACK_MAX_SOCK_NUM  8192

#define NSTACK_MAX_PID 65536    /* release sockets when app exit */

#ifndef AF_INET
#define AF_INET  2
#endif

#ifndef NSTACK_SOCKOPT_CHECK
#define NSTACK_SOCKOPT_CHECK
/* setsockopt level type*/
enum
{
  NSTACK_SOCKOPT = 0xff02
};
/*setsockopt optname type*/
enum
{
  NSTACK_SEM_SLEEP = 0X001
};
#endif
#define nstack_is_nstack_sk(fd)     ((fd) >= 0 && ((fd) < (int)NSTACK_KERNEL_FD_MAX))

typedef enum __nstack_fd_Stat
{
  NSTACK_FD_INUSING,
  NSTACK_FD_DISCARD
} nstack_fd_stat;

#define NSTACK_BIND_SUCCESS 0
#define NSTACK_LISTEN_SUCCESS 0
#define NSTACK_LISTEN_FAIL  1
#define NSTACK_BIND_FAIL   1
#define NSTACK_LISTENING    1
#define NSTACK_NO_LISTENING 0

#define NSTACK_FD_INIT    (0)
#define NSTACK_FD_OPEN    (1)
#define NSTACK_FD_CLOSE   (2)

#define NSTACK_FD_NOBIND    (0)
#define NSTACK_FD_BIND      (1)

/* release sockets when app exit */

typedef struct
{
  ns_int32 fd;
  ns_int32 errCode;
  union
  {
    struct result
    {
      ns_int32 brslt:8;
      ns_int32 lrslt:8;
      ns_int32 resrv:16;
    } rslt;
    ns_int32 pad;
  };
  ns_int32 liststate;
} __attribute__ ((__packed__)) nstack_protoFD_t;

#define NSTACK_FD_ATTR_NONBLOCKING 0x00000001
#define NSTACK_FD_ATTR_EPOLL_SOCKET 0x00000002
#define NSTACK_FD_ATTR_LISTEN_SOCKET 0x00000004

#define NSTACK_IS_FD_NONBLOCKING(inf) ((inf)->attr & NSTACK_FD_ATTR_NONBLOCKING)
#define NSTACK_SET_FD_NONBLOCKING(inf) ((inf)->attr |= NSTACK_FD_ATTR_NONBLOCKING)
#define NSTACK_SET_FD_BLOCKING(inf) ((inf)->attr &= (~NSTACK_FD_ATTR_NONBLOCKING))

#define NSTACK_IS_FD_EPOLL_SOCKET(inf) ((inf)->attr & NSTACK_FD_ATTR_EPOLL_SOCKET)
#define NSTACK_SET_FD_EPOLL_SOCKET(inf) ((inf)->attr |= NSTACK_FD_ATTR_EPOLL_SOCKET)
#define NSTACK_IS_FD_LISTEN_SOCKET(inf) ((inf)->attr & NSTACK_FD_ATTR_LISTEN_SOCKET)
#define NSTACK_SET_FD_LISTEN_SOCKET(inf) ((inf)->attr |= NSTACK_FD_ATTR_LISTEN_SOCKET)

typedef struct
{
  atomic_t fd_ref;
  common_mem_spinlock_t close_lock;
  volatile int fd_status;
} nstack_fd_local_lock_info_t;
/*
fd_ref:the number of times the fd is used, when it is 0, should release fd resource
close_lock:lock for close and epoll
fd_status:when created, it is FD_OPEN;after close, it is FD_CLOSING;after release_fd,
it is FD_CLOSE.
- Begin */
typedef enum
{
  FD_CLOSE,
  FD_OPEN,
  FD_CLOSING
} FD_STATUS;

typedef enum
{
  NSTACK_STATE_CLOSE,
  NSTACK_STATE_OPEN,
  NSTACK_STATE_MAX
} nstack_fdstate;
typedef struct __nstack_fd_Inf
{
  ns_int32 rlfd;                /*the protocl stack returned fd */
  ns_int32 rmidx;
  ns_int32 nxtfd;
  nstack_socket_ops *ops;       /*opers of the fd, for save space we user opIdx here */
  ns_int32 type;                /*the fd type like SOCK_STREAM|SOCK_NONBLOCK ... */
  ns_int32 stat;
  ns_int32 fd;
  ns_uint32 attr;               /* attribute like non-blocking, listen socket , epoll socket.... */

  nstack_protoFD_t protoFD[NSTACK_MAX_MODULE_NUM];      // where is protocol fd stores, index is module type
  nstack_fd_local_lock_info_t local_lock;
  /* if has bound to an addr */
  u8_t isBound;                 /*0:no call bind, 1: call bind */
  char last_reserve[2];         //reserve for update
} nstack_fd_Inf;

#define nstack_set_router_protocol(_fdInf, _proto) \
    (_fdInf)->rmidx = (_proto); \
    nsep_set_infomdix((_fdInf)->fd, (_proto));\
    nssct_set_index((_fdInf)->fd, (_proto));\

#define nstack_set_routed_fd(_fdInf, _protoFD) \
    (_fdInf)->rlfd = (_protoFD); \
    nsep_set_infoRlfd((_fdInf)->fd, (_protoFD));\


nstack_fd_Inf *nstack_fd2inf (int fd);

void nstack_reset_fdInf (nstack_fd_Inf * fdInf);

static inline nstack_fd_Inf *
nstack_getValidInf (int fd)
{
  nstack_fd_Inf *retInf = NULL;
  retInf = nstack_fd2inf (fd);
  if (NULL == retInf || FD_OPEN != retInf->local_lock.fd_status)
    {
      return NULL;
    }
  return retInf;
}

#define nstack_get_protoFd(fdInf, modInx) ((fdInf)->protoFD[modInx].fd)
#define nstack_set_bind_ret(fdInf, modInx, ret) ((fdInf)->protoFD[modInx].rslt.brslt = ret)
#define nstack_set_listen_ret(fdInf, modInx, ret) ((fdInf)->protoFD[modInx].rslt.lrslt = ret)
#define nstack_set_ret(fdInf, modInx, ret) ((fdInf)->protoFD[modInx].pad = ret)
#define nstack_set_listen_state(fdInf, modInx, state) ((fdInf)->protoFD[modInx].liststate = state)
#define nstack_get_listen_state(fdInf, modInx) ((fdInf)->protoFD[modInx].liststate)
#define nstack_get_listen_ret(fdInf, modInx) ((fdInf)->protoFD[modInx].rslt.lrslt)
#define nstack_get_bind_ret(fdInf, modInx) ((fdInf)->protoFD[modInx].rslt.brslt)
#define nstack_get_protoFdSt(fdInf, modInx) (&(fdInf)->protoFD[modInx])
void nstack_set_protoFd (nstack_fd_Inf * fdInf, int modInx, int protofd);
void nstack_set_app_info (nstack_fd_Inf * fdInf, int modInx);

int nstack_fd_free_with_kernel (nstack_fd_Inf * fdInf);
int nstack_fd_free_without_kernel (nstack_fd_Inf * fdInf, int ref);
extern nstack_fd_Inf *nstack_lk_fd_alloc_with_kernel (int nfd); //alloc a nstack socket that include kernel fd
extern void nstack_fd_free (nstack_fd_Inf * fdInf);

void nstack_fork_init_child (pid_t ppid);
void nstack_fork_init_parent (pid_t ppid);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __NSTACK_FD_MNG_H__ */
