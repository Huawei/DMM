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

#ifndef _NSFW_PS_MEM_MODULE_H
#define _NSFW_PS_MEM_MODULE_H

#include "list.h"
#include "pidinfo.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define MEMZONE_MAX_NAME 32
#define NS_MAX_FORK_NUM 32
#define NSFW_PS_MEM_MAX_FILTER 4

#define NSFW_SRV_RESTORE_TVALUE_DEF 120
#define NSFW_SRV_RESTORE_TVALUE g_mem_cfg.srv_restore_tvalue
#define NSFW_SRV_STATE_SUSPEND  g_mem_cfg.srv_suspend

#define NSFW_PS_MEM_RESEND_TIMER  1
#define NSFW_PS_MEM_RESEND_TVLAUE_DEF 2
#define NSFW_PS_SEND_PER_TIME_DEF     150
#define NSFW_PS_SEND_PER_TIME     (g_mem_cfg.ps_send_per_time)
#define NSFW_PS_MEM_RESEND_TVLAUE (g_mem_cfg.ps_exit_resend_tvalue)

typedef struct _ns_mem_mng_init_cfg
{
  u16 srv_restore_tvalue;
  u16 ps_exit_resend_tvalue;
  u16 ps_send_per_time;
  u16 srv_suspend;
  void *p_restore_timer;
} ns_mem_mng_init_cfg;

/*mem alloc by msg begin*/
typedef struct
{
  nsfw_mem_name stname;
  u16 ustype;
} nsfw_mem_type_info;

#define NSFW_MEM_CALL_ARG_BUF 256
#define MEM_GET_CALLARGV(_dst_member,_src_member, _dst_type,_srctype,_dst_buf, _src_buf) \
        ((_dst_type*)(void*)_dst_buf)->_dst_member = ((_srctype*)(void*)_src_buf)->_src_member

typedef void *(*nsfw_ps_mem_create_fun) (void *memstr);
typedef u8 (*nsfw_ps_mem_msg_to_memstr) (u16 msg_type, char *msg_body,
                                         char *memstr_buf, i32 buf_len);

typedef struct __nsfw_ps_mem_item_cfg
{
  u16 usmsg_type;
  u16 item_size;
  u16 mem_type;
  nsfw_ps_mem_create_fun create_fun;
  nsfw_ps_mem_msg_to_memstr change_fun;
} nsfw_ps_mem_item_cfg;

void *mem_item_free (void *pdata);
void *mem_item_lookup (void *pdata);
u8 mem_item_get_callargv (u16 msg_type, char *msg_body, char *memstr_buf,
                          i32 buf_len);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSFW_PS_MEM_MODULE_H  */
