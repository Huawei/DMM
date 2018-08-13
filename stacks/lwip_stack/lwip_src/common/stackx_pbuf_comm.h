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

#ifndef STACKX_PBUF_COMM_H
#define STACKX_PBUF_COMM_H
#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define SPL_PBUF_TRANSPORT_HLEN 20
#define SPL_PBUF_UDP_HLEN 8
#define SPL_PBUF_IP_HLEN 20

#define SPL_PBUF_VLAN_HLEN	0

#ifndef SPL_PBUF_LINK_HLEN
#define SPL_PBUF_LINK_HLEN (14 + SPL_PBUF_VLAN_HLEN)
#endif

typedef enum spl_pbuf_layer
{
  SPL_PBUF_TRANSPORT,
  SPL_PBUF_UDP,
  SPL_PBUF_IP,
  SPL_PBUF_LINK,
  SPL_PBUF_RAW,
  SPL_PBUF_MAX_LAYER,
} spl_pbuf_layer;

typedef enum spl_pbuf_type
{
  SPL_PBUF_RAM,                 /* pbuf data is stored in RAM */
  SPL_PBUF_ROM,                 /* pbuf data is stored in ROM */
  SPL_PBUF_REF,                 /* pbuf comes from the pbuf pool */
  SPL_PBUF_POOL,                /* pbuf payload refers to RAM */
  SPL_PBUF_HUGE                 /* pbuf is stored in HugePage memory in struct common_mem_mbuf */
} spl_pbuf_type;

enum spl_pbuf_proto
{
  SPL_PBUF_PROTO_NONE,
  SPL_PBUF_TCP_SEND,
  SPL_PBUF_TCP_RECV
} spl_pbuf_proto;

typedef struct spl_pbuf
{
    /** next pbuf in singly linked pbuf chain */
  union
  {
    struct spl_pbuf *next;
    u64 next_a;
  };

    /** pointer to the actual data in the buffer */
  union
  {
    void *payload;
    u64 payload_a;
  };

  union
  {
    void *msg;
    u64 msg_a;
  };

  union
  {
    void *conn;
    u64 conn_a;
  };

  struct spl_pbuf *freeNext;    // This pointer will point to next pbuf you want to free in stackx

  u32 tot_len;
  u32 len;
  int proto_type;
  u16 ref;
  u8 type;
  u8 flags;
  nsfw_res res_chk;

} spl_pbuf;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
