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

#ifndef __STACKX_SOCKETS_H__
#define __STACKX_SOCKETS_H__

#include <errno.h>
#include <stddef.h>             /* for size_t */
#include "arch/sys_arch.h"
#include "sys/socket.h"
#include <errno.h>
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#ifndef socklen_t
#define socklen_t u32_t
#endif

#define SET_STACKP_ERRNO(err)	(errno = (err)) //thread-local errno

#ifndef set_errno
#define set_errno(err) 	SET_STACKP_ERRNO(err)
#endif

#define sock_set_errno(sk, e) do { \
    (sk)->err = (e); \
    if ((sk)->err != 0) \
        set_errno((sk)->err); \
    } while (0)

#ifndef __BITS_SOCKET_H
/* Socket protocol types (TCP/UDP/RAW) */
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
/*
 * Option flags per-socket. These must match the SOF_ flags in ip.h (checked in init.c)
 */
#define  SO_DEBUG 0x0001        /* Unimplemented: turn on debugging info storing */
#define  SO_REUSEADDR  0x0002
#endif

#define  SO_DONTROUTE 5         //0x0010  /* Unimplemented: just use interface addresses */
#define  SO_BROADCAST 6         //0x0020  /* permit to send and to receive broad cast messages*/
#define  SO_KEEPALIVE    9      //0x0008 gaussdb  /* keep connections alive */
#define  SO_OOBINLINE 10        //0x0100  /* Unimplemented: leave received OOB data in line */
#define  SO_LINGER 13           //0x0080     /* linger on close if data present */
#define  SO_REUSEPORT 15        /* Unimplemented: allow local address & port reuse */
#define  SO_ACCEPTCONN 30       //0x0002 /* socket has had listen() */
#define  SO_USELOOPBACK 0x0040  /* Unimplemented: bypass hardware when possible */

#define SO_DONTLINGER ((int)(~SO_LINGER))

/*
 * Additional options, not kept in so_options.
 */
#define SO_TYPE 3               /* get socket type */
#define SO_ERROR 4
#define SO_SNDBUF  7            /* send buffer size */
#define SO_RCVBUF  8            /* receive buffer size */
#define SO_NO_CHECK 11          /* don't create UDP checksum */
#define SO_RCVLOWAT 18          /* receive low-water mark */
#define SO_SNDLOWAT 19          /* send low-water mark */
#define SO_RCVTIMEO 20          /* receive timeout */
#define SO_SNDTIMEO 21          /* Unimplemented: send timeout */

#define SO_CONTIMEO 0x1009      /* Unimplemented: connect timeout */

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define  SOL_SOCKET  1          //0xfff

#ifndef __BITS_SOCKET_H

#define AF_UNSPEC 0
#define PF_UNSPEC AF_UNSPEC
#define AF_INET 2
#define PF_INET AF_INET

#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_UDPLITE 136

#define IPPROTO_IP 0

#define MSG_PEEK 0x02
#define MSG_WAITALL 0x100
#define MSG_OOB 0x01
#define MSG_DONTWAIT 0x40
#define MSG_MORE 0x8000
#endif

#define IP_TOS 1
#define IP_TTL 2

#define RCV_WND 0x21
#define RCV_ANN_WND 0x22
#define INIT_CWND 0x23
#define THRESHOLD_FACTOR 0x24
#define TMR_INTERVAL 0x25

/*
 * Options and types for UDP multicast traffic handling
 */
#ifndef __BITS_SOCKET_H
#define IP_ADD_MEMBERSHIP 3
#define IP_DROP_MEMBERSHIP 4
#define IP_MULTICAST_TTL 5
#define IP_MULTICAST_IF 6
#define IP_MULTICAST_LOOP 7
#endif

/*
 * The Type of Service provides an indication of the abstract
 * parameters of the quality of service desired.  These parameters are
 * to be used to guide the selection of the actual service parameters
 * when transmitting a datagram through a particular network.  Several
 * networks offer service precedence, which somehow treats high
 * precedence traffic as more important than other traffic (generally
 * by accepting only traffic above a certain precedence at time of high
 * load).  The major choice is a three way tradeoff between low-delay,
 * high-reliability, and high-throughput.
 * The use of the Delay, Throughput, and Reliability indications may
 * increase the cost (in some sense) of the service.  In many networks
 * better performance for one of these parameters is coupled with worse
 * performance on another.  Except for very unusual cases at most two
 * of these three indications should be set.
 */
