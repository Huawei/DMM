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

#ifndef __LWIP_NETBUF_H__
#define __LWIP_NETBUF_H__

#include "spl_opt.h"
#include "spl_pbuf.h"
#include "spl_ip_addr.h"
#include "common_mem_base_type.h"
#include "common_mem_pal.h"
#include "common_pal_bitwide_adjust.h"
#include "stackx_netbuf.h"
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/** This spl_netbuf has dest-addr/port set */
#define NETBUF_FLAG_DESTADDR 0x01

/** This spl_netbuf includes a checksum */
#define NETBUF_FLAG_CHKSUM 0x02

void spl_netbuf_delete (struct spl_netbuf *buf);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __LWIP_NETBUF_H__ */
