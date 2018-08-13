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

#ifndef _TOOL_COMMON_H_
#define _TOOL_COMMON_H_

#include <time.h>
#include "types.h"

#ifndef NSTACK_STATIC
#ifndef NSTACK_STATIC_CHECK
#define NSTACK_STATIC static
#else
#define NSTACK_STATIC
#endif
#endif

#ifndef IP_ADDR_LEN
#define IP_ADDR_LEN 16
#endif

#ifndef MAC_ADDR_LEN
#define MAC_ADDR_LEN 6
#endif

#ifndef MAC_ADDR_STR_LEN
#define MAC_ADDR_STR_LEN 17
#endif

#define ICMP_ECHO 8
#define ICMP_REPLY 0
#define MS_TO_NS 1000000
#define US_TO_NS 1000
#define NPING_RETRY_COUNT 1000
#define MAX_NPING_RETRY_COUNT 20000

#define MAX_PORT_STR_LEN 5
#define MAX_IP_STR_LEN 15
#define MAX_INTEGER_STR_LEN 10

#define DUMP_HBT_TIMER 1

#define INVALID_DIRECTION 0xFFFF
#define DEFAULT_DUMP_COUNT 1000

#ifndef CUSTOM_SOCK_TYPE
#define CUSTOM_SOCK_TYPE 0xF001
#endif

enum DUMP_ERR_CODE
{
  RET_OK = 0,
  INPUT_INVALID = 1,
  FRAMEWORK_INIT_FAILED = 2,
  MEMPOOL_INIT_FAILED = 3,
  MEMRING_INIT_FAILED = 4,
  START_TASK_FAILED = 5,
  START_TIMER_FAILED = 6,
  UNKNOW_ERR
};

enum COND_LOCAL_REMOTE_SET
{
  COND_NOT_SET = 0,
  COND_REMOTE_SET = 0x1,
  COND_LOCAL_SET = 0x2,
  COND_AND_SET = 0x3,
  COND_OR_SET = 0x4
};

enum DUMP_OPT_ARG
{
  OPT_ARG_HOST = 256,
  OPT_ARG_LOCAL_HOST,
  OPT_ARG_REMOTE_HOST,
  OPT_ARG_PORT,
  OPT_ARG_LOCAL_PORT,
  OPT_ARG_REMOTE_PORT,
  OPT_ARG_MAC,
  OPT_ARG_LOCAL_MAC,
  OPT_ARG_REMOTE_MAC,
  OPT_ARG_INVALID
};

typedef struct _ip_head
{
  u8 ihl:4;
  u8 version:4;
  u8 tos;
  u16 tot_len;
  u16 id;
  u16 frag_off;
  u8 ttl;
  u8 protocol;
  u16 chk_sum;
  u32 local_ip;
  u32 remote_ip;
} ip_head;

typedef struct _tcp_head
{
  u16 src_port;
  u16 dst_port;
  u32 seq_no;
  u32 ack_no;
} tcp_head;

typedef struct _udp_head
{
  u16 src_port;
  u16 dst_port;
  u16 uhl;
  u16 chk_sum;
} udp_head;

typedef struct _dump_file_head
{
  u32 magic;
  u16 major_ver;
  u16 minor_ver;
  u32 area;
  u32 time_stamp;
  u32 max_pack_size;
  u32 link_type;
} dump_file_head;

typedef struct _packet_head
{
  u32 sec;
  u32 usec;
  u32 save_len;
  u32 org_len;
} packet_head;

typedef struct _ip_addr_bits
{
  u32 addr_bits1;
  u32 addr_bits2;
  u32 addr_bits3;
  u32 addr_bits4;
} ip_addr_bits;

typedef struct _parse_msg_info
{
  u16 l2_protocol;              // ARP/IP/OAM/LACP
  u16 l3_protocol;              // TCP/UDP/ICMP
  u16 local_port;
  u16 remote_port;
  u32 local_ip;
  u32 remote_ip;
  char local_mac[MAC_ADDR_LEN + 1];
  char remote_mac[MAC_ADDR_LEN + 1];

  void *org_msg;
} parse_msg_info;

typedef struct _dump_condition
{
  bool has_condition;
  u32 dump_count;
  u32 dump_time;
  u32 limit_len;
  u16 direction;                //1:send 2:recv 3:send-recv
  u16 l2_protocol;              // ARP/IP/OAM/LACP
  u16 l3_protocol;              // TCP/UDP/ICMP
  u16 port_set_flag;
  u16 port;
  u16 local_port;
  u16 remote_port;
  u16 ip_set_flag;
  u32 ip_addr;
  u32 local_ip;
  u32 remote_ip;
  u16 mac_set_flag;
  char mac_addr[MAC_ADDR_LEN + 1];
  char local_mac[MAC_ADDR_LEN + 1];
  char remote_mac[MAC_ADDR_LEN + 1];

  char *dump_file_name;
} dump_condition;

typedef struct _icmp_head
{
  u8 icmp_type;
  u8 icmp_code;
  u16 icmp_cksum;
  u16 icmp_id;
  u16 icmp_seq;
  u32 timestamp;

  long icmp_sec;
  long icmp_nsec;
} icmp_head;

typedef struct _ning_input_info
{
  i32 send_count;               // total send req
  i32 retry_count;              // retry count for 1 req
  char src_ip[IP_ADDR_LEN];
  char dst_ip[IP_ADDR_LEN];
} input_info;

typedef struct _nping_stat_info
{
  u32 send_seq;
  u32 recv_ok;
  double all_interval;
  double min_interval;
  double max_interval;
  struct timespec start_time;
  struct timespec end_time;
} stat_info;

#ifndef sys_sleep_ns
#define sys_sleep_ns(_s, _ns)\
{ \
    if (_s >= 0 && _ns >= 0) \
    { \
        struct timespec delay, remain; \
        delay.tv_sec = _s; \
        delay.tv_nsec = _ns; \
        while (nanosleep(&delay, &remain) < 0) \
        { \
            delay = remain; \
        } \
    } \
}
#endif

#endif
