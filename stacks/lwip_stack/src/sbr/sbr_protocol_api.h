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

#ifndef SBR_PROTOCOL_API_H
#define SBR_PROTOCOL_API_H
#include <sys/uio.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include "sbr_err.h"
#include "nsfw_msg_api.h"
#include "nsfw_mt_config.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#ifndef SBR_MAX_INTEGER
#define SBR_MAX_INTEGER 0x7FFFFFFF
#endif

#ifndef socklen_t
#define socklen_t u32
#endif

#define SBR_MAX_FD_NUM MAX_SOCKET_NUM

typedef struct sbr_socket_s sbr_socket_t;
typedef struct
{
  int (*socket) (sbr_socket_t *, int, int, int);
  int (*bind) (sbr_socket_t *, const struct sockaddr *, socklen_t);
  int (*listen) (sbr_socket_t *, int);
  int (*accept) (sbr_socket_t *, sbr_socket_t *, struct sockaddr *,
                 socklen_t *);
  int (*accept4) (sbr_socket_t *, sbr_socket_t *, struct sockaddr *,
                  socklen_t *, int);
  int (*connect) (sbr_socket_t *, const struct sockaddr *, socklen_t);
  int (*shutdown) (sbr_socket_t *, int);
  int (*getsockname) (sbr_socket_t *, struct sockaddr *, socklen_t *);
  int (*getpeername) (sbr_socket_t *, struct sockaddr *, socklen_t *);
  int (*getsockopt) (sbr_socket_t *, int, int, void *, socklen_t *);
  int (*setsockopt) (sbr_socket_t *, int, int, const void *, socklen_t);
  int (*recvfrom) (sbr_socket_t *, void *, size_t, int, struct sockaddr *,
                   socklen_t *);
  int (*readv) (sbr_socket_t *, const struct iovec *, int);
  int (*recvmsg) (sbr_socket_t *, struct msghdr *, int);
  int (*send) (sbr_socket_t *, const void *, size_t, int);
  int (*sendto) (sbr_socket_t *, const void *, size_t, int,
                 const struct sockaddr *, socklen_t);
  int (*sendmsg) (sbr_socket_t *, const struct msghdr *, int);
  int (*writev) (sbr_socket_t *, const struct iovec *, int);
  int (*fcntl) (sbr_socket_t *, int, long);
  int (*ioctl) (sbr_socket_t *, unsigned long, void *);
  int (*close) (sbr_socket_t *);
  int (*peak) (sbr_socket_t *);
  void (*lock_common) (sbr_socket_t *);
  void (*unlock_common) (sbr_socket_t *);
  void (*fork_parent) (sbr_socket_t *, pid_t);
  void (*fork_child) (sbr_socket_t *, pid_t, pid_t);
  unsigned int (*ep_ctl) (sbr_socket_t *, int triggle_ops,
                          struct epoll_event * event, void *pdata);
  unsigned int (*ep_getevt) (sbr_socket_t *, unsigned int events);
  void (*set_app_info) (sbr_socket_t *, void *appinfo);
  void (*set_close_stat) (sbr_socket_t *, int flag);
} sbr_fdopt;

struct sbr_socket_s
{
  int fd;
  sbr_fdopt *fdopt;
  void *stack_obj;
  void *sk_obj;
};

int sbr_init_protocol ();
int sbr_fork_protocol ();
sbr_fdopt *sbr_get_fdopt (int domain, int type, int protocol);
void sbr_app_touch_in (void);   /*app send its version info to nStackMain */
int lwip_try_select (int fdsize, fd_set * fdread, fd_set * fdwrite,
                     fd_set * fderr, struct timeval *timeout);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
