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

#ifndef __NSOCKET_DMM_API_H__
#define __NSOCKET_DMM_API_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nstack_rd_data.h"

/*
 *Standard api interface definition.
 *these interface is provided by Protocol stack to nStack
 */
typedef struct __nstack_socket_ops
{
#undef NSTACK_MK_DECL
#define NSTACK_MK_DECL(ret, fn, args)  ret (*pf##fn) args
#include "declare_syscalls.h"
} nstack_socket_ops;

typedef enum
{
  STACK_FD_INVALID_CHECK,       /*check wether fd is created by this stack */
  STACK_FD_FUNCALL_CHECK,       /*check this stack support default call */
} nstack_fd_check;

typedef enum
{
  nstack_ep_triggle_add,
  nstack_ep_triggle_mod,
  nstack_ep_triggle_del
} nstack_ep_triggle_ops_t;

/*
 *Interactive interface for Protocol stack and nStack defined here
 *these interface is provided by Protocol stack to nStack
 */
typedef struct __nstack_extern_ops
{
  int (*module_init) (void);    /*stack module init */
  int (*module_init_child) (void);      /*stack module init for child process */
  int (*fork_init_child) (pid_t p, pid_t c);    /*after fork, stack child process init again if needed. */
  void (*fork_parent_fd) (int s, pid_t p);      /*after fork, stack parent process proc again if needed. */
  void (*fork_child_fd) (int s, pid_t p, pid_t c);      /*after fork, child record pid for recycle if needed. */
  void (*fork_free_fd) (int s, pid_t p, pid_t c);       /*for SOCK_CLOEXEC when fork if needed. */
  unsigned int (*ep_ctl) (int epFD, int proFD, int ctl_ops, struct epoll_event * event, void *pdata);   /*when fd add to epoll fd, triggle stack to proc if need */
  unsigned int (*ep_getevt) (int epFD, int profd, unsigned int events); /*check whether some events exist really */
  int (*ep_prewait_proc) (int epfd);
  int (*stack_fd_check) (int s, int flag);      /*check whether fd belong to stack, if belong, return 1, else return 0 */
  int (*stack_alloc_fd) ();     /*alloc a fd id for epoll */
  int (*peak) (int s);          /*used for stack-x , isource maybe no need */
} nstack_extern_ops;

/*
 *The event notify interface provided to the protocol stack by nStack
 *these interface is provided by nStack to Protocol stack
 */
typedef struct __nstack_event_cb
{
  void *handle;                 /*current so file handler */
  int type;                     /*nstack is assigned to the protocol stack and needs to be passed to nstack when the event is reported */
  int (*event_cb) (void *pdata, int events);
} nstack_event_cb;

typedef struct __nstack_proc_cb
{
  nstack_socket_ops socket_ops; /*posix socket api */
  nstack_extern_ops extern_ops; /*other proc callback */
} nstack_proc_cb;

typedef int (*nstack_stack_register_fn) (nstack_proc_cb * proc_fun,
                                         nstack_event_cb * event_ops);

#endif
