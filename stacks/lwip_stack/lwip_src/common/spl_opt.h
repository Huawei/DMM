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

#ifndef __STACKX_OPT_H__
#define __STACKX_OPT_H__

#include "stackxopts.h"
#include "stackx_debug.h"
#include "compiling_check.h"

#define PBUF_VLAN_HLEN	0

//#define SIZEOF_ETH_HDR (14)

#ifndef STACKX_NETIF_API
#define STACKX_NETIF_API 1
#endif

/*
   ------------------------------------
   ---------- Thread options ----------
   ------------------------------------
 */

#define PTIMER_THREAD_NAME "ptimer_thread"

/**
 * TCPIP_THREAD_NAME: The name assigned to the main tcpip thread.
 */
#ifndef TCPIP_THREAD_NAME
#define TCPIP_THREAD_NAME "spl_tcpip_thread"
#endif

#ifndef TCPIP_THREAD_STACKSIZE
#define TCPIP_THREAD_STACKSIZE 0
#endif

#ifndef INT_MAX
#define INT_MAX			2147483647
#endif

#ifndef INT_64_MAX
#define INT_64_MAX			9223372036854775807
#endif

#ifndef RECV_BUFSIZE_DEFAULT
#define RECV_BUFSIZE_DEFAULT INT_MAX
#endif

/*
   ----------------------------------------
   ---------- Statistics options ----------
   ----------------------------------------
 */

/*
   ---------------------------------------
   ---------- Debugging options ----------
   ---------------------------------------
 */

#ifndef STACKX_DBG_TYPES_ON
#define STACKX_DBG_TYPES_ON STACKX_DBG_ON       //ON
#endif

#ifndef INTERRUPT_DEBUG
#define INTERRUPT_DEBUG STACKX_DBG_ON   //ON
#endif

#ifndef TESTSOCKET_DEBUG
#define TESTSOCKET_DEBUG STACKX_DBG_ON  //ON
#endif

#ifndef ETHARP_DEBUG
#define ETHARP_DEBUG STACKX_DBG_ON
#endif

#ifndef NETIF_DEBUG
#define NETIF_DEBUG STACKX_DBG_ON
#endif

#ifndef PBUF_DEBUG
#define PBUF_DEBUG STACKX_DBG_ON
#endif

#ifndef API_LIB_DEBUG
#define API_LIB_DEBUG STACKX_DBG_ON
#endif

#ifndef API_MSG_DEBUG
#define API_MSG_DEBUG STACKX_DBG_ON
#endif

#ifndef SOCKETS_DEBUG
#define SOCKETS_DEBUG STACKX_DBG_ON
#endif

#ifndef NS_EPOLL_DBG
#define NS_EPOLL_DBG STACKX_DBG_ON
#endif

#ifndef ICMP_DEBUG
#define ICMP_DEBUG STACKX_DBG_ON
#endif

#ifndef IGMP_DEBUG
#define IGMP_DEBUG STACKX_DBG_ON
#endif

#ifndef INET_DEBUG
#define INET_DEBUG STACKX_DBG_ON
#endif

#ifndef IP_DEBUG
#define IP_DEBUG STACKX_DBG_ON
#endif

#ifndef IP_REASS_DEBUG
#define IP_REASS_DEBUG STACKX_DBG_ON
#endif

#ifndef RAW_DEBUG
#define RAW_DEBUG STACKX_DBG_ON
#endif

#ifndef MEMP_DEBUG
#define MEMP_DEBUG STACKX_DBG_ON
#endif

#ifndef SYS_DEBUG
#define SYS_DEBUG STACKX_DBG_OFF
#endif

#ifndef TIMERS_DEBUG
#define TIMERS_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_DEBUG
#define TCP_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_TEST_DEBUG
#define TCP_TEST_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_INPUT_DEBUG
#define TCP_INPUT_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_RTO_DEBUG
#define TCP_RTO_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_FR_DEBUG
#define TCP_FR_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_FLOW_CTL_DEBUG
#define TCP_FLOW_CTL_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_CWND_DEBUG
#define TCP_CWND_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_WND_DEBUG
#define TCP_WND_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_OUTPUT_DEBUG
#define TCP_OUTPUT_DEBUG STACKX_DBG_ON
#endif

#ifndef TCP_RST_DEBUG
#define TCP_RST_DEBUG STACKX_DBG_ON
#endif

#ifndef UDP_DEBUG
#define UDP_DEBUG STACKX_DBG_ON
#endif

#ifndef TCPIP_DEBUG
#define TCPIP_DEBUG STACKX_DBG_ON
#endif

