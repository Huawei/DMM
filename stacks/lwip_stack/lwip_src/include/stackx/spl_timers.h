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

#ifndef __SPL_TIMERS_H__
#define __SPL_TIMERS_H__

#include "opt.h"
#include "common_mem_base_type.h"

typedef void (*sys_timeout_handler) (void *arg);

#include "rb_tree.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/** Function prototype for a timeout callback function. Register such a function
 * using sys_timeout().
 *
 * @param arg Additional argument to pass to the function - set up by sys_timeout()
 */

/*
 * *************************************************************************
 * PTIMER defined  2013/3/15
 * *************************************************************************
 */
#define PTIMER_DEFAULT 0x00     /* periodic mode */
#define PTIMER_ONESHOT 0x01
#define  PTIMER_USER_DEF 0x02

enum msg_type
{
  SYS_PTIMEROUT_MSG,
  SYS_UNPTIMEROUT_MSG,
};

struct msg_context
{
  unsigned long msec;
  union
  {
    sys_timeout_handler handle;
  } action;
#define _act_category action.act_category
#define _phandle action.handle
  u32_t flags;                  /* oneshot|user_def|... */
  void *ctx;                    /* pcb ptr */
};

struct ptimer_node
{
  struct rb_node node;
  unsigned long abs_nsec;
  struct msg_context info;
  unsigned long state;
  u16_t index;                  /* store a lwip thread message box id */
};

struct ptimer_msg
{
  enum msg_type msg_type;
  struct ptimer_node *node;
  struct ptimer_msg *next, *prev;
};

struct ptimer_base
{
  struct rb_root active;
  struct rb_node *first;        /* point the recently timeout */
  pthread_mutex_t lock;
  pthread_cond_t cond;
  struct ptimer_msg *head, *tail;
};

/*
 * *****************************************************
 *  ptimer  E-N-D
 * *****************************************************
 */
void ptimer_thread (void *arg);
void timeout_phandler (void *act, void *arg);
void regedit_ptimer (enum msg_type type, sys_timeout_handler handler,
                     struct ptimer_node *node);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __LWIP_TIMERS_H__ */
