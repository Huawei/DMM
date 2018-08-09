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

/*****************************************************************************
*   Prototype    : ifndef _NSFW_MGRCOM_MODULE_H
*   Description  : mgr com module definition
*   Input        : None
*   Output       : None
*   Return Value : #
*   Calls        :
*   Called By    :
 *****************************************************************************/
#ifndef _NSFW_MGRCOM_MODULE_H
#define _NSFW_MGRCOM_MODULE_H

#include "pthread.h"
#include "nsfw_mem_api.h"
#include "common_mem_api.h"
#include "common_mem_memzone.h"
#include "common_func.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSFW_MGRCOM_PATH_LEN 128
#define NSFW_MGRCOM_MAX_SOCKET 1024
#define NSFW_MGRCOM_MAX_PROC_FUN 8

#define MAX_RECV_BUF_DEF         0x34000*2

#define MGR_COM_MSG_COUNT_DEF 1023      /*g_mgr_com_cfg */
#define MGR_COM_RECV_TIMEOUT_DEF 5
#define MGR_COM_MAX_DROP_MSG_DEF 1024

#define MGR_COM_MSG_COUNT       (g_mgr_com_cfg.msg_size)
#define MGR_COM_RECV_TIMEOUT    (g_mgr_com_cfg.max_recv_timeout)
#define MGR_COM_MAX_DROP_MSG    (g_mgr_com_cfg.max_recv_drop_msg)

#define NSFW_MAIN_FILE "nStackMainMgr"
#define NSFW_MASTER_FILE "nStackMasterMgr"
#define NSFW_ALARM_FILE "/HuskyAlarm.domain"

#define NSFW_MGRCOM_THREAD "nStackMgrCom"

typedef struct _nsfw_mgr_init_cfg
{
  u8 proc_type;                 /*fw_poc_type */
  u8 max_recv_timeout;
  u16 max_recv_drop_msg;
  u32 msg_size;
  common_mem_atomic32_t cur_idx;
  u64 u64reserve;
  mring_handle msg_pool;
  char domain_path[NSFW_MGRCOM_PATH_LEN];
} nsfw_mgr_init_cfg;

typedef struct _nsfw_mgrcom_stat
{
  u64 msg_send[MGR_MSG_MAX];
  u64 msg_recv[MGR_MSG_MAX];
  u64 recv_drop[MGR_MSG_MAX];
  u64 msg_alloc;
  u64 msg_free;
  u64 msg_send_failed;
  u64 reconnect_count;
} nsfw_mgrcom_stat;

typedef struct _nsfw_mgr_sock_info
{
  u8 proc_type;       /*_ns_poc_type*/
  u32 host_pid;
  common_mem_spinlock_t opr_lock;
} nsfw_mgr_sock_info;

typedef struct _nsfw_mgr_sock_map
{
  i32 proc_cache[NSFW_PROC_MAX];
  nsfw_mgr_sock_info *sock;
} nsfw_mgr_sock_map;

#define NSFW_SOCK_MAX_PROC_FUN 4

typedef struct _nsfw_mgrcom_proc_fun
{
  i32 fd;
  nsfw_mgr_sock_fun fun;
} nsfw_mgrcom_proc_fun;

typedef struct _nsfw_mgrcom_proc
{
  i32 epfd;
  u32 hbt_count;
  pthread_t ep_thread;
  nsfw_mgr_sock_fun *ep_fun;
} nsfw_mgrcom_proc;

i32 nsfw_set_sock_block (i32 sock, u8 flag);

u8 nsfw_rmv_sock_from_ep (i32 fd);
u8 nsfw_add_sock_to_ep (i32 fd);

int nsfw_mgr_new_msg (i32 epfd, i32 socket, u32 events);

u8 nsfw_mgr_ep_start ();
u8 nsfw_mgr_stop ();

#define LOCK_MGR_FD(_fd){\
    if ((i32)NSFW_MGR_FD_MAX > _fd)\
    {\
        common_mem_spinlock_lock(&g_mgr_socket_map.sock[_fd].opr_lock);\
    }\
}

#define UNLOCK_MGR_FD(_fd){\
    if ((i32)NSFW_MGR_FD_MAX > _fd)\
    {\
        common_mem_spinlock_unlock(&g_mgr_socket_map.sock[_fd].opr_lock);\
    }\
}

#define NSFW_MGR_FD_MAX   g_mgr_sockfdmax

extern void set_thread_attr (pthread_attr_t * pattr, int stacksize, int pri,
                             int policy);

extern void nsfw_com_attr_set (int policy, int pri);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSFW_MGRCOM_MODULE_H  */