#ifndef NEW_RING_DEBUG
#define NEW_RING_DEBUG STACKX_DBG_ON
#endif

#ifndef PACKET_DISPATCH
#define PACKET_DISPATCH 1
#endif

#ifndef NETSTAT_SWITCH
#define NETSTAT_SWITCH 1
#endif

#ifndef DISTRIBUTOR_DEBUG

#define DISTRIBUTOR_DEBUG STACKX_DBG_ON
#endif
#define PBUF_REF_DEBUG STACKX_DBG_ON

#ifndef CONTEXT_TIMER_DEBUG
#define CONTEXT_TIMER_DEBUG STACKX_DBG_OFF
#endif

#if (DISTRIBUTOR_DEBUG == STACKX_DBG_ON)
#define PD_DISTRIBUTOR_DEBUG

#define DISTRIBUTOR_SINGLE
#ifdef DISTRIBUTOR_SINGLE

#ifndef __STACKX_DEBUG_H__
#define STACKX_DBG_OFF  0x80U
#define STACKX_DBG_ON  0x00U
#endif

#ifndef ETHARP_DEBUG
#define ETHARP_DEBUG STACKX_DBG_ON
#endif
#ifndef NETIF_DEBUG
#define NETIF_DEBUG STACKX_DBG_ON
#endif
#ifndef PBUF_DEBUG
#define PBUF_DEBUG STACKX_DBG_ON
#endif
#ifndef API_LIB_DEBUG
#define API_LIB_DEBUG STACKX_DBG_ON
#endif
#ifndef API_MSG_DEBUG
#define API_MSG_DEBUG STACKX_DBG_ON
#endif
#ifndef ICMP_DEBUG
#define ICMP_DEBUG STACKX_DBG_ON
#endif
#ifndef IGMP_DEBUG
#define IGMP_DEBUG STACKX_DBG_ON
#endif
#ifndef INET_DEBUG
#define INET_DEBUG STACKX_DBG_ON
#endif
#ifndef IP_DEBUG
#define IP_DEBUG STACKX_DBG_ON
#endif
#ifndef IP_REASS_DEBUG
#define IP_REASS_DEBUG STACKX_DBG_ON
#endif
#ifndef RAW_DEBUG
#define RAW_DEBUG STACKX_DBG_ON
#endif
#ifndef MEMP_DEBUG
#define MEMP_DEBUG STACKX_DBG_ON
#endif
#ifndef SYS_DEBUG
#define SYS_DEBUG STACKX_DBG_ON
#endif
#ifndef TIMERS_DEBUG
#define TIMERS_DEBUG STACKX_DBG_ON
#endif
#ifndef TCP_DEBUG
#define TCP_DEBUG STACKX_DBG_ON
#endif
#ifndef TCP_TEST_DEBUG
#define TCP_TEST_DEBUG STACKX_DBG_ON
#endif
#ifndef TCP_INPUT_DEBUG
#define TCP_INPUT_DEBUG STACKX_DBG_ON
#endif
#ifndef TCP_FR_DEBUG
#define TCP_FR_DEBUG STACKX_DBG_ON
#endif
#ifndef TCP_RTO_DEBUG
#define TCP_RTO_DEBUG STACKX_DBG_ON
#endif
#ifndef TCP_CWND_DEBUG
#define TCP_CWND_DEBUG STACKX_DBG_ON
#endif
#ifndef TCP_WND_DEBUG
#define TCP_WND_DEBUG STACKX_DBG_ON
#endif
#ifndef TCP_OUTPUT_DEBUG
#define TCP_OUTPUT_DEBUG STACKX_DBG_ON
#endif
#ifndef TCP_RST_DEBUG
#define TCP_RST_DEBUG STACKX_DBG_ON
#endif
#ifndef UDP_DEBUG
#define UDP_DEBUG STACKX_DBG_ON
#endif
#ifndef TCPIP_DEBUG
#define TCPIP_DEBUG STACKX_DBG_ON
#endif

#define SC_DPDK_INFO STACKX_DBG_ON
#define SOCK_INFO    STACKX_DBG_ON
#ifndef STACKX_DBG_OFF
#define STACKX_DBG_OFF  0x00U
#endif

#ifndef NS_EPOLL_DBG
#define NS_EPOLL_DBG STACKX_DBG_ON
#endif

#ifndef DFX_DBG
#define DFX_DBG STACKX_DBG_ON
#endif

#endif
#endif /* DISTRIBUTOR_DEBUG */

#ifndef STACKX_FLOW_CTL
#define STACKX_FLOW_CTL 0
#endif

#endif /* __STACKX_OPT_H__ */
