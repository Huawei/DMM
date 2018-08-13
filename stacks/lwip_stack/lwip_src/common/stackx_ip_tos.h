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

#ifndef STACKX_IP_TOS_H
#define STACKX_IP_TOS_H
#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
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
 * performance on another.	Except for very unusual cases at most two
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

#define IPTOS_RT_MASK (IPTOS_TOS_MASK & ~3)

#define TC_PRIO_BESTEFFORT 0
#define TC_PRIO_FILLER 1
#define TC_PRIO_BULK 2
#define TC_PRIO_INTERACTIVE_BULK 4
#define TC_PRIO_INTERACTIVE 6
#define TC_PRIO_CONTROL 7
#define TC_PRIO_MAX 15

#define ECN_OR_COST(class ) TC_PRIO_ ## class

static u8 stackx_ip_tos2prio[(IPTOS_TOS_MASK >> 1) + 1] = {
  TC_PRIO_BESTEFFORT,
  ECN_OR_COST (FILLER),
  TC_PRIO_BESTEFFORT,
  ECN_OR_COST (BESTEFFORT),
  TC_PRIO_BULK,
  ECN_OR_COST (BULK),
  TC_PRIO_BULK,
  ECN_OR_COST (BULK),
  TC_PRIO_INTERACTIVE,
  ECN_OR_COST (INTERACTIVE),
  TC_PRIO_INTERACTIVE,
  ECN_OR_COST (INTERACTIVE),
  TC_PRIO_INTERACTIVE_BULK,
  ECN_OR_COST (INTERACTIVE_BULK),
  TC_PRIO_INTERACTIVE_BULK,
  ECN_OR_COST (INTERACTIVE_BULK)
};

static inline char
stackx_rt_tos2priority (u8 tos)
{
  return stackx_ip_tos2prio[IPTOS_TOS (tos) >> 1];
}

typedef enum
{
  STACKX_PRIM,
  STACKX_HIGH,
  STACKX_MEDIUM,
  STACKX_LOW
} stackx_prio;

static inline stackx_prio
stackx_get_prio (u8 tos)
{
  if (0 == tos)
    {
      return STACKX_PRIM;
    }
  char prio = stackx_rt_tos2priority (tos);
  if ((TC_PRIO_INTERACTIVE == prio))
    {
      return STACKX_HIGH;
    }
  else if ((TC_PRIO_BESTEFFORT == prio) || (TC_PRIO_INTERACTIVE_BULK == prio))
    {
      return STACKX_MEDIUM;
    }
  else if ((TC_PRIO_BULK == prio) || (TC_PRIO_FILLER == prio))
    {
      return STACKX_LOW;
    }

  return STACKX_PRIM;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
