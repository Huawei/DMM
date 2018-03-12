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

#ifndef _LINUX_KERNEL_MODULE_H_
#define _LINUX_KERNEL_MODULE_H_

#include "nstack_dmm_api.h"

#define K_SELECT_THREAD_NAME "kernel_select"
#define K_EPOLL_THREAD_NAME "kernel_epoll"

#ifndef ks_success
#define ks_success 0
#endif

#ifndef ks_fail
#define ks_fail -1
#endif

#ifndef ks_bool
typedef char ks_bool;
#endif

#ifndef ks_false
#define ks_false 0
#endif

#ifndef ks_true
#define ks_true 1
#endif

int kernel_stack_register (nstack_proc_cb * ops, nstack_event_cb * val);

typedef struct
{
  nstack_event_cb regVal;
  int epfd;
  int checkEpollFD;
  pthread_t ep_thread;
  ks_bool thread_inited;
  pthread_t select_thread;      //listen select events
  nstack_socket_ops libcOps;
} kernel_stack_info_t;

extern kernel_stack_info_t g_ksInfo;

extern int linux_kernel_stack_init (void);
#endif
