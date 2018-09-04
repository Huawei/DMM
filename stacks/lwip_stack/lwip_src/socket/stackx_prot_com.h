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

#ifndef STACKX_PROT_COM_H
#define STACKX_PROT_COM_H
#include "stackx_socket.h"
#include "stackx_ip_addr.h"
#include "stackx_res_mgr.h"
#include "stackx_pbuf.h"
#include "stackx_macro.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define MAX_RECV_FREE_BUF 32
#define MAX_RECV_FREE_LEN (SPL_FRAME_MTU - SPL_IP_HLEN)
#define USEC_TO_SEC 1000000

#ifndef NSTACK_SOCKOPT_CHECK
#define NSTACK_SOCKOPT_CHECK
/* setsockopt level type*/
enum
{
  NSTACK_SOCKOPT = 0xff02
};
/*setsockopt optname type*/
enum
{
  NSTACK_SEM_SLEEP = 0X001
};
#endif

#define sbr_malloc_tx_pbuf(len, offset) sbr_malloc_pbuf(sbr_get_tx_pool(), len, TX_MBUF_MAX_LEN, offset)

static inline int
sbr_spl_err_to_errno (i32 err)
{
  static int table[] = {
    0,                          /* ERR_OK             0      No error, everything OK.    */
    ENOMEM,                     /* ERR_MEM            -1      Out of memory error.       */
    ENOBUFS,                    /* ERR_BUF            -2      Buffer error.              */
    ETIMEDOUT,                  /* ERR_TIMEOUT        -3      Timeout                    */
    EHOSTUNREACH,               /* ERR_RTE            -4      Routing problem.           */
    EINPROGRESS,                /* ERR_INPROGRESS     -5      Operation in progress      */
    EINVAL,                     /* ERR_VAL            -6      Illegal value.             */
    EWOULDBLOCK,                /* ERR_WOULDBLOCK     -7      Operation would block.     */
    EADDRINUSE,                 /* ERR_USE            -8      Address in use.            */
    EISCONN,                    /* ERR_ISCONN         -9      Already connected.         */
    ECONNABORTED,               /* ERR_ABRT           -10     Connection aborted.        */
    ECONNRESET,                 /* ERR_RST            -11     Connection reset.          */
    ENOTCONN,                   /* ERR_CLSD           -12     Connection closed.         */
    ENOTCONN,                   /* ERR_CONN           -13     Not connected.             */
    EIO,                        /* ERR_ARG            -14     Illegal argument.          */
    -1,                         /* ERR_IF             -15     Low-level netif error      */
    EALREADY,                   /*ERR_ALREADY         -16     previous connect attemt has not yet completed */
    EPROTOTYPE,                 /*ERR_PROTOTYPE       -17     prototype error or some other generic error.
                                   the operation is not allowed on current socket              */
    EINVAL,                     /* ERR_CALLBACK       -18     callback error             */
    EADDRNOTAVAIL,              /* ERR_CANTASSIGNADDR -19 Cannot assign requested address */
    EIO,                        /* ERR_CONTAINER_ID  -20 Illegal container id           */
    ENOTSOCK,                   /*ERR_NOTSOCK         -21 not a socket                   */
    -1,                         /* ERR_CLOSE_WAIT     -22 closed in established state */
    EPROTONOSUPPORT,            /* ERR_EPROTONOSUPPORT -23 Protocol not supported */
    ECONNABORTED                /* ERR_FAULTRECOVERY   -24 SPL just recovered from a fatal fault */
  };

  if (((-(err)) >= 0)
      && (-(err)) < (i32) (sizeof (table) / sizeof (table[0])))
    {
      return table[-(err)];
    }
  else
    {
      return EIO;
    }
}

int sbr_getsockopt_sol_socket (sbr_socket_t * sk, int optname, void *optval,
                               socklen_t optlen);
int sbr_getsockopt_ipproto_ip (int optname, void *optval, socklen_t optlen);
int sbr_setsockopt_sol_socket (sbr_socket_t * sk, int optname,
                               const void *optval, socklen_t optlen,
                               spl_netconn_type_t type);
int sbr_setsockopt_ipproto_ip (int optname, const void *optval,
                               socklen_t optlen, spl_netconn_type_t type);
int sbr_dequeue_buf (sbr_socket_t * sk, void **buf, i32 timeout);
int sbr_com_peak (sbr_socket_t * sk);
int sbr_com_try_lock_recv (sbr_socket_t * sk);
/*****************************************************************************
*   Prototype    : sbr_com_lock_recv
*   Description  : lock recv
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
sbr_com_lock_recv (sbr_socket_t * sk)
{
#ifdef SBR_USE_LOCK
  while (!common_spinlock_try_lock_with_pid
         (&sbr_get_fd_share (sk)->recv_lock, get_sys_pid ()))
    {
      sys_sleep_ns (0, 0);
    }
#endif

}

/*****************************************************************************
*   Prototype    : sbr_com_unlock_recv
*   Description  : unlock recv
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
sbr_com_unlock_recv (sbr_socket_t * sk)
{
#ifdef SBR_USE_LOCK
  common_spinlock_unlock (&sbr_get_fd_share (sk)->recv_lock);
#endif
}

void sbr_com_lock_common (sbr_socket_t * sk);
void sbr_com_unlock_common (sbr_socket_t * sk);
void sbr_com_free_recv_buf (sbr_socket_t * sk, struct spl_pbuf *p);
int sbr_get_sockaddr_and_len (u16 port, spl_ip_addr_t * ip_addr,
                              struct sockaddr *addr, socklen_t * addrlen);
void sbr_com_set_app_info (sbr_socket_t * sk, void *appinfo);

void sbr_com_fork_parent (sbr_socket_t * sk, pid_t p);
void sbr_com_fork_child (sbr_socket_t * sk, pid_t p, pid_t c);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
