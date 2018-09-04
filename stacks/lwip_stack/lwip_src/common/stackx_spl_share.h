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

#ifndef STACKX_SPL_SHARE_H
#define STACKX_SPL_SHARE_H
#include "types.h"
#include "common_mem_api.h"
#include "nsfw_mem_api.h"
#include "common_pal_bitwide_adjust.h"
#include "stackx_ip_addr.h"
#include <sys/types.h>
#include "spl_opt.h"
#include "stackx_ip_tos.h"
#include "stackx_common_opt.h"
#include "stackx_tx_box.h"
#include "pidinfo.h"
//#include "stackx_dfx_api.h"
#include "compiling_check.h"
#include "nsfw_msg.h"
#include "stackx_spl_msg.h"
#include "hal_api.h"
#include "stackx_types.h"
#include <errno.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define MSG_PRIO_QUEUE_NUM 3
COMPAT_PROTECT (MSG_PRIO_QUEUE_NUM, 3);

#define SPL_MSG_BOX_NUM (MSG_PRIO_QUEUE_NUM + 1)

#define SBR_FD_NETCONN_SIZE (256 * 3)
COMPAT_PROTECT (SBR_FD_NETCONN_SIZE, 256 * 3);

#define SS_NETCONN_SIZE (256 * 2)
#define SBR_FD_SIZE (SBR_FD_NETCONN_SIZE - SS_NETCONN_SIZE)

typedef struct
{
  PRIMARY_ADDR mring_handle msg_pool;
  PRIMARY_ADDR mring_handle conn_pool;
  PRIMARY_ADDR struct spl_netconn **conn_array;
  u32 conn_num;
  i64 extend_member_bit;
} sbr_recycle_group;

typedef enum
{
  SS_DELAY_STARTED,
  SS_DELAY_STOPPING,
  SS_DELAY_STOPPED,
  SS_DELAY_AGAIN
} ss_delay_flag;

typedef enum
{
  NORMAL_RECV_RING = 0,
  CUSTOM_RECV_RING = 1,
} recv_ring_type;

typedef struct
{
  PRIMARY_ADDR sbr_recycle_group *group;
  PRIMARY_ADDR struct spl_netconn *accept_from; /* for recycle fd in listen */
  void *delay_msg;
  nsfw_pidinfo pid_info;
  volatile i16 fork_ref;
  i8 is_listen_conn;
  i8 delay_flag;
} netconn_recycle;

typedef struct spl_netconn
{
  i64 recv_obj;
  i64 comm_pcb_data;            /* only used by spl */
  i64 private_data;             /* only used by spl */
  PRIMARY_ADDR mring_handle msg_box;
  PRIMARY_ADDR mring_handle recv_ring;
  volatile u32 snd_buf;
  volatile i32 epoll_flag;
  /*here using prod/cons to instead recv_avail, avoid atomic oper. */
  volatile u32_t recv_avail_prod;       /*the product of recv_avail */
  volatile u32_t recv_avail_cons;       /*the consume of recv_avail */
  volatile i32 rcvevent;
  spl_netconn_state_t state;
  u16 sendevent;
  u16 errevent;
  volatile u16 shut_status;
  u8 flags;
  i8 last_err;
  u8 CanNotReceive;
  u8 recv_ring_valid;
  u16 bind_thread_index;
  u16 mss;
  sys_sem_st close_completed;
  mring_handle conn_pool;       /* where the conn in pcb is from */

  void *epInfo;

  /* The following shared field access rules: protocol stack process
   * is responsible for writing, socket api process read only
   */
  i32 send_bufsize;
  i32 tcp_sndbuf;
  u32 tcp_wmem_alloc_cnt;
  u32 tcp_wmem_sbr_free_cnt;
  volatile u32 tcp_wmem_spl_free_cnt;
  u16 remote_port;
  u16 local_port;
  spl_ip_addr_t remote_ip;
  spl_ip_addr_t local_ip;
  spl_netconn_type_t type;
  spl_tcp_state_t tcp_state;

  /* for recycle */
  netconn_recycle recycle;
  nsfw_res res_chk;
  i64 extend_member_bit;
} spl_netconn_t;                /* sizeof(spl_netconn_t) need < SS_NETCONN_SIZE */
SIZE_OF_TYPE_PLUS8_NOT_LARGER_THAN (spl_netconn_t, SS_NETCONN_SIZE);

