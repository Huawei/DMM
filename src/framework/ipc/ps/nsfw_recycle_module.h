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

#ifndef _NSFW_RECYCLE_MODULE_H
#define _NSFW_RECYCLE_MODULE_H

#include "nsfw_recycle_api.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define MEM_RECYCLE_PER_PRO_QUE 128
#define MEM_RECYCLE_OBJ_MAX_NUM (MEM_RECYCLE_PER_PRO_QUE*NSFW_REC_PRO_MAX)

#define MEM_REC_POOL_NAME "APP_REC_POOL"
#define MEM_REC_QUEUE_NAME "APP_REC_RINGQ"

#define REC_POOL_MAGIC_FLAG 0xDBFC01A600000000

#define NSFW_REC_WEND_TIMER  1
#define NSFW_REC_WEND_TVLAUE_DEF 60

#define NSFW_REC_WEND_TVLAUE (g_rec_cfg.rec_waite_end_tvalue)

typedef struct _nsfw_recycle_obj
{
  u8 alloc_flag;
  u8 u8reserve;
  u16 rec_type;
  u32 host_pid;
  void *data;
  u64 u64reserve;
} nsfw_recycle_obj;

#define NSFW_REC_LOCK_REL_MAX_FUN 32

typedef struct _nsfw_rec_fun_info
{
  nsfw_recycle_fun rec_fun;
  void *data;
  u8 proc_type;
} nsfw_rec_fun_info;

typedef struct _nsfw_recycle_pool
{
  u32 pool_size;
  nsfw_recycle_obj obj[0];
} nsfw_recycle_pool;

typedef struct _nsfw_recycle_cfg
{
  u16 rec_waite_end_tvalue;
  mring_handle mem_rec_obj_pool;
  mzone_handle mem_rec_pro[NSFW_REC_PRO_MAX];
} nsfw_recycle_cfg;

extern nsfw_rcc_stat nsfw_recycle_callback_all_obj (u32 pid,
                                                    nsfw_recycle_pool *
                                                    rec_pool);
extern int nsfw_recycle_obj_timeout (u32 timer_type, void *data);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSFW_RECYCLE_MODULE_H  */