#define IPTOS_LOWCOST 0x02
#define IPTOS_RELIABILITY 0x04
#define IPTOS_THROUGHPUT 0x08
#define IPTOS_LOWDELAY 0x10
#define IPTOS_TOS_MASK 0x1E
#define IPTOS_MINCOST IPTOS_LOWCOST
#define IPTOS_TOS(tos) ((tos) & IPTOS_TOS_MASK)

/*
 * The Network Control precedence designation is intended to be used
 * within a network only.  The actual use and control of that
 * designation is up to each network. The Internetwork Control
 * designation is intended for use by gateway control originators only.
 * If the actual use of these precedence designations is of concern to
 * a particular network, it is the responsibility of that network to
 * control the access to, and use of, those precedence designations.
 */
#define IPTOS_PREC_ROUTINE 0x00
#define IPTOS_PREC_PRIORITY 0x20
#define IPTOS_PREC_IMMEDIATE 0x40
#define IPTOS_PREC_FLASH 0x60
#define IPTOS_PREC_FLASHOVERRIDE 0x80
#define IPTOS_PREC_CRITIC_ECP 0xa0
#define IPTOS_PREC_INTERNETCONTROL 0xc0
#define IPTOS_PREC_MASK 0xe0
#define IPTOS_PREC_NETCONTROL 0xe0
#define IPTOS_PREC(tos) ((tos) & IPTOS_PREC_MASK)

#if !defined (FIONREAD) || !defined (FIONBIO)
#define IOC_VOID 0x20000000UL   /* no parameters */
#define IOC_OUT 0x40000000UL    /* copy out parameters */
#define IOC_IN 0x80000000UL     /* copy in parameters */
#define IOCPARM_MASK 0x7fU      /* parameters must be < 128 bytes */
#define IOC_INOUT (IOC_IN | IOC_OUT)    /* 0x20000000 distinguishes new & old ioctl's */

#define _IO(x, y) (((x) << 8) | (y) |IOC_VOID )

#define _IOR(x, y, t) (IOC_OUT | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))

#define _IOW(x, y, t) (IOC_IN | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#endif /* !defined(FIONREAD) || !defined(FIONBIO) */

#ifndef FIONREAD
#define FIONREAD _IOR('f', 127, unsigned long)
#endif
#ifndef FIONBIO
#define FIONBIO _IOW('f', 126, unsigned long)
#endif

/*unimplemented */
#ifndef SIOCSHIWAT
#define SIOCSHIWAT _IOW('s', 0, unsigned long)
#define SIOCGHIWAT _IOR('s', 1, unsigned long)
#define SIOCSLOWAT _IOW('s', 2, unsigned long)
#define SIOCGLOWAT _IOR('s', 3, unsigned long)
#ifndef __BITS_SOCKET_H
#define SIOCATMARK _IOR('s', 7, unsigned long)
#endif
#endif

/* commands for fnctl */
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif

/* File status flags and file access modes for fnctl,
   these are bits in an int. */
#ifndef O_NONBLOCK
#define O_NONBLOCK 0X800        /* nonblocking I/O */
#endif
#ifndef O_NDELAY
#define O_NDELAY O_NONBLOCK     /* same as O_NONBLOCK, for compatibility */
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC   0x80000     /* set close_on_exec */
#endif
#ifndef __BITS_SOCKET_H
#define SOCK_CLOEXEC    O_CLOEXEC
#endif
#ifndef FD_CLOEXEC
#define FD_CLOEXEC  1
#endif
#ifndef SHUT_RD
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#endif

struct pollfd
{

  int fd;                       /* file descriptor */
  short events;                 /* wait event */
  short revents;                /* actual event happened */
};

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __STACKX_SOCKETS_H__ */
