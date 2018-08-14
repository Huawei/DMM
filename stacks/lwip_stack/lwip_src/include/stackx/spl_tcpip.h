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

#ifndef __STACKX_TCPIP_H__
#define __STACKX_TCPIP_H__

#include "spl_opt.h"
#include "stackx/spl_ip_addr.h"
#include "tcp.h"

#define USEAGE_LOW 60
#define USEAGE_HIGHT 80
#define USEAGE_INVALID 0xFF

/*** Put into stackx_instance *********

************************************/
#include "stackx/spl_api_msg.h"
#include "netifapi.h"
#include "stackx/spl_pbuf.h"
#include "stackx/spl_api.h"
#include "sys.h"
#include "netif.h"
#include "ip_module_api.h"
#include "internal_msg.h"
#include "pbuf.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/** Function prototype for send timeout message */
err_t ltt_apimsg (sys_timeout_handler h, void *arg);

/** Function prototype for the init_done function passed to tcpip_init */
typedef void (*tcpip_init_done_fn) (void *arg);

/** Function prototype for functions passed to tcpip_callback() */
typedef void (*tcpip_callback_fn) (void *ctx);

int init_by_main_thread ();
int init_by_tcpip_thread ();
err_t spl_tcpip_input (struct pbuf *p, struct netif *inp);

int post_ip_module_msg (void *arg, ip_module_type type,
                        ip_module_operate_type operate_type);
int process_ip_module_msg (void *arg, ip_module_type type,
                           ip_module_operate_type operate_type);
int init_new_network_configuration ();

#if STACKX_NETIF_API
err_t tcpip_netif_add (msg_add_netif * tmp);
#endif /* STACKX_NETIF_API */

err_t ltt_clearTmrmsg (void *pcb, void *arg);

sys_mbox_t get_primary_box ();

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __STACKX_TCPIP_H__ */
