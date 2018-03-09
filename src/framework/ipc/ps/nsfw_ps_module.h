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

#ifndef _NSFW_PS_MODULE_H
#define _NSFW_PS_MODULE_H

#include "nsfw_ps_api.h"
#include "nsfw_mem_api.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

/*==============================================*
 *      constants or macros define              *
 *----------------------------------------------*/

#define NSFW_MAX_PID 65535
COMPAT_PROTECT (NSFW_MAX_PID, 65535);

#define NSFW_PS_INFO_MAX_COUNT 4095
#define MAX_NET_LINK_BUF_DEF 0x34000*32

#define NSFW_PS_WEXIT_TIMER  1
#define NSFW_PS_WEXIT_TVLAUE_DEF 180

#define NSFW_PS_WEXIT_TVLAUE (g_ps_cfg.ps_waite_exit_tvalue)
#define MAX_NET_LINK_BUF     (g_ps_cfg.net_link_buf)

typedef struct _nsfw_ps_init_cfg
{
  u32 ps_info_size;
  u32 net_link_buf;

  u16 ps_waite_exit_tvalue;
  u16 ps_chk_hbt_count;
  u16 ps_chk_hbt_soft_count;
  u16 ps_chk_hbt_tvalue;

  mring_handle ps_info_pool;
} nsfw_ps_init_cfg;

#define NSFW_PS_CHK_TIMER  1
#define NSFW_PS_FIRST_CHK_TVLAUE 1
#define NSFW_PS_CHK_EXIT_TVLAUE 1

typedef struct _nsfw_pid_item
{
  u8 proc_type;
  u8 u8_reserve;
  u16 u16_reserve;
  u32 u32_reserve;
  nsfw_ps_info *ps_info;
} nsfw_pid_item;

int nsfw_ps_change_fun (i32 epfd, i32 socket, u32 events);
u8 nsfw_sw_ps_state (nsfw_ps_info * pps_info, u8 new_state);

/* for heartbeat checking*/
#define NSFW_MAX_THREAD_DOGS_COUNT 8
#define NSFW_CHK_HBT_TIMER  1
#define NSFW_MAX_HBT_PROC_FUN  4

#define NSFW_CHK_HBT_TVLAUE_DEF 1

#define NSFW_MAX_HBT_CHK_COUNT   (g_ps_cfg.ps_chk_hbt_count)
#define NSFW_SOFT_HBT_CHK_COUNT   (g_ps_cfg.ps_chk_hbt_soft_count)
#define NSFW_CHK_HBT_TVLAUE      (g_ps_cfg.ps_chk_hbt_tvalue)

typedef struct _nsfw_ps_chk_msg
{
  u32 ps_state;
  i32 thread_chk_count;
} nsfw_ps_chk_msg;

int nsfw_ps_chk_timeout (u32 timer_type, void *data);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSFW_PS_MODULE_H  */
