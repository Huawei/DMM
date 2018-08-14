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

#ifndef STACKX_COMMON_OPT_H
#define STACKX_COMMON_OPT_H
#include <sys/ioctl.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK 0X800        /* nonblocking I/O */
#endif

#if !defined (FIONREAD) || !defined (FIONBIO)
#define IOC_VOID 0x20000000UL   /* no parameters */
#define IOC_OUT 0x40000000UL    /* copy out parameters */
#define IOC_IN 0x80000000UL     /* copy in parameters */
#define IOCPARM_MASK 0x7fU      /* parameters must be < 128 bytes */
#define IOC_INOUT (IOC_IN | IOC_OUT)    /* 0x20000000 distinguishes new & old ioctl's */
#define _IO(x, y) (((x) << 8) | (y) | IOC_VOID)
#define _IOR(x, y, t) (IOC_OUT | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#define _IOW(x, y, t) (IOC_IN | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#endif

#ifndef FIONREAD
#define FIONREAD _IOR('f', 127, unsigned long)
#endif

#ifndef FIONBIO
#define FIONBIO _IOW('f', 126, unsigned long)
#endif

#ifndef F_GETFL
#define F_GETFL 3
#endif

#ifndef F_SETFL
#define F_SETFL 4
#endif

#ifndef SPL_SHUT_RD
#define SPL_SHUT_RD 0
#define SPL_SHUT_WR 1
#define SPL_SHUT_RDWR 2
#endif

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
#define SPL_NETCONN_FLAG_CHECK_WRITESPACE 0x10

/* For the netconn API, these values are use as a bitmask! */
#define SPL_NETCONN_SHUT_RD 1
#define SPL_NETCONN_SHUT_WR 2
#define SPL_NETCONN_SHUT_RDWR (SPL_NETCONN_SHUT_RD | SPL_NETCONN_SHUT_WR)
#define STACKX_TIMER_THREAD_SUPPORT 1

/* Flags for netconn_write (u8_t) */
#define SPL_NETCONN_NOFLAG 0x00
#define SPL_NETCONN_NOCOPY 0x00
#define SPL_NETCONN_COPY 0x01
#define SPL_NETCONN_MORE 0x02
#define SPL_NETCONN_DONTBLOCK 0x04

#define SPL_TCP_NODELAY 0x01
#define SPL_TCP_KEEPALIVE 0x02
#define SPL_TCP_KEEPIDLE 0x04
#define SPL_TCP_KEEPINTVL 0x05
#define SPL_TCP_KEEPCNT 0x06
#define SPL_TCP_LINGER2 0x08
#define SPL_TCP_DEFER_ACCEPT 0x09

typedef enum spl_netconn_type
{
  SPL_NETCONN_INVALID = 0,
  SPL_NETCONN_TCP = 0x10,
  SPL_NETCONN_UDP = 0x20,
  SPL_NETCONN_UDPLITE = 0x21,
  SPL_NETCONN_UDPNOCHKSUM = 0x22,
  SPL_NETCONN_RAW = 0x40,
} spl_netconn_type_t;

typedef enum spl_netconn_state
{
  SPL_NETCONN_NONE,
  SPL_NETCONN_WRITE,
  SPL_NETCONN_LISTEN,
  SPL_NETCONN_CONNECT,
  SPL_NETCONN_CLOSE,
} spl_netconn_state_t;

typedef enum spl_tcp_state
{
  SPL_CLOSED = 0,
  SPL_LISTEN = 1,
  SPL_SYN_SENT = 2,
  SPL_SYN_RCVD = 3,
  SPL_ESTABLISHED = 4,
  SPL_FIN_WAIT_1 = 5,
  SPL_FIN_WAIT_2 = 6,
  SPL_CLOSE_WAIT = 7,
  SPL_CLOSING = 8,
  SPL_LAST_ACK = 9,
  SPL_TIME_WAIT = 10
} spl_tcp_state_t;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
