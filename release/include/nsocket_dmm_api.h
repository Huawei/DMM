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

typedef enum
{
  NSTACK_MODEL_TYPE1 = 1,       /*nSocket and stack belong to the same process */
  NSTACK_MODEL_TYPE2 = 2,       /*nSocket and stack belong to different processes,
                                 *and nStack don't take care the communication between stack and stack adpt
                                 */
  NSTACK_MODEL_TYPE3 = 3,       /*nSocket and stack belong to different processes, and sbr was spplied to communicate whit stack */
  NSTACK_MODEL_INVALID,
} nstack_model_deploy_type;

typedef enum
{
  MBUF_TRANSPORT,
  MBUF_UDP,
  MBUF_IP,
  MBUF_LINK,
  MBUF_RAW,
  MBUF_MAX_LAYER,
} mbuf_layer;

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

/*
 *Interactive interface for Protocol stack and nStack defined here
 *these interface is provided by Protocol stack to nStack
 */
typedef struct __nstack_extern_ops
{
  int (*module_init) (void);    /*stack module init */
  int (*fork_init_child) (pid_t p, pid_t c);    /*after fork, stack child process init again if needed. */
  void (*fork_parent_fd) (int s, pid_t p);      /*after fork, stack parent process proc again if needed. */
  void (*fork_child_fd) (int s, pid_t p, pid_t c);      /*after fork, child record pid for recycle if needed. */
  void (*fork_free_fd) (int s, pid_t p, pid_t c);       /*for SOCK_CLOEXEC when fork if needed. */
  unsigned int (*ep_ctl) (int epFD, int proFD, int ctl_ops, struct epoll_event * event, void *pdata);   /*when fd add to epoll fd, triggle stack to proc if need */
  unsigned int (*ep_getevt) (int epFD, int profd, unsigned int events); /*check whether some events exist really */
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

/*
 *Module registration interface.
 *ouput param:posix_ps,proc_ops
 *input param:event_ops
 */
typedef int (*nstack_stack_registe_fn) (nstack_proc_cb * proc_fun,
                                        nstack_event_cb * event_ops);
typedef void (*io_send_fn) (void *pbuf, void *pargs);
typedef void (*io_recv_cb) (void **pbuf, void *pargs);
typedef int (*recycle_fun) (unsigned int exit_pid, void *pdata,
                            unsigned short rec_type);

/*structure type of mbuf*/
typedef enum
{
  MBUF_TYPE_DPDK,               /*mbuf structure type of DPDK */
  MBUF_TYPE_STACKPOOL,          /*mbuf structure type of stack-x */
  MBUF_TYPE_INVALID,
} mbuf_type;

typedef struct
{
  int (*event_notify) (void *pdata, int events);        /*event notify */
  int (*ep_pdata_free) (void *pdata);   /*pdata free */
  void (*obj_recycle_reg) (unsigned char priority, unsigned short rec_type, void *data, recycle_fun cb_fun);    /*source recycle */
  void *(*mbuf_alloc) (mbuf_layer layer, unsigned short length, mbuf_type Type, unsigned short thread_index);   /*zero copy memory alloc */
  void (*mbuf_free) (void *p, mbuf_type Type);  /*zero copy memory free */
  void (*pkt_send_cb) (void *pbuf, mbuf_type inType, mbuf_type outType,
                       void *pargs, io_send_fn io_send);
  void (*pkt_recv_cb) (void **pbuf, mbuf_type inType, mbuf_type outType,
                       void *pargs, io_recv_cb io_recv);
} nStack_adpt_fun;

extern int nstack_adpt_init (nstack_model_deploy_type deploy_type,
                             nStack_adpt_fun * out_ops, int flag);

#endif
