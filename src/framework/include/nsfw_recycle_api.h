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

#ifndef _NSFW_RECYCLE_API_H
#define _NSFW_RECYCLE_API_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSFW_RECYCLE_MODULE "nsfw_recycle"

typedef enum _nsfw_recycle_item_type
{
  NSFW_REC_TYPE_NULL = 0,
  NSFW_REC_SBR_START = 1,
  NSFW_REC_SBR_SOCKET,
  NSFW_REC_SBR_END = NSFW_REC_SBR_START + 63,
  NSFW_REC_NSOCKET_START,
  NSFW_REC_NSOCKET_EPOLL,
  NSFW_REC_NSOCKET_END = NSFW_REC_NSOCKET_START + 63,
  NSFW_REC_TYPE_MAX = 512
} nsfw_recycle_item_type;

typedef enum _nsfw_recycle_priority
{
  NSFW_REC_PRO_HIGHTEST = 0,
  NSFW_REC_PRO_NORMAL = 1,
  NSFW_REC_PRO_DEFAULT = 2,
  NSFW_REC_PRO_LOWEST = 3,
  NSFW_REC_PRO_MAX = 4
} nsfw_recycle_priority;

typedef enum _nsfw_rcc_stat
{
  NSFW_RCC_CONTINUE = 0,
  NSFW_RCC_SUSPEND = 1,
  NSFW_RCC_FAILED = 2,
} nsfw_rcc_stat;

/*work on nStackMain*/
typedef nsfw_rcc_stat (*nsfw_recycle_fun) (u32 exit_pid, void *pdata,
                                           u16 rec_type);
extern u8 nsfw_recycle_reg_fun (u16 obj_type, nsfw_recycle_fun fun);
extern u8 nsfw_recycle_obj_end (u32 pid);
extern u8 nsfw_recycle_lock_rel_fun (nsfw_recycle_fun fun, void *data,
                                     u8 proc_type);
extern int nsfw_recycle_exit_pid_lock (u32 pid, u8 proc_type, void *argv);

#define REGIST_RECYCLE_OBJ_FUN(_obj_type, _fun) \
    NSTACK_STATIC void regist_ ## _obj_type ## _fun (void)            \
    __attribute__((__constructor__));                              \
    NSTACK_STATIC void regist_ ## _obj_type ## _fun (void)            \
    {                                                               \
        (void)nsfw_recycle_reg_fun(_obj_type, _fun);              \
    }

#define REGIST_RECYCLE_LOCK_REL(_fun, _data, _proc) \
    NSTACK_STATIC void regist_lock ## _fun (void)            \
    __attribute__((__constructor__));                              \
    NSTACK_STATIC void regist_lock ## _fun (void)            \
    {                                                               \
        (void)nsfw_recycle_lock_rel_fun(_fun, _data,_proc);              \
    }

/*work on nStackApp*/
extern void *nsfw_recycle_reg_obj (u8 priority, u16 rec_type, void *data);
extern u8 nsfw_recycle_fork_init ();
extern int nsfw_recycle_rechk_lock ();

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSFW_RECYCLE_API_H  */
