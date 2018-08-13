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

#ifndef _DUMP_TOOL_H_
#define _DUMP_TOOL_H_

#define IP_PROTO_ICMP 1
#define IP_PROTO_IGMP 2
#define IP_PROTO_UDP 17
#define IP_PROTO_UDPLITE 136
#define IP_PROTO_TCP 6

#ifndef MAC_ADDR_LEN
#define MAC_ADDR_LEN 6
#endif

#ifndef IP_HEAD_LEN
#define IP_HEAD_LEN 20
#endif
#ifndef TCP_HEAD_LEN
#define TCP_HEAD_LEN 20
#endif
#ifndef UDP_HEAD_LEN
#define UDP_HEAD_LEN 8
#endif

#ifndef ICMP_HEAD_LEN
#define ICMP_HEAD_LEN 8
#endif

typedef struct _dump_task_info
{
  u16 task_state;               // 0:off, 1:on
  i16 task_id;
  u32 task_keep_time;
  u32 task_start_sec;
  u32 last_hbt_seq;
  u32 last_hbt_sec;
  void *task_queue;
  void *task_pool;
} dump_task_info;

typedef struct _dump_eth_head
{
  u8 dest_mac[MAC_ADDR_LEN];
  u8 src_mac[MAC_ADDR_LEN];
  u16 eth_type;
} eth_head;

#define ETH_HEAD_LEN sizeof(eth_head)

struct ip_hdr
{
  u16 _v_hl_tos;
  u16 _len;
  u16 _id;
  u16 _offset;
  u8 _ttl;
  u8 _proto;
  u16 _chksum;
  u32 src;
  u32 dest;
};

typedef int (*pack_msg_fun) (void *dump_buf, char *msg_buf, u32 len,
                             u16 direction, void *para);

#endif
