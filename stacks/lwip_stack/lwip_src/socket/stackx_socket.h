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

#ifndef STACKX_SOCKET_H
#define STACKX_SOCKET_H
#include "sbr_protocol_api.h"
#include "stackx_spl_share.h"
#include "nstack_log.h"
#include "stackx_pbuf.h"
#include "common_mem_spinlock.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef struct
{
  PRIMARY_ADDR struct spl_pbuf *head;
  PRIMARY_ADDR struct spl_pbuf *tail;
  int totalLen;
} sbr_recvbuf_recoder;

/* need fork and recycle */
typedef struct
{
  common_mem_spinlock_t recv_lock;
  common_mem_spinlock_t common_lock;
  PRIMARY_ADDR void *lastdata;
  u32 lastoffset;
  sbr_recvbuf_recoder recoder;
  i32 recv_timeout;
  i32 send_timeout;
  i32 rcvlowat;
  int err;
  u64 block_polling_time;
  i64 extend_member_bit;
} sbr_fd_share;

/* check sbr_fd_share size */
SIZE_OF_TYPE_NOT_LARGER_THAN (sbr_fd_share, SBR_FD_SIZE);

#define sbr_get_fd_share(sk) ((sbr_fd_share*)sk->sk_obj)

#define sbr_get_conn(sk) ((spl_netconn_t*)sk->stack_obj)

#define sbr_get_msg_box(sk) ss_get_msg_box(sbr_get_conn(sk))

/*****************************************************************************
*   Prototype    : sbr_set_sk_errno
*   Description  : set errno for sk
*   Input        : sbr_socket_t * sk
*                  int err
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
sbr_set_sk_errno (sbr_socket_t * sk, int err)
{
  sbr_get_fd_share (sk)->err = err;
  if (err != 0)
    {
      if (sbr_get_conn (sk))
        {
          NSSBR_LOGERR ("fd=%d,errno=%d,conn=%p,private_data=%p", sk->fd, err,
                        sbr_get_conn (sk),
                        ss_get_private_data (sbr_get_conn (sk)));
        }

      sbr_set_errno (err);
    }
}

/*****************************************************************************
*   Prototype    : sbr_set_sk_io_errno
*   Description  : set errno for sk in send/recv func, in case of too many logs
*   Input        : sbr_socket_t * sk
*                  int err
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
sbr_set_sk_io_errno (sbr_socket_t * sk, int err)
{
  sbr_get_fd_share (sk)->err = err;
  if (err != 0)
    {
      if (sbr_get_conn (sk))
        {
          NSSBR_LOGDBG ("fd=%d,errno=%d,conn=%p,private_data=%p", sk->fd, err,
                        sbr_get_conn (sk),
                        ss_get_private_data (sbr_get_conn (sk)));
        }

      sbr_set_errno (err);
    }
}

/*****************************************************************************
*   Prototype    : sbr_get_sk_errno
*   Description  : get sk's errno
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline int
sbr_get_sk_errno (sbr_socket_t * sk)
{
  return sbr_get_fd_share (sk)->err;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
