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

#ifndef _NSFW_PS_API_H
#define _NSFW_PS_API_H

#include "list.h"
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSFW_PS_MODULE      "nsfw_ps"

#define NSFW_PS_MAX_CALLBACK 16

#define NSFW_MAX_HBT_CHK_COUNT_DEF 60

typedef enum _nsfw_ps_state
{
  NSFW_PS_NULL = 0,
  NSFW_PS_START,
  NSFW_PS_RUNNING,
  NSFW_PS_PARENT_FORK,
  NSFW_PS_CHILD_FORK,
  NSFW_PS_HBT_FAILED,
  NSFW_PS_EXITING,
  NSFW_PS_EXIT,
  NSFW_PS_MAX
} nsfw_ps_state;

/* for state change call back proc*/
typedef int (*nsfw_ps_proc_fun) (void *pps_info, void *argv);
typedef struct _nsfw_ps_callback
{
  u8 state;
  nsfw_ps_proc_fun fun;
  void *argv;
} nsfw_ps_callback;

/* for value in ps_info get/set*/
typedef enum _nsfw_ps_user_value
{
  NSFW_REC_IDX = 1,
  NSFW_REC_TIMER = 2,
  NSFW_PS_UV_MAX = 16
} nsfw_ps_user_value;
#define nsfw_ps_set_uv(_pps_info, _type, _value) (_pps_info)->value[(_type)] = (_value)
#define nsfw_ps_get_uv(_pps_info, _type) ((_pps_info)->value[(_type)])

typedef struct _nsfw_ps_info
{
  struct list_head node;
  u8 alloc_flag;
  u8 state;                     /*nsfw_ps_state */
  u8 proc_type;                 /*fw_poc_type */
  u8 rechk_flg;
  u32 host_pid;
  u32 parent_pid;               /* only use for fork */
  u32 cur_child_pid;            /* only use for fork */
  void *exit_timer_ptr;
  void *resend_timer_ptr;
  void *hbt_timer_ptr;
  u32 hbt_failed_count;
  nsfw_ps_callback callback[NSFW_PS_MAX_CALLBACK];
  void *value[NSFW_PS_UV_MAX];
} nsfw_ps_info;

typedef struct _nsfw_thread_dogs
{
  u8 alloc_flag;
  i32 count;
  u32 thread_id;
} nsfw_thread_dogs;

extern nsfw_ps_info *nsfw_ps_info_alloc (u32 pid, u8 proc_type);
extern nsfw_ps_info *nsfw_ps_info_get (u32 pid);
extern void nsfw_ps_info_free (nsfw_ps_info * ps_info);

extern u8 nsfw_ps_reg_fun (nsfw_ps_info * pps_info, u8 ps_state,
                           nsfw_ps_proc_fun fun, void *argv);

/* will auto reg after ps_info alloc*/
extern u8 nsfw_ps_reg_global_fun (u8 proc_type, u8 ps_state,
                                  nsfw_ps_proc_fun fun, void *argv);

typedef struct _nsfw_ps_info_msg
{
  u32 host_pid;
  u32 parent_pid;
  u64 reserve;
} nsfw_ps_info_msg;
extern u8 nsfw_ps_exit_end_notify (u32 pid);

/*for heartbeat check*/
extern u8 nsfw_ps_check_dst_init (u8 dst_proc_type);
extern u8 nsfw_thread_chk ();
extern nsfw_thread_dogs *nsfw_thread_getDog ();
extern u8 nsfw_thread_chk_unreg ();
extern u8 nsfw_ps_hbt_start (nsfw_ps_info * ps_info);
extern u8 nsfw_ps_hbt_stop (nsfw_ps_info * ps_info);

extern u32 nsfw_ps_iterator (nsfw_ps_proc_fun fun, void *argv);

#define MAX_THREAD 16
extern pthread_t g_all_thread[];
extern u8 nsfw_reg_trace_thread (pthread_t tid);

typedef int (*nsfw_ps_pid_fun) (u32 pid, u8 proc_type, void *argv);
extern int nsfw_ps_rechk_pid_exit (nsfw_ps_pid_fun fun, void *argv);
extern nsfw_ps_info *nsfw_share_ps_info_get (u32 pid);
extern void nsfw_ps_cfg_set_chk_count (u16 count);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSFW_PS_API_H  */
