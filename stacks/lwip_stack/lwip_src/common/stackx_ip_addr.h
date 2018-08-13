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

#ifndef STACKX_IP_ADDR_H
#define STACKX_IP_ADDR_H
#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* This is the aligned version of ip_addr_t,
   used as local variable, on the stack, etc. */
typedef struct spl_ip_addr
{
  u32 addr;
} spl_ip_addr_t;

/** 127.0.0.1 */
#define IPADDR_LOOPBACK ((u32_t)0x7f000001UL)

/** 0.0.0.0 */
#define IPADDR_ANY ((u32_t)0x00000000UL)

/** 255.255.255.255 */
#define IPADDR_BROADCAST ((u32_t)0xffffffffUL)

/** 255.255.255.255 */
#define SPL_IPADDR_NONE ((u32)0xffffffffUL)

/** Set address to IPADDR_ANY (no need for spl_htonl()) */
#define spl_ip_addr_set_any(ipaddr) ((ipaddr)->addr = IPADDR_ANY)

/** Set complete address to zero */
#define spl_ip_addr_set_zero(ipaddr) ((ipaddr)->addr = 0)

/** IPv4 only: set the IPaddress given as an u32_t */
#define ip4_addr_set_u32(dest_ipaddr, src_u32) ((dest_ipaddr)->addr = (src_u32))

/** IPv4 only: get the IPaddress as an u32_t */
#define ip4_addr_get_u32(src_ipaddr) ((src_ipaddr)->addr)

#define inet_addr_to_ipaddr(target_ipaddr, source_inaddr) (ip4_addr_set_u32(target_ipaddr, (source_inaddr)->s_addr))
#define inet_addr_from_ipaddr(target_inaddr, source_ipaddr) ((target_inaddr)->s_addr = ip4_addr_get_u32(source_ipaddr))

/** For backwards compatibility */
#define spl_ip_ntoa(ipaddr) spl_ipaddr_ntoa(ipaddr)

u32 spl_ipaddr_addr (const char *cp);
int spl_ipaddr_aton (const char *cp, spl_ip_addr_t * addr);

/** returns ptr to static buffer; not reentrant! */
char *spl_ipaddr_ntoa (const spl_ip_addr_t * addr);
char *spl_ipaddr_ntoa_r (const spl_ip_addr_t * addr, char *buf, int buflen);

#define spl_inet_addr(cp) spl_ipaddr_addr(cp)
#define spl_inet_aton(cp, paddr) spl_ipaddr_aton(cp, (spl_ip_addr_t*)(paddr))
#define spl_inet_ntoa(paddr) spl_ipaddr_ntoa((spl_ip_addr_t*)&(paddr))
#define spl_inet_ntoa_r(addr, buf, buflen) spl_ipaddr_ntoa_r((spl_ip_addr_t*)&(addr), buf, buflen)

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
