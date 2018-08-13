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

#ifndef __LWIP_API_MSG_H__
#define __LWIP_API_MSG_H__

#include <stddef.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "nsfw_msg.h"
#include "spl_opt.h"
#include "spl_ip_addr.h"
#include "spl_err.h"
#include "spl_api.h"
//#include "sockets.h"
#include "stackx_spl_share.h"
#include "stackx_spl_msg.h"

/* From lwip */
#include "tcp.h"
#include "udp.h"
#include "sys.h"

#ifdef HAL_LIB
#else
#include "rte_memcpy.h"
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* For the netconn API, these values are use as a bitmask! */
#define NETCONN_SHUT_RD 1
#define NETCONN_SHUT_WR 2
#define NETCONN_SHUT_RDWR (NETCONN_SHUT_RD | NETCONN_SHUT_WR)

struct callback_fn
{
  tcp_sent_fn sent_fn;
  tcp_recv_fn recv_fn;
  tcp_connected_fn connected_fn;
  tcp_poll_fn poll_fn;
  tcp_err_fn err_fn;
  tcp_err_fn close_fn;
  tcp_accept_fn accept_fn;
};

#ifdef HAL_LIB
#else
typedef enum _mbuf_recyle_flg
{
  MBUF_UNUSED = 0,
  MBUF_HLD_BY_APP = 1,
  MBUF_HLD_BY_SPL = 2,
} mbuf_recycle_flg;
#endif

err_t sp_enqueue (struct common_pcb *cpcb, void *p);
err_t accept_dequeue (spl_netconn_t * lconn, void **new_buf,
                      u32_t timeout /*miliseconds */ );
err_t accept_enqueue (spl_netconn_t * conn, void *p);
void free_conn_by_spl (spl_netconn_t * conn);
void unlink_pcb (struct common_pcb *cpcb);
void do_try_delconn (void *close_data, u32 delay_sec);
void do_delconn (struct common_pcb *cpcb, msg_delete_netconn * msg);
void do_bind (struct common_pcb *cpcb, msg_bind * msg);
void do_pbuf_free (struct spl_pbuf *buf);
void do_connect (struct common_pcb *cpcb, msg_connect * msg);
void do_listen (struct common_pcb *cpcb, msg_listen * msg);
void do_send (struct common_pcb *cpcb, msg_send_buf * msg);
void do_recv (struct common_pcb *cpcb, msg_recv_buf * msg);
void do_write (struct common_pcb *cpcb, msg_write_buf * msg);
void do_getaddr (struct common_pcb *cpcb, msg_getaddrname * msg);
void do_close (struct common_pcb *cpcb, msg_close * msg);
void do_getsockopt_internal (struct common_pcb *cpcb,
                             msg_setgetsockopt * smsg);
void do_setsockopt_internal (struct common_pcb *cpcb,
                             msg_setgetsockopt * smsg);
void do_getsockname (struct common_pcb *cpcb, msg_getaddrname * amsg);
int netconn_drain (enum spl_netconn_type t, spl_netconn_t * conn);
int spl_pcb_new (msg_new_netconn * m);
void do_app_touch (msg_app_touch * smsg);
int do_close_finished (struct common_pcb *cpcb, u8_t close_finished,
                       u8_t shut, err_t err, int OpShutDown);
err_t do_writemore (struct spl_netconn *conn);
err_t do_close_internal (struct common_pcb *cpcb, int OpShutDown);

u8 get_shut_op (data_com_msg * m);
int ks_to_stk_opt (int opt);
void update_tcp_state (spl_netconn_t * conn, enum tcp_state state);

err_t spl_poll_tcp (void *arg, struct tcp_pcb *pcb);
err_t spl_recv_tcp (void *arg, struct tcp_pcb *pcb, struct pbuf *p,
                    err_t err);
err_t spl_sent_tcp (void *arg, struct tcp_pcb *pcb, u16_t len);
void spl_err_tcp (void *arg, err_t err);
err_t spl_tcp_recv_null (void *arg, struct tcp_pcb *pcb, struct pbuf *p,
                         err_t err);
err_t spl_do_connected (void *arg, struct tcp_pcb *pcb, err_t err);
err_t spl_accept_function (void *arg, struct tcp_pcb *newpcb, err_t err);

struct common_pcb *alloc_common_pcb ();
void free_common_pcb (struct common_pcb *cpcb);
int common_pcb_init (struct common_pcb *cpcb);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __LWIP_API_MSG_H__ */
