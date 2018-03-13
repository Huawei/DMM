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

#ifndef __NSTACK_IP_ADDR_H__
#define __NSTACK_IP_ADDR_H__

#ifndef ip4_addr1_16
#include "nstack_types.h"

/* Get one byte from the 4-byte address */
#define ip4_addr1(ipaddr) (((u8_t*)(ipaddr))[0])        /* ip4_addr1(ipaddr)  */
#define ip4_addr2(ipaddr) (((u8_t*)(ipaddr))[1])        /* ip4_addr2(ipaddr)  */
#define ip4_addr3(ipaddr) (((u8_t*)(ipaddr))[2])        /* ip4_addr3(ipaddr)  */
#define ip4_addr4(ipaddr) (((u8_t*)(ipaddr))[3])        /* ip4_addr4(ipaddr)  */

/* These are cast to u16_t, with the intent that they are often arguments
 * to printf using the U16_F format from cc.h. */
#define ip4_addr1_16(ipaddr) ((u16_t)ip4_addr1(ipaddr)) /*ip4_addr1_16(ipaddr) */
#define ip4_addr2_16(ipaddr) ((u16_t)ip4_addr2(ipaddr)) /*ip4_addr2_16(ipaddr) */
#define ip4_addr3_16(ipaddr) ((u16_t)ip4_addr3(ipaddr)) /*ip4_addr3_16(ipaddr) */
#define ip4_addr4_16(ipaddr) ((u16_t)ip4_addr4(ipaddr)) /*ip4_addr4_16(ipaddr) */
#endif
#endif
