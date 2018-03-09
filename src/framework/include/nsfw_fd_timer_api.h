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

#ifndef _NSFW_FD_TIMER_API_H
#define _NSFW_FD_TIMER_API_H

#include "list.h"
#include <time.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSFW_TIMER_MODULE      "nsfw_timer"

typedef struct _nsfw_timer_init_cfg
{
  u32 timer_info_size;
  void *timer_info_pool;
  struct list_head timer_head;
  struct list_head exp_timer_head;
} nsfw_timer_init_cfg;

typedef int (*nsfw_timer_proc_fun) (u32 timer_type, void *argv);
typedef struct _nsfw_timer_info
{
  struct list_head node;
  nsfw_timer_proc_fun fun;
  void *argv;
  struct timespec time_left;
  u32 timer_type;
  u8 alloc_flag;
} nsfw_timer_info;

extern nsfw_timer_info *nsfw_timer_reg_timer (u32 timer_type, void *data,
                                              nsfw_timer_proc_fun fun,
                                              struct timespec time_left);
extern void nsfw_timer_rmv_timer (nsfw_timer_info * tm_info);

extern u8 g_hbt_switch;
extern int nsfw_timer_module_init (void *param);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSFW_FD_TIMER_API_H  */
