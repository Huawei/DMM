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

#ifndef __LWIP_IP_ADDR_H__
#define __LWIP_IP_ADDR_H__

#include "spl_opt.h"
#include "spl_def.h"
#include "ip_module_api.h"
#include "stackx_ip_addr.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/** ip_addr_t uses a struct for convenience only, so that the same defines can
 * operate both on ip_addr_t as well as on ip_addr_p_t. */
/* Forward declaration to not include netif.h */
struct netif;
#if BYTE_ORDER == BIG_ENDIAN

/** Set an IPaddress given by the four byte-parts */
#define SPL_IP4_ADDR(ipaddr, a, b, c, d) \
    (ipaddr)->addr = ((u32_t)((a) & 0xff) << 24) | \
                     ((u32_t)((b) & 0xff) << 16) | \
                     ((u32_t)((c) & 0xff) << 8) | \
                     (u32_t)((d) & 0xff)
#else

/** Set an IPaddress given by the four byte-parts.
    Little-endian version that prevents the use of htonl. */
#define SPL_IP4_ADDR(ipaddr, a, b, c, d) \
    (ipaddr)->addr = ((u32_t)((d) & 0xff) << 24) | \
                     ((u32_t)((c) & 0xff) << 16) | \
                     ((u32_t)((b) & 0xff) << 8) | \
                     (u32_t)((a) & 0xff)
#endif

/** memcpy-like copying of IPaddresses where addresses are known to be
 * 16-bit-aligned if the port is correctly configured (so a port could define
 * this to copying 2 u16_t's) - no NULL-pointer-checking needed. */
#ifndef IPADDR2_COPY
#define IPADDR2_COPY(dest, src) MEMCPY_S(dest, sizeof(ip_addr_t), src, sizeof(ip_addr_t))
#endif

/** Copy IPaddress - faster than spl_ip_addr_set: no NULL check */
#define spl_ip_addr_copy(dest, src) ((dest).addr = (src).addr)

/** Safely copy one IPaddress to another (src may be NULL) */
#define spl_ip_addr_set(dest, src) ((dest)->addr = \
                                    ((src) == NULL ? 0 : \
                                     (src)->addr))

/** Safely copy one IPaddress to another and change byte order
 * from host- to network-order. */
#define spl_ip_addr_set_hton(dest, src) ((dest)->addr = \
                                         ((src) == NULL ? 0 : \
                                          htonl((src)->addr)))

/** Get the network address by combining host address with netmask */
#define spl_ip_addr_get_network(target, host, netmask) ((target)->addr = ((host)->addr) & ((netmask)->addr))

/**
 * Determine if two address are on the same network.
 *
 * @arg addr1 IPaddress 1
 * @arg addr2 IPaddress 2
 * @arg mask network identifier mask
 * @return !0 if the network identifiers of both address match
 */
#define spl_ip_addr_netcmp(addr1, addr2, mask) (((addr1)->addr & \
                                             (mask)->addr) == \
                                            ((addr2)->addr & \
                                             (mask)->addr))

/*       && ((dest)->addr & tmp->netmask.addr) == (tmp->ip_addr.addr & tmp->netmask.addr) */
/*  add "netif = netif->root; \"*/
#define   ip_net_netif_cmp( dest, netif) ({ \
	netif = netif->root; \
    struct netif* tmp = netif; \
    int find = 0; \
    while(tmp) \
    { \
        if (is_in_same_network((dest)->addr, tmp->ip_addr.addr)) \
        { \
            netif->ip_addr.addr = tmp->ip_addr.addr; \
            netif->netmask.addr = tmp->netmask.addr; \
            find = 1; \
            break; \
        } \
        if (netif->is_out) \
        { \
            break; \
        } \
        tmp = tmp->vnext; \
    } \
    (!!find); \
 })

/* Check if netif match dest , if not , find one and swap  */
/*  add "netif = netif->root; \"*/

#define   ip_addr_netif_cmp_and_swap( dest, pnetif) ({ \
	pnetif = pnetif->root; \
    struct netif* tmp = pnetif; \
    int find = 0; \
    while(tmp) \
    { \
        if ((dest)->addr == tmp->ip_addr.addr) \
        { \
            pnetif->ip_addr.addr = tmp->ip_addr.addr; \
            pnetif->netmask.addr = tmp->netmask.addr; \
            find = 1; \
            break; \
        } \
        if (pnetif->is_out) \
        { \
            break; \
        } \
        tmp = tmp->vnext; \
    } \
    (!!find); \
  })

#define   ip_addr_netif_cmp( dest, pnetif) ({ \
	pnetif = pnetif->root; \
    struct netif* tmp = pnetif; \
    int find = 0; \
    while(tmp) \
    { \
        if ((dest)->addr == tmp->ip_addr.addr) \
        { \
            find = 1; \
            break; \
        } \
        tmp = tmp->vnext; \
    } \
    (!!find); \
  })

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __LWIP_IP_ADDR_H__ */