typedef struct
{
    /**
     * it will increase after every restart which used for
     * tell different runnings.
     */
  u32 running_ref_no;

  /* only increased when fault to restart */
  u32 fault_ref_no;

  /* the procedures before upgrade are finished successfully */
  u32 prepare_upgrade_done;

} spl_init_info_t;

typedef struct netifExt
{
  struct netifExt *next;
  u16_t id;
  char if_name[HAL_MAX_NIC_NAME_LEN];
  hal_hdl_t hdl;
  u16_t num_packets_recv;
} netifExt;

static inline mring_handle
ss_get_msg_pool (spl_netconn_t * sh)
{
  sbr_recycle_group *group =
    (sbr_recycle_group *) ADDR_SHTOL (sh->recycle.group);
  return ADDR_SHTOL (group->msg_pool);
}

static inline mring_handle
ss_get_conn_pool (spl_netconn_t * sh)
{
  sbr_recycle_group *group =
    (sbr_recycle_group *) ADDR_SHTOL (sh->recycle.group);
  return ADDR_SHTOL (group->conn_pool);
}

static inline int
ss_is_nonblock_flag (spl_netconn_t * sh)
{
  return (sh->flags & SPL_NETCONN_FLAG_NON_BLOCKING) != 0;
}

static inline int
ss_get_nonblock_flag (spl_netconn_t * sh)
{
  return ss_is_nonblock_flag (sh) ? O_NONBLOCK : 0;
}

static inline void
ss_set_nonblock_flag (spl_netconn_t * sh, u32 flag)
{
  if (flag)
    {
      sh->flags |= SPL_NETCONN_FLAG_NON_BLOCKING;
    }
  else
    {
      sh->flags &= ~SPL_NETCONN_FLAG_NON_BLOCKING;
    }
}

static inline int
ss_get_last_errno (spl_netconn_t * sh)
{
  return sh->last_err;
}

static inline i32
ss_get_recv_event (spl_netconn_t * sh)
{
  return sh->rcvevent;
}

static inline int
ss_can_not_recv (spl_netconn_t * sh)
{
  return sh->CanNotReceive;
}

static inline int
ss_is_shut_rd (spl_netconn_t * sh)
{
  int status = sh->shut_status;
  return ((SPL_SHUT_RD == status) || (SPL_SHUT_RDWR == status));
}

static inline void
ss_set_shut_status (spl_netconn_t * sh, u16 how)
{
  sh->shut_status = how;
}

static inline u16
ss_get_shut_status (spl_netconn_t * sh)
{
  return sh->shut_status;
}

static inline void
ss_sub_recv_event (spl_netconn_t * sh)
{
  __sync_fetch_and_sub (&sh->rcvevent, 1);
}

static inline void
ss_add_recv_event (spl_netconn_t * sh)
{
  __sync_fetch_and_add (&sh->rcvevent, 1);
}

static inline i64
ss_get_recv_obj (spl_netconn_t * sh)
{
  return sh->recv_obj;
}

static inline i64
ss_get_private_data (spl_netconn_t * sh)
{
  return sh->private_data;
}

static inline i64
ss_get_comm_private_data (spl_netconn_t * sh)
{
  return sh->comm_pcb_data;
}

static inline int
ss_recv_ring_valid (spl_netconn_t * sh)
{
  return sh->recv_ring_valid;
}

static inline mring_handle
ss_get_recv_ring (spl_netconn_t * sh)
{
  return (mring_handle) ADDR_SHTOL (sh->recv_ring);
}

static inline void
ss_set_send_event (spl_netconn_t * sh, u16 value)
{
  sh->sendevent = value;
}

static inline int
ss_is_noautorecved_flag (spl_netconn_t * sh)
{
  return (sh->flags & SPL_NETCONN_FLAG_NO_AUTO_RECVED) != 0;
}

static inline int
ss_get_noautorecved_flag (spl_netconn_t * sh)
{
  return ss_is_noautorecved_flag (sh) ? SPL_NETCONN_FLAG_NO_AUTO_RECVED : 0;
}

static inline void
ss_set_noautorecved_flag (spl_netconn_t * sh, u8 flag)
{
  if (flag)
    {
      sh->flags |= SPL_NETCONN_FLAG_NO_AUTO_RECVED;
    }
  else
    {
      sh->flags &= ~SPL_NETCONN_FLAG_NO_AUTO_RECVED;
    }
}

