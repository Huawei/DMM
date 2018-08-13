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

#ifndef __internal_msg_h__
#define __internal_msg_h__

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#include "nsfw_msg.h"
#include "netif.h"
#include "lwip/netifapi.h"
#include "ip_module_api.h"

enum netif_msg_type
{
  NETIF_DO_ADD,
  NETIF_MSG_API_MAX = MAX_MINOR_TYPE
};

/* NETIF_DO_ADD */
typedef struct msg_add_netif_T
{
  void (*function) (struct msg_add_netif_T * m);
  struct netif *netif;
  spl_ip_addr_t *ipaddr;
  spl_ip_addr_t *netmask;
  spl_ip_addr_t *gw;
  void *state;
  netif_init_fn init;
  netif_input_fn input;
  netifapi_void_fn voidfunc;
  /* no need to extend member */
} msg_add_netif;

typedef struct
{
  ip_module_type type;
  ip_module_operate_type operate_type;
  void *arg;
} msg_ip_module;

typedef struct msg_internal_callback_T
{
  void (*function) (void *ctx);
  void *ctx;
} msg_internal_callback;

enum timer_msg_type
{
  TIMER_MSG_TIMEOUT,
  TIMER_MSG_CLEAR,
  TIMER_MSG_MAX = MAX_MINOR_TYPE
};

typedef struct msg_timer_T
{
  void *act;
  void *arg;
} msg_timer;

enum mt_msg_type
{
  MT_MSG_VER_MGR,
  MT_MSG_MAX = MAX_MINOR_TYPE
};

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __internal_msg_h__ */
