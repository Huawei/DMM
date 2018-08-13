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

#ifndef MSG_H
#define MSG_H
#include "types.h"
#include "common_mem_api.h"
#include "nsfw_rti.h"
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define MAX_MSG_SIZE 512
COMPAT_PROTECT (MAX_MSG_SIZE, 512);
#define MAX_MSG_PARAM_SIZE 128
COMPAT_PROTECT (MAX_MSG_PARAM_SIZE, 128);

#define MSG_ASYN_POST 0
#define MSG_SYN_POST 1

typedef struct
{
  u16 module_type;
  u16 major_type;
  u16 minor_type;
  u16 op_type;                  /* MSG_SYN_POST or MSG_ASYN_POST */
  sys_sem_st op_completed;
  i32 err;
  PRIMARY_ADDR void *msg_from;  /* use it to free msg */
  i64 receiver;
  i64 comm_receiver;
  nsfw_res res_chk;
  u32 src_pid;
  u32 recycle_pid;              /* use it in recycle */
  u64 span_pid;
  i64 extend_member_bit;
} msg_param;

typedef struct msg_t
{
  msg_param param;
  i8 msg_param_pad[MAX_MSG_PARAM_SIZE - sizeof (msg_param)];    /* sizeof(msg_param) + sizeof(msg_param_pad) = MAX_MSG_PARAM_SIZE */
  i64 buffer[(MAX_MSG_SIZE - MAX_MSG_PARAM_SIZE) / 8];
}
data_com_msg;

#define MAX_MODULE_TYPE 64
#define MAX_MAJOR_TYPE 256
#define MAX_MINOR_TYPE 256

struct rti_queue
{
  /* corresponding to enum spl_tcpip_msg_type */
  volatile u64 tcpip_msg_enq[MAX_MAJOR_TYPE];
  volatile u64 tcpip_msg_enq_fail[MAX_MAJOR_TYPE];
  u64 tcpip_msg_deq[MAX_MAJOR_TYPE];

  /* corresponding to enum api_msg_type, this is sub-type of SPL_TCPIP_NEW_MSG_API */
  volatile u64 api_msg_enq[MAX_MINOR_TYPE];
  volatile u64 api_msg_enq_fail[MAX_MINOR_TYPE];
  u64 api_msg_deq[MAX_MINOR_TYPE];

  u64 extend_member_bit;
};

enum MSG_MODULE_TYPE
{
  MSG_MODULE_IP,
  MSG_MODULE_SBR,
  MSG_MODULE_HAL,
  MSG_MODULE_SPL,
  MSG_MODULE_TIMER,
  MSG_MODULE_MT,
  MSG_MODULE_DFX,
  MSG_MODULE_MAX = MAX_MODULE_TYPE
};

typedef int (*msg_fun) (data_com_msg * m);

/* *INDENT-OFF* */
extern msg_fun g_msg_module_fun_array[MAX_MODULE_TYPE];
extern msg_fun g_msg_module_major_fun_array[MAX_MODULE_TYPE][MAX_MAJOR_TYPE];
extern msg_fun g_msg_module_major_minor_fun_array[MAX_MODULE_TYPE][MAX_MAJOR_TYPE][MAX_MINOR_TYPE];
extern msg_fun g_msg_unsupport_fun;
/* *INDENT-ON* */

#define REGIST_MSG_MODULE_FUN(module, fun) \
    static void regist_ ## module ## _function (void)                   \
    __attribute__((__constructor__));                              \
    static void regist_ ## module ## _function (void)                   \
    {                                                               \
        g_msg_module_fun_array[module] = fun;                           \
    } \

#define REGIST_MSG_MODULE_MAJOR_FUN(module, major, fun) \
    static void regist_ ## module ## major ## _function (void)            \
    __attribute__((__constructor__));                              \
    static void regist_ ## module ## major ## _function (void)            \
    {                                                               \
        g_msg_module_major_fun_array[module][major] = fun;              \
    } \

#define REGIST_MSG_MODULE_MAJOR_MINOR_FUN(module, major, minor, fun) \
    static void regist_ ## module ## major ## minor ## _function (void)     \
    __attribute__((__constructor__));                              \
    static void regist_ ## module ## major ## minor ## _function (void)     \
    {                                                               \
        g_msg_module_major_minor_fun_array[module][major][minor] = fun; \
    } \

#define REGIST_MSG_UNSUPPORT_FUN(fun) \
    static void regist_msg_unsupport_function (void)     \
    __attribute__((__constructor__));                              \
    static void regist_msg_unsupport_function (void)     \
    {                                                               \
        g_msg_unsupport_fun = fun; \
    }

static inline int
unsupport_msg (data_com_msg * m)
{
  if (g_msg_unsupport_fun)
    {
      return g_msg_unsupport_fun (m);
    }

  return -1;
}

/*****************************************************************************
*   Prototype    : call_msg_fun
*   Description  : call msg fun
*   Input        : data_com_msg* m
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
call_msg_fun (data_com_msg * m)
{
  u16 module = m->param.module_type;
  u16 major = m->param.major_type;
  u16 minor = m->param.minor_type;

  if ((module >= MAX_MODULE_TYPE) || (major >= MAX_MAJOR_TYPE)
      || (minor >= MAX_MINOR_TYPE))
    {
      return unsupport_msg (m);
    }

  nsfw_rti_stat_macro (NSFW_STAT_PRIMARY_DEQ, m);

  if (g_msg_module_fun_array[module]
      && (g_msg_module_fun_array[module] (m) != 0))
    {
      return -1;
    }

  if (g_msg_module_major_fun_array[module][major]
      && (g_msg_module_major_fun_array[module][major] (m) != 0))
    {
      return -1;
    }

  if (g_msg_module_major_minor_fun_array[module][major][minor])
    {
      return g_msg_module_major_minor_fun_array[module][major][minor] (m);
    }

  if (!g_msg_module_fun_array[module]
      && !g_msg_module_major_fun_array[module][major]
      && !g_msg_module_major_minor_fun_array[module][major][minor])
    {
      return unsupport_msg (m);
    }

  return 0;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
