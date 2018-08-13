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

#ifndef __SPL_API_H__
#define __SPL_API_H__

#include "opt.h"
#include "common_mem_base_type.h"
#include <stddef.h>             /* for size_t */
#include "arch/queue.h"
#include "arch/sys_arch.h"
#include "arch/atomic_32.h"
#include "stackx_common_opt.h"
#include "stackx_spl_share.h"

/* From lwip */
#include "api.h"
#include "sys.h"
#include "sys_arch.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define	MAX_WAIT_TIMEOUT	0x7FFFFFFF

/* Throughout this file, IPaddresses and port numbers are expected to be in
 * the same byte order as in the corresponding pcb.
 */

/* Flags for struct netconn.flags (u8_t) */

/** TCP: when data passed to netconn_write doesn't fit into the send buffer,
    this temporarily stores whether to wake up the original application task
    if data couldn't be sent in the first try. */
#define SPL_NETCONN_FLAG_WRITE_DELAYED 0x01

/** Should this netconn avoid blocking? */
#define SPL_NETCONN_FLAG_NON_BLOCKING 0x02

/** Was the last connect action a non-blocking one? */
#define SPL_NETCONN_FLAG_IN_NONBLOCKING_CONNECT 0x04

/** If this is set, a TCP netconn must call netconn_recved() to update
    the TCP receive window (done automatically if not set). */
#define SPL_NETCONN_FLAG_NO_AUTO_RECVED 0x08

/** If a nonblocking write has been rejected before, poll_tcp needs to
    check if the netconn is writable again */
// #define NETCONN_FLAG_CHECK_WRITESPACE 0x10

enum stackx_model
{
  SOCKET_STACKX = 0,
  CALLBACK_STACKX
};

enum callback_type
{
  API_ACCEPT_EVENT = 0xF0,
  API_RECV_EVENT,
  API_SEND_EVENT,
  API_CLOSE_EVENT
};
#if 1
/** Use to inform the callback function about changes */
enum spl_netconn_evt
{
  SPL_NETCONN_EVT_RCVPLUS,
  SPL_NETCONN_EVT_RCVMINUS,
  SPL_NETCONN_EVT_SENDPLUS,
  SPL_NETCONN_EVT_SENDMINUS,
  SPL_NETCONN_EVT_ERROR,
  SPL_NETCONN_EVT_HUP,
  SPL_NETCONN_EVT_RDHUP,
  SPL_NETCONN_EVT_AGAIN,
  SPL_NETCONN_EVT_ACCEPT
};
#endif
enum
{
  INET_ECN_NOT_ECT = 0,
  INET_ECN_ECT_1 = 1,
  INET_ECN_ECT_0 = 2,
  INET_ECN_CE = 3,
  INET_ECN_MASK = 3,
};

#define SPL_NETCONNTYPE_GROUP(t) (t & 0xF0)
#define SPL_NETCONNTYPE_DATAGRAM(t) (t & 0xE0)

/* forward-declare some structs to avoid to include their headers */
typedef struct common_pcb
{
  enum stackx_model model;

  int socket;

    /** type of the netconn (TCP, UDP or RAW) */
  enum spl_netconn_type type;

  /* share memory between sbr and stackx */
  spl_netconn_t *conn;

  u16 bind_thread_index;
  u8 close_progress;
  u8 recv_ring_not_empty;

    /** TCP: when data passed to netconn_write doesn't fit into the send buffer,
        this temporarily stores the message.
        Also used during connect and close.
     */
  data_com_msg *current_msg;

  msg_write_buf *msg_head;
  msg_write_buf *msg_tail;

  size_t write_offset;

    /** timeout to wait for new data to be received
        (or connections to arrive for listening netconns) */
  int recv_timeout;

  /* timeout to wait for send buffer writtable */
  int send_timeout;

  int sk_rcvlowat;

  //DFX stat for connection packet
  /* dfx_conn_t dfx; */

  /*store the hostpid info for release */
  uint32_t hostpid;
  u8_t l4_tick;                 /* if is odd number, use l4 ring first */
  u8_t dataSentFlag;

  nsfw_res res_chk;
} common_pcb;

/** A callback prototype to inform about events for a netconn */
//typedef void (*netconn_callback)(struct spl_netconn *, enum netconn_evt, u16_t len);

