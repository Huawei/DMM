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

#ifndef STACKX_SPL_MSG_H
#define STACKX_SPL_MSG_H
#include "nsfw_msg.h"
#include "stackx_ip_addr.h"
#include "stackx_spl_share.h"
#include "stackx_common_opt.h"
#include <sys/socket.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

enum spl_tcpip_msg_type
{
  SPL_TCPIP_NEW_MSG_API,
#if STACKX_TIMER_THREAD_SUPPORT
  SPL_TCPIP_MSG_TIMER,
#endif
#if STACKX_NETIF_API
  SPL_TCPIP_MSG_NETIFAPI,
#endif
  SPL_TCPIP_MSG_CALLBACK,
  SPL_TCPIP_MSG_IP_MODULE,

  SPL_TCPIP_MSG_MAX,            //the number of enum
  SPL_TCPIP_MSG_BOTTOM = MAX_MAJOR_TYPE //the max number of enum
};

enum api_msg_type
{
  SPL_API_DO_NEWCONN = 0,
  SPL_API_DO_DELCON,
  SPL_API_DO_RECV,
  SPL_API_DO_GETADDR,
  SPL_API_DO_BIND,
  SPL_API_DO_CONNECT,
  SPL_API_DO_LISTEN,
  SPL_API_DO_CLOSE,
  SPL_API_DO_SET_SOCK_OPT,
  SPL_API_DO_GET_SOCK_OPT,
  SPL_API_DO_SEND,
  SPL_API_DO_WRITE,
  SPL_API_DO_POST,
  SPL_API_DO_SHOW,
  SPL_API_DO_EXTREN_SOCK_SET,
  SPL_API_DO_ENABLE_TRACE,
  SPL_API_GET_PROTOCOL,
  SPL_API_DO_IOCTL_OPT,
  SPL_API_DO_GETSOCK_NAME,
  SPL_API_DO_DUMP_L4,
  SPL_API_DO_PBUF_FREE,
  SPL_API_DO_APP_TOUCH,         /* app send its version info to nStackMain */

  SPL_API_MSG_MAX,              //the number of enum
  SPL_API_MSG_BOTTOM = MAX_MINOR_TYPE   //the max number of enum
};

/* TODO: move thes to apporpriate file */
#define TCP_RTO_MAX ((unsigned)(120*1000))      /* 120s */
#define TCP_RTO_MIN ((unsigned)(200))   /* 200ms */

#define TCP_TIMEOUT_INIT ((unsigned)(1*1000))   /* RFC6298 2.1 initial RTO value    */
#define CONN_TCP_MEM_MIN_LINE       ((TX_MBUF_POOL_SIZE)/1024)  //conn level : min value of send_buf
#define CONN_TCP_MEM_DEF_LINE  	    ((TX_MBUF_POOL_SIZE)/128)   //conn level : default value of send_buf
#define CONN_TCP_MEM_MAX_LINE       ((TX_MBUF_POOL_SIZE)/12)

#define SOF_DEBUG           0x01U
#define SOF_ACCEPTCONN      0x02U
#define SOF_DONTROUTE       0x10U
#define SOF_USELOOPBACK     0x40U
#define SOF_LINGER          0x80U
#define SOF_OOBINLINE       0x0100U
#define SOF_REUSEPORT       0x0200U

/* General structure for calling APIs*/
typedef struct api_param_t
{
  data_com_msg *m;
  i64 comm_private_data;
} api_param;

/* SPL_API_DO_SEND */
typedef struct
{
  PRIMARY_ADDR struct spl_pbuf *p;
  spl_ip_addr_t local_ip;
  spl_ip_addr_t addr;
  spl_ip_addr_t srcAddr;
  u16 port;
  u16 srcPort;
  u16 toport_chksum;
  u8 tos;
  u8 pad;
  u8 flags;
  i64 extend_member_bit;
} msg_send_buf;

/* SPL_API_DO_BIND */
typedef struct
{
  spl_ip_addr_t ipaddr;
  u16 port;
  i64 extend_member_bit;
} msg_bind;

/* SPL_API_DO_CLOSE */
typedef struct
{
  long time_started;
  u8 shut;
  i64 extend_member_bit;
} msg_close;

/* SPL_API_DO_CONNECT */
typedef struct
{
  spl_ip_addr_t local_ip;
  spl_ip_addr_t ipaddr;
  u16 port;
  i64 extend_member_bit;
} msg_connect;

/* SPL_API_DO_DELCON */
typedef struct
{
  PRIMARY_ADDR struct spl_netconn *conn;
  PRIMARY_ADDR struct spl_pbuf *buf;
  long time_started;
  pid_t pid;
  u8 shut;
  u8 msg_box_ref;               /* use it for priority */
  u8 notify_omc;                /* bool */
  i64 extend_member_bit;
} msg_delete_netconn;

/* SPL_API_DO_WRITE */
typedef struct msg_write_buf_T
{
  PRIMARY_ADDR struct spl_pbuf *p;
  struct msg_write_buf_T *next;
  size_t len;
  u8 apiflags;
  i64 extend_member_bit;
} msg_write_buf;

/* SPL_API_DO_LISTEN */
typedef struct
{
  mring_handle conn_pool;
  u8 backlog;
  i64 extend_member_bit;
} msg_listen;

/* SPL_API_DO_NEWCONN */
typedef struct
{
  PRIMARY_ADDR struct spl_netconn *conn;
  spl_netconn_type_t type;
  u8 proto;
  int socket;
  i64 extend_member_bit;
} msg_new_netconn;

/* SPL_API_DO_RECV */
typedef struct
{
  PRIMARY_ADDR struct spl_pbuf *p;
  u32 len;
  i64 extend_member_bit;
} msg_recv_buf;

/* SPL_API_DO_GETSOCK_NAME */
typedef struct
{
  struct sockaddr sock_addr;
  u8 cmd;
  i64 extend_member_bit;
} msg_getaddrname;

/* SPL_API_DO_GET_SOCK_OPT,SPL_API_DO_SET_SOCK_OPT */
typedef struct
{
  int level;
  int optname;
  PRIMARY_ADDR mring_handle msg_box;    /* IP_TOS, spl will get new msg box for app */
  union
  {
    int int_optval;
    //struct in_addr inaddr_optval;
    //struct linger _linger;
    struct timeval timeval_optval;
    //ipmreq ipmreq_optval;  //Multicast support later
  } optval;

  u32 optlen;
  i64 extend_member_bit;
} msg_setgetsockopt;

/* SPL_API_DO_PBUF_FREE */
typedef struct
{
  PRIMARY_ADDR struct spl_pbuf *buf;
  i64 extend_member_bit;
} msg_free_buf;

/* app send its version info to nStackMain */
#define NSTACK_VERSION_LEN 128
/* SPL_API_DO_APP_TOUCH */
typedef struct
{
  u32_t hostpid;
  char app_version[NSTACK_VERSION_LEN];
} msg_app_touch;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