static inline void
ss_set_state (spl_netconn_t * sh, spl_netconn_state_t state)
{
  sh->state = state;
}

static inline int
ss_is_write_state (spl_netconn_t * sh)
{
  return (SPL_NETCONN_WRITE == sh->state);
}

static inline int
ss_is_listen_state (spl_netconn_t * sh)
{
  return (SPL_NETCONN_LISTEN == sh->state);
}

static inline void
ss_sub_recv_avail (spl_netconn_t * sh, int value)
{
  sh->recv_avail_cons += value;
}

static inline i32
ss_get_recv_avail (spl_netconn_t * sh)
{
  return sh->recv_avail_prod - sh->recv_avail_cons;
}

static inline u16
ss_get_mss (spl_netconn_t * sh)
{
  return sh->mss;
}

static inline spl_ip_addr_t *
ss_get_remote_ip (spl_netconn_t * sh)
{
  return &(sh->remote_ip);
}

static inline u16
ss_get_remote_port (spl_netconn_t * sh)
{
  return sh->remote_port;
}

static inline spl_tcp_state_t
ss_get_tcp_state (spl_netconn_t * sh)
{
  return sh->tcp_state;
}

static inline u16
ss_get_bind_thread_index (spl_netconn_t * sh)
{
  return sh->bind_thread_index;
}

static inline void
ss_set_bind_thread_index (spl_netconn_t * sh, u16 bind_thread_index)
{
  sh->bind_thread_index = bind_thread_index;
}

static inline void
ss_set_msg_box (spl_netconn_t * sh, mring_handle box)
{
  sh->msg_box = (mring_handle) ADDR_LTOSH (box);
}

static inline mring_handle
ss_get_msg_box (spl_netconn_t * sh)
{
  return (mring_handle) ADDR_SHTOL (sh->msg_box);
}

static inline spl_ip_addr_t *
ss_get_local_ip (spl_netconn_t * sh)
{
  return &sh->local_ip;
}

static inline void
ss_set_local_ip (spl_netconn_t * sh, u32 addr)
{
  if (!sh)
    {
      return;
    }
  sh->local_ip.addr = addr;
}

static inline u16
ss_get_local_port (spl_netconn_t * sh)
{
  return sh->local_port;
}

static inline void
ss_set_local_port (spl_netconn_t * sh, u16 local_port)
{
  sh->local_port = local_port;
}

static inline void
ss_set_accept_from (spl_netconn_t * sh, spl_netconn_t * accept_from)
{
  sh->recycle.accept_from = accept_from;
}

static inline void
ss_set_is_listen_conn (spl_netconn_t * sh, i8 is_listen_conn)
{
  sh->recycle.is_listen_conn = is_listen_conn;
}

static inline i32
ss_inc_fork_ref (spl_netconn_t * sh)
{
  return __sync_add_and_fetch (&sh->recycle.fork_ref, 1);
}

static inline i32
ss_dec_fork_ref (spl_netconn_t * sh)
{
  return __sync_sub_and_fetch (&sh->recycle.fork_ref, 1);
}

static inline i32
ss_get_fork_ref (spl_netconn_t * sh)
{
  return sh->recycle.fork_ref;
}

static inline int
ss_add_pid (spl_netconn_t * sh, pid_t pid)
{
  return nsfw_add_pid (&sh->recycle.pid_info, pid);
}

static inline int
ss_del_pid (spl_netconn_t * sh, pid_t pid)
{
  return nsfw_del_pid (&sh->recycle.pid_info, pid);
}

static inline int
ss_is_pid_exist (spl_netconn_t * sh, pid_t pid)
{
  return nsfw_pid_exist (&sh->recycle.pid_info, pid);
}

static inline int
ss_is_pid_array_empty (spl_netconn_t * sh)
{
  return nsfw_pidinfo_empty (&sh->recycle.pid_info);
}

spl_netconn_t *ss_malloc_conn (mring_handle pool, spl_netconn_type_t type);
spl_netconn_t *ss_malloc_conn_app (mring_handle pool,
                                   spl_netconn_type_t type);
void ss_free_conn (spl_netconn_t * conn);
void ss_reset_conn (spl_netconn_t * conn);

typedef void (*ss_close_conn_fun) (void *close_data, u32 delay_sec);
int ss_recycle_conn (void *close_data, ss_close_conn_fun close_conn);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