/* Though these callback pointers are not set and referenced in nStack Core, still
the padding is required since the structure will be used in netconn and it needs
padding across 32-bit and 64-bit architecture */
typedef struct
{
  union
  {
    int (*accept_event) (struct spl_netconn * conn);
    PTR_ALIGN_TYPE accept_event_a;
  };

  union
  {
    int (*recv_event) (struct spl_netconn * conn);
    PTR_ALIGN_TYPE recv_event_a;
  };
  union
  {
    int (*send_event) (struct spl_netconn * conn);
    PTR_ALIGN_TYPE send_event_a;
  };
  union
  {
    int (*close_event) (struct spl_netconn * conn);
    PTR_ALIGN_TYPE close_event_a;
  };
} callback_funcation;

union ring_addr_u
{
  void *ring_addr;
  PTR_ALIGN_TYPE ring_addr_a;
};

struct mem_manage
{
  volatile uint32_t current_read;
  volatile uint32_t current_write;
  union ring_addr_u ring_addr[RECV_MAX_POOL];
  union ring_addr_u l4_ring;    /* recv ring for l4 */
  //void *ring_addr[RECV_MAX_POOL];
};

/* Pbuf free should be done in network stack */
struct spl_netconn_recvbuf_recoder
{
  struct spl_pbuf *head;
  struct spl_pbuf *tail;
  int totalLen;
};

/** Register an Network connection event */
void spl_event_callback (spl_netconn_t * conn, enum spl_netconn_evt evt,
                         int postFlag);

#define SPL_API_EVENT(c, e, p) spl_event_callback(c, e, p)

/** Set conn->last_err to err but don't overwrite fatal errors */
#define SPL_NETCONN_SET_SAFE_ERR(conn, err) do { \
        SYS_ARCH_PROTECT(lev); \
        if (!ERR_IS_FATAL((conn)->last_err)) { \
            (conn)->last_err = err; \
        } \
        SYS_ARCH_UNPROTECT(lev); \
    } while (0);

/** Set the blocking status of netconn calls (@todo: write/send is missing) */
#define spl_netconn_set_nonblocking(conn, val) do { if (val) { \
                                                     (conn)->flags |= SPL_NETCONN_FLAG_NON_BLOCKING; \
                                                 } else { \
                                                     (conn)->flags &= ~SPL_NETCONN_FLAG_NON_BLOCKING; }} while (0)

/** Get the blocking status of netconn calls (@todo: write/send is missing) */
#define spl_netconn_is_nonblocking(conn) (((conn)->flags & SPL_NETCONN_FLAG_NON_BLOCKING) != 0)

/** TCP: Set the no-auto-recved status of netconn calls (see NETCONN_FLAG_NO_AUTO_RECVED) */
#define spl_netconn_set_noautorecved(conn, val) do { if (val) { \
                                                  (conn)->flags |= SPL_NETCONN_FLAG_NO_AUTO_RECVED; \
                                              } else { \
                                                  (conn)->flags &= ~SPL_NETCONN_FLAG_NO_AUTO_RECVED; }} while (0)

/** TCP: Get the no-auto-recved status of netconn calls (see NETCONN_FLAG_NO_AUTO_RECVED) */
#define spl_netconn_get_noautorecved(conn) (((conn)->flags & SPL_NETCONN_FLAG_NO_AUTO_RECVED) != 0)

/** Set the receive timeout in milliseconds */
#define spl_netconn_set_recvtimeout(cpcb, timeout) ((cpcb)->recv_timeout = (timeout))

/** Get the receive timeout in milliseconds */
#define spl_netconn_get_recvtimeout(cpcb) ((cpcb)->recv_timeout)

/** Set the send timeout in milliseconds */
#define spl_netconn_set_sendtimeout(cpcb, timeout) ((cpcb)->send_timeout = (timeout))

/** Get the send timeout in milliseconds */
#define spl_netconn_get_sendtimeout(cpcb) ((cpcb)->send_timeout)

#define spl_netconn_set_sendbufsize(conn, sendbufsize) ((conn)->send_bufsize = (sendbufsize))

/*  "man 7 socket" information
    SO_SNDBUF
    Sets or _gets the maximum socket send buffer in bytes. The kernel doubles
    this value (to allow space for bookkeeping overhead) when it is set using
    setsockopt(2), and this doubled value is returned by getsockopt(2).
*/
#define spl_netconn_get_sendbufsize(conn) (2 *((conn)->send_bufsize))

#define spl_netconn_set_reclowbufsize(cpcb, recvlowbufsize) ((cpcb)->sk_rcvlowat = ((recvlowbufsize) > 0) ? recvlowbufsize : 1)
#define spl_netconn_get_reclowbufsize(cpcb) ((cpcb)->sk_rcvlowat)

extern int spl_post_msg (u16 mod, u16 maj, u16 min, u16 op, char *data,
                         u16 data_len, u32 src_pid);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __LWIP_API_H__ */
