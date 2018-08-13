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

#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <malloc.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include "types.h"
#include "nsfw_init.h"
#include "nsfw_mem_api.h"
#include "nsfw_fd_timer_api.h"
#include "nsfw_maintain_api.h"
#include "nstack_securec.h"

#include "tool_common.h"

NSTACK_STATIC struct option g_dump_long_options[] = {
  {"host", 1, NULL, OPT_ARG_HOST},
  {"lhost", 1, NULL, OPT_ARG_LOCAL_HOST},
  {"rhost", 1, NULL, OPT_ARG_REMOTE_HOST},
  {"port", 1, NULL, OPT_ARG_PORT},
  {"lport", 1, NULL, OPT_ARG_LOCAL_PORT},
  {"rport", 1, NULL, OPT_ARG_REMOTE_PORT},
  {"mac", 1, NULL, OPT_ARG_MAC},
  {"lmac", 1, NULL, OPT_ARG_LOCAL_MAC},
  {"rmac", 1, NULL, OPT_ARG_REMOTE_MAC},
  {0, 0, 0, 0}
};

static char *g_dump_short_options = "c:s:w:y:G:P:T:";

NSTACK_STATIC FILE *g_dump_fp = NULL;
static u32 g_dumped_packet = 0;
NSTACK_STATIC u32 g_captured_packet = 0;
static u32 g_filtered_packet = 0;

NSTACK_STATIC bool g_dump_exit = 0;

static dump_timer_info g_dump_hbt_timer;
NSTACK_STATIC dump_condition g_dump_condition;
static nsfw_mem_name g_dump_mem_ring_info =
  { NSFW_SHMEM, NSFW_PROC_MAIN, DUMP_SHMEM_RIGN_NAME };
static nsfw_mem_name g_dump_mem_pool_info =
  { NSFW_SHMEM, NSFW_PROC_MAIN, DUMP_SHMEM_POOL_NAME };

void
dump_exit ()
{
  g_dump_exit = 1;
}

void
print_help_ntcpdump ()
{
  printf ("usage:ntcpdump \n\
        -c count \n\
        -s len \n\
        -w file \n\
        -y l2_protocol \n\
        -T l3_protocol \n\
        -G dump_seconds \n\
        -P in/out/inout \n\
        --host/rhost/lhost ip_addr \n\
        --port/rport/lport port \n\
        --mac/rmac/lmac mac_addr\n");
}

bool
is_digit_ntcpdump (char *str, size_t src_len)
{
  size_t count = 0;

  if (NULL == str || 0 == *str)
    {
      return FALSE;
    }

  while (*str)
    {
      if (*str > '9' || *str < '0')
        {
          return false;
        }

      str++;
      count++;
    }

  if (count > src_len)
    {
      return false;
    }

  return true;
}

NSTACK_STATIC inline bool
is_ip_addr (const char *addr, ip_addr_bits * addr_info)
{
  u32 ip1;
  u32 ip2;
  u32 ip3;
  u32 ip4;

  if (SSCANF_S (addr, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) != 4)
    {
      return false;
    }

  if ((ip1 & 0xffffff00) || (ip2 & 0xffffff00) || (ip3 & 0xffffff00)
      || (ip4 & 0xffffff00))
    {
      return false;
    }

  addr_info->addr_bits1 = ip1;
  addr_info->addr_bits2 = ip2;
  addr_info->addr_bits3 = ip3;
  addr_info->addr_bits4 = ip4;

  return true;
}

NSTACK_STATIC inline bool
get_ip_addr (const char *addr, u32 * ip_addr)
{
  ip_addr_bits addr_info;
  if (!is_ip_addr (addr, &addr_info))
    {
      return false;
    }

  *ip_addr = ((addr_info.addr_bits1 & 0xFF)
              | (addr_info.addr_bits2 & 0xFF) << 8
              | (addr_info.addr_bits3 & 0xFF) << 16
              | (addr_info.addr_bits4 & 0xFF) << 24);

  return true;
}

NSTACK_STATIC inline bool
get_dump_port (const char *pport, u16 * port)
{
  int user_port;

  if (!is_digit_ntcpdump ((char *) pport, MAX_PORT_STR_LEN))
    {
      return false;
    }

  user_port = atoi (pport);
  if (user_port < 1024 || user_port > 65535)
    {
      return false;
    }

  *port = (unsigned short) user_port;
  return true;
}

NSTACK_STATIC inline bool
get_dump_len (char *plen, u32 * limit_len)
{
  if (!is_digit_ntcpdump (plen, MAX_INTEGER_STR_LEN))
    {
      return false;
    }

  *limit_len = atoi (plen);
  return true;
}

NSTACK_STATIC inline bool
get_mac_addr (const char *opt_value, char *mac_addr)
{
  /* avoid coredump when opt_value is NULL */
  if (NULL == opt_value)
    {
      return false;
    }

  size_t org_len = strlen (opt_value);
  if (org_len != MAC_ADDR_STR_LEN)
    {
      return false;
    }

  u32 tmp_mac[MAC_ADDR_LEN];

  if (6 != SSCANF_S (opt_value, "%x:%x:%x:%x:%x:%x",
                     &tmp_mac[0],
                     &tmp_mac[1],
                     &tmp_mac[2], &tmp_mac[3], &tmp_mac[4], &tmp_mac[5]))
    {
      return false;
    }

  int i = 0;
  for (; i < MAC_ADDR_LEN; i++)
    {
      if (tmp_mac[i] > 0xFF)
        {
          // 01:02:03:04:5:1FF
          return false;
        }

      mac_addr[i] = tmp_mac[i];
    }

  return true;
}

NSTACK_STATIC inline bool
check_file_name (const char *file_name)
{
  if (0 == access (file_name, F_OK))
    {
      printf ("dump file exist, file name=%s.\n", file_name);
      return false;
    }
  return true;
}

NSTACK_STATIC inline u16
get_para_direction (const char *para)
{
  if (0 == strcasecmp (para, "out"))
    {
      return DUMP_SEND;
    }

  if (0 == strcasecmp (para, "in"))
    {
      return DUMP_RECV;
    }

  if (0 == strcasecmp (para, "inout"))
    {
      return DUMP_SEND_RECV;
    }

  return INVALID_DIRECTION;
}

NSTACK_STATIC inline u16
get_para_l2_protocol (const char *para)
{
  if (0 == strcasecmp (para, "arp"))
    {
      return PROTOCOL_ARP;
    }

  if (0 == strcasecmp (para, "rarp"))
    {
      return PROTOCOL_RARP;
    }

  if (0 == strcasecmp (para, "ip"))
    {
      return PROTOCOL_IP;
    }

  if (0 == strcasecmp (para, "oam"))
    {
      return PROTOCOL_OAM_LACP;
    }

  if (0 == strcasecmp (para, "lacp"))
    {
      return PROTOCOL_OAM_LACP;
    }

  return INVALID_L2_PROTOCOL;
}

NSTACK_STATIC inline u16
get_para_l3_protocol (const char *para)
{
  if (0 == strcasecmp (para, "tcp"))
    {
      return PROTOCOL_TCP;
    }

  if (0 == strcasecmp (para, "udp"))
    {
      return PROTOCOL_UDP;
    }

  if (0 == strcasecmp (para, "icmp"))
    {
      return PROTOCOL_ICMP;
    }

  return INVALID_L3_PROTOCOL;
}

NSTACK_STATIC bool
parse_long_options (int opt, const char *long_opt_arg, int optindex,
                    dump_condition * filter_info)
{
  switch (opt)
    {
    case OPT_ARG_HOST:
      if (!get_ip_addr (long_opt_arg, &filter_info->ip_addr))
        {
          printf ("invalid ip addr, optindex=%d, port=%s.\n", optindex,
                  long_opt_arg);
          return false;
        }
      filter_info->ip_set_flag |= COND_OR_SET;
      break;
    case OPT_ARG_LOCAL_HOST:
      if (!get_ip_addr (long_opt_arg, &filter_info->local_ip))
        {
          printf ("invalid ip addr, optindex=%d, port=%s.\n", optindex,
                  long_opt_arg);
          return false;
        }
      filter_info->ip_set_flag |= COND_LOCAL_SET;
      break;
    case OPT_ARG_REMOTE_HOST:
      if (!get_ip_addr (long_opt_arg, &filter_info->remote_ip))
        {
          printf ("invalid ip addr, optindex=%d, port=%s.\n", optindex,
                  long_opt_arg);
          return false;
        }
      filter_info->ip_set_flag |= COND_REMOTE_SET;
      break;
    case OPT_ARG_PORT:
      if (!get_dump_port (long_opt_arg, &filter_info->port))
        {
          printf ("invalid port, optindex=%d, port=%s.\n", optindex,
                  long_opt_arg);
          return false;
        }
      filter_info->port_set_flag |= COND_OR_SET;
      break;
    case OPT_ARG_LOCAL_PORT:
      if (!get_dump_port (long_opt_arg, &filter_info->local_port))
        {
          printf ("invalid port, optindex=%d, port=%s.\n", optindex,
                  long_opt_arg);
          return false;
        }
      filter_info->port_set_flag |= COND_LOCAL_SET;
      break;
    case OPT_ARG_REMOTE_PORT:
      if (!get_dump_port (long_opt_arg, &filter_info->remote_port))
        {
          printf ("invalid port, optindex=%d, port=%s.\n", optindex,
                  long_opt_arg);
          return false;
        }
      filter_info->port_set_flag |= COND_REMOTE_SET;
      break;
    case OPT_ARG_MAC:
      if (!get_mac_addr (long_opt_arg, filter_info->mac_addr))
        {
          printf ("invalid mac addr, optindex=%d, mac=%s.\n", optindex,
                  long_opt_arg);
          return false;
        }
      filter_info->mac_set_flag |= COND_OR_SET;
      break;
    case OPT_ARG_LOCAL_MAC:
      if (!get_mac_addr (long_opt_arg, filter_info->local_mac))
        {
          printf ("invalid mac addr, optindex=%d, mac=%s.\n", optindex,
                  long_opt_arg);
          return false;
        }
      filter_info->mac_set_flag |= COND_LOCAL_SET;
      break;
    case OPT_ARG_REMOTE_MAC:
      if (!get_mac_addr (long_opt_arg, filter_info->remote_mac))
        {
          printf ("invalid mac addr, optindex=%d, mac=%s.\n", optindex,
                  long_opt_arg);
          return false;
        }
      filter_info->mac_set_flag |= COND_REMOTE_SET;
      break;
    default:
      printf ("unknow arg, optindex=%d, arg=%s.\n", optindex, long_opt_arg);
      return false;
    }

  return true;
}

NSTACK_STATIC inline bool
condition_valid (dump_condition * condition)
{
  // check direction
  if (INVALID_DIRECTION == condition->direction)
    {
      printf ("direction invalid.\n");
      return false;
    }

  // check l2 protocol
  if (INVALID_L2_PROTOCOL == condition->l2_protocol)
    {
      printf ("L2 protocol invalid.\n");
      return false;
    }

  // check l3 protocol
  if (INVALID_L3_PROTOCOL == condition->l3_protocol)
    {
      printf ("L3 protocol invalid.\n");
      return false;
    }

  // check ip
  if (condition->ip_set_flag > 0x4)
    {
      printf ("IP options invalid.\n");
      return false;
    }

  // check port
  if (condition->port_set_flag > 0x4)
    {
      printf ("Port options invalid.\n");
      return false;
    }

  // check mac
  if (condition->mac_set_flag > 0x4)
    {
      printf ("MAC options invalid.\n");
      return false;
    }

  if (condition->dump_time > MAX_DUMP_TIME
      || condition->dump_time < MIN_DUMP_TIME)
    {
      printf ("dump time invalid.\n");
      return false;
    }

  return true;
}

NSTACK_STATIC inline bool
get_dump_condition (int argc, char **argv, dump_condition * filter_info)
{
  int opt = 0;
  int opt_index = 0;
  bool arg_invalid = false;
  filter_info->has_condition = 0;

  if (argc < 2)
    {
      // dump all package
      return true;
    }

  while (1)
    {
      opt =
        getopt_long (argc, argv, g_dump_short_options, g_dump_long_options,
                     &opt_index);
      if (-1 == opt)
        {
          break;
        }

      switch (opt)
        {
          // for short options
        case 'c':
          filter_info->dump_count = atoi (optarg);
          break;
        case 's':
          if (!get_dump_len (optarg, &filter_info->limit_len))
            {
              printf ("length invalid, optindex=%d, arg=%s.\n", opt_index,
                      optarg);
              arg_invalid = true;
            }
          break;
        case 'w':
          if (!check_file_name (optarg))
            {
              printf ("invalid file name, optindex=%d, arg=%s.\n", opt_index,
                      optarg);
              arg_invalid = true;
            }
          else
            {
              filter_info->dump_file_name = optarg;
            }
          break;
        case 'y':
          filter_info->l2_protocol = get_para_l2_protocol (optarg);
          break;
        case 'G':
          filter_info->dump_time = atoi (optarg);
          break;
        case 'P':
          filter_info->direction = get_para_direction (optarg);
          break;
        case 'T':
          filter_info->l3_protocol = get_para_l3_protocol (optarg);
          break;
        case '?':
          arg_invalid = true;
          break;
          // for long options
        default:
          if (!parse_long_options (opt, optarg, opt_index, filter_info))
            {
              arg_invalid = true;
            }
          break;
        }

      if (arg_invalid)
        {
          print_help_ntcpdump ();
          return false;
        }

      filter_info->has_condition = 1;
    }

  if (!condition_valid (filter_info))
    {
      filter_info->has_condition = 0;
      return false;
    }

  return true;
}

NSTACK_STATIC inline void
open_file ()
{
  if (NULL == g_dump_condition.dump_file_name)
    {
      return;
    }

  g_dump_fp = fopen (g_dump_condition.dump_file_name, "w+");
  if (NULL == g_dump_fp)
    {
      printf ("open file %s failed\n", g_dump_condition.dump_file_name);
    }
}

NSTACK_STATIC inline void
close_file ()
{
  if (NULL != g_dump_fp)
    {
      fclose (g_dump_fp);
      g_dump_fp = NULL;
    }
}

NSTACK_STATIC inline void
write_file_head (FILE * fp)
{
  dump_file_head file_head;
  file_head.magic = 0xA1B2C3D4;
  file_head.major_ver = 2;      // 0x0200;
  file_head.minor_ver = 4;      // 0x0400;
  file_head.area = 0;
  file_head.time_stamp = 0;
  file_head.max_pack_size = 0x0000FFFF; // 0xFFFF0000;
  file_head.link_type = 1;      //0x01000000;

  if (fwrite (&file_head, sizeof (dump_file_head), 1, fp) != 1)
    {
      return;
    }

  fflush (fp);
}

NSTACK_STATIC inline void
write_packet (parse_msg_info * pmsg, FILE * fp)
{
  packet_head pack_head;
  dump_msg_info *org_msg = (dump_msg_info *) pmsg->org_msg;
  pack_head.sec = org_msg->dump_sec;
  pack_head.usec = org_msg->dump_usec;
  pack_head.save_len =
    (u32) nstack_min (org_msg->len, g_dump_condition.limit_len);
  pack_head.org_len = org_msg->len;

  if (fwrite (&pack_head, sizeof (packet_head), 1, fp) != 1)
    {
      // log error
      return;
    }

  if (fwrite (org_msg->buf, pack_head.save_len, 1, fp) != 1)
    {
      // log error
      return;
    }

  fflush (fp);
  return;
}

#define EMPTY(x) (0 == (x))
#define EQUAL(x, y) ((x) == (y))

#define STR_EMPTY(str) (0 == str[0])
#define STR_EQUAL(str1, str2, len) (0 == memcmp(str1, str2, len))

#define MATCH(cond, info, field) \
    (EQUAL(cond->field, info->field))

#define MATCH_MORE(cond, field, info, field1, field2) \
    (EQUAL(cond->field, info->field1) || EQUAL(cond->field, info->field2))

#define MATCH_STR(cond, info, field, len) \
    (STR_EQUAL(cond->field, info->field, len))

#define MATCH_STR_MORE(cond, field, info, field1, field2, len) \
    (STR_EQUAL(cond->field, info->field1, len) || STR_EQUAL(cond->field, info->field2, len))

NSTACK_STATIC inline bool
ip_match (dump_condition * condition, parse_msg_info * msg_info)
{
  bool ret = false;
  switch (condition->ip_set_flag)
    {
    case COND_NOT_SET:
      ret = true;
      break;
    case COND_LOCAL_SET:
      if (MATCH (condition, msg_info, local_ip))
        {
          ret = true;
        }
      break;
    case COND_REMOTE_SET:
      if (MATCH (condition, msg_info, remote_ip))
        {
          ret = true;
        }
      break;
    case COND_AND_SET:
      if (MATCH_MORE (condition, local_ip, msg_info, local_ip, remote_ip)
          && MATCH_MORE (condition, remote_ip, msg_info, local_ip, remote_ip))
        {
          ret = true;
        }
      break;
    case COND_OR_SET:
      if (MATCH_MORE (condition, ip_addr, msg_info, local_ip, remote_ip))
        {
          ret = true;
        }
      break;
    default:
      break;
    }

  return ret;
}

NSTACK_STATIC inline bool
port_match (dump_condition * condition, parse_msg_info * msg_info)
{
  bool ret = false;
  switch (condition->port_set_flag)
    {
    case COND_NOT_SET:
      ret = true;
      break;
    case COND_LOCAL_SET:
      if (MATCH (condition, msg_info, local_port))
        {
          ret = true;
        }
      break;
    case COND_REMOTE_SET:
      if (MATCH (condition, msg_info, remote_port))
        {
          ret = true;
        }
      break;
    case COND_AND_SET:
      if (MATCH (condition, msg_info, local_port)
          && MATCH (condition, msg_info, remote_port))
        {
          ret = true;
        }
      break;
    case COND_OR_SET:
      if (MATCH_MORE (condition, port, msg_info, local_port, remote_port))
        {
          ret = true;
        }
      break;
    default:
      break;
    }

  return ret;
}

NSTACK_STATIC inline bool
mac_match (dump_condition * condition, parse_msg_info * msg_info)
{
  bool ret = false;
  switch (condition->mac_set_flag)
    {
    case COND_NOT_SET:
      ret = true;
      break;
    case COND_LOCAL_SET:
      if (MATCH_STR (condition, msg_info, local_mac, MAC_ADDR_LEN))
        {
          ret = true;
        }
      break;
    case COND_REMOTE_SET:
      if (MATCH_STR (condition, msg_info, remote_mac, MAC_ADDR_LEN))
        {
          ret = true;
        }
      break;
    case COND_AND_SET:
      if ((MATCH_STR_MORE
           (condition, local_mac, msg_info, local_mac, remote_mac,
            MAC_ADDR_LEN)
           && MATCH_STR_MORE (condition, remote_mac, msg_info, local_mac,
                              remote_mac, MAC_ADDR_LEN)))
        {
          ret = true;
        }
      break;
    case COND_OR_SET:
      if (MATCH_STR_MORE
          (condition, mac_addr, msg_info, local_mac, remote_mac,
           MAC_ADDR_LEN))
        {
          ret = true;
        }
      break;
    default:
      break;
    }

  return ret;
}

NSTACK_STATIC inline bool
filter_by_condition (dump_condition * condition, parse_msg_info * msg_info)
{
  dump_msg_info *org_msg = (dump_msg_info *) msg_info->org_msg;
  if (0 == condition->has_condition)
    {
      return false;
    }

  // direction
  if (!(condition->direction & org_msg->direction))
    {
      return true;
    }

  // l2_protocol
  if ((0 != condition->l2_protocol)
      && !MATCH (condition, msg_info, l2_protocol))
    {
      return true;
    }

  // l3_protocol
  if ((0 != condition->l3_protocol)
      && !MATCH (condition, msg_info, l3_protocol))
    {
      return true;
    }

  // ip
  if (!ip_match (condition, msg_info))
    {
      return true;
    }

  // port
  if (!port_match (condition, msg_info))
    {
      return true;
    }

  // mac
  if (!mac_match (condition, msg_info))
    {
      return true;
    }

  return false;
}

NSTACK_STATIC inline char *
get_l2_protocol_desc (u16 l2_protocol)
{
  switch (l2_protocol)
    {
    case PROTOCOL_IP:
      return "IP";
    case PROTOCOL_ARP:
      return "ARP";
    case PROTOCOL_RARP:
      return "RARP";
    case PROTOCOL_OAM_LACP:
      return "OAM/LACP";
    default:
      return "unknown";
    }
}

NSTACK_STATIC inline char *
get_l3_protocol_desc (u16 l3_protocol)
{
  switch (l3_protocol)
    {
    case PROTOCOL_ICMP:
      return "ICMP";
    case PROTOCOL_TCP:
      return "TCP";
    case PROTOCOL_UDP:
      return "UDP";
    default:
      return "unknown";
    }
}

NSTACK_STATIC inline void
get_ip_str (char *pip_addr, u32 ip_addr_len, u32 ip)
{
  int retVal;
  retVal = SPRINTF_S (pip_addr, ip_addr_len, "%d.%d.%d.%d",
                      ip & 0x000000FF,
                      (ip & 0x0000FF00) >> 8,
                      (ip & 0x00FF0000) >> 16, (ip & 0xFF000000) >> 24);
  if (-1 == retVal)
    {
      printf ("get_ip_str:SPRINTF_S failed %d.\n", retVal);
    }
}

NSTACK_STATIC inline void
print_packet (parse_msg_info * msg_info, u32 seq)
{
  char str_local_ip[IP_ADDR_LEN];
  char str_remote_ip[IP_ADDR_LEN];
  get_ip_str (str_local_ip, sizeof (str_local_ip), msg_info->local_ip);
  get_ip_str (str_remote_ip, sizeof (str_remote_ip), msg_info->remote_ip);

  dump_msg_info *org_msg = (dump_msg_info *) msg_info->org_msg;

  printf ("%-6d %-6d:%6d %-8s %-8s %-16s %-16s %-10d %-10d %-8d\n",
          seq,
          org_msg->dump_sec, org_msg->dump_usec,
          get_l2_protocol_desc (msg_info->l2_protocol),
          get_l3_protocol_desc (msg_info->l3_protocol),
          str_local_ip,
          str_remote_ip,
          msg_info->local_port, msg_info->remote_port, org_msg->len);
}

void
print_head ()
{
  if (NULL != g_dump_fp)
    {
      write_file_head (g_dump_fp);
    }
  else
    {
      printf ("ntcpdump start listening:\n");
      printf ("%-6s %-18s %-8s %-8s %-16s %-16s %-10s %-10s %-8s\n",
              "Frame", "sec:usec", "L2", "L3", "Src IP", "Dest IP",
              "Src Port", "Dest Port", "Length");
    }
}

void
register_dump_signal ()
{
  signal (SIGINT, dump_exit);
}

NSTACK_STATIC inline void
init_parse_msg_info (parse_msg_info * info)
{
  int retVal =
    MEMSET_S (info, sizeof (parse_msg_info), 0, sizeof (parse_msg_info));
  if (EOK != retVal)
    {
      printf ("MEMSET_S failed.\n");
    }
}

NSTACK_STATIC inline void
parse_msg (dump_msg_info * msg, parse_msg_info * info)
{
  init_parse_msg_info (info);

  info->org_msg = msg;

  char *pmsg = msg->buf;
  u32 len = msg->len;
  /* BEGIN: Added for PN:CODEDEX by l00351127, 2017/11/14 CID:50886 */
  if (len < MAC_ADDR_LEN + MAC_ADDR_LEN + sizeof (u16))
    {
      return;
    }
  /* END:   Added for PN:CODEDEX by l00351127, 2017/11/14 */

  // get mac addr
  if (EOK !=
      MEMCPY_S (info->remote_mac, sizeof (info->remote_mac), pmsg,
                MAC_ADDR_LEN))
    {
      return;
    }

  pmsg += MAC_ADDR_LEN;
  len -= MAC_ADDR_LEN;

  if (EOK !=
      MEMCPY_S (info->local_mac, sizeof (info->local_mac), pmsg,
                MAC_ADDR_LEN))
    {
      return;
    }

  pmsg += MAC_ADDR_LEN;
  len -= MAC_ADDR_LEN;

  info->l2_protocol = htons (*(u16 *) pmsg);
  pmsg += sizeof (u16);
  len -= sizeof (u16);

  if (PROTOCOL_IP != info->l2_protocol)
    {
      return;
    }

  ip_head *p_ip_head = (ip_head *) pmsg;
  if (len < p_ip_head->ihl)
    {
      return;
    }

  info->local_ip = p_ip_head->local_ip;
  info->remote_ip = p_ip_head->remote_ip;
  info->l3_protocol = p_ip_head->protocol;

  pmsg += p_ip_head->ihl * sizeof (u32);

  if (PROTOCOL_TCP == info->l3_protocol)
    {
      tcp_head *p_tcp_head = (tcp_head *) pmsg;
      info->local_port = htons (p_tcp_head->src_port);
      info->remote_port = htons (p_tcp_head->dst_port);
      return;
    }

  if (PROTOCOL_UDP == info->l3_protocol)
    {
      udp_head *p_udp_head = (udp_head *) pmsg;
      info->local_port = htons (p_udp_head->src_port);
      info->remote_port = htons (p_udp_head->dst_port);
      return;
    }

  return;
}

NSTACK_STATIC inline bool
time_expired (struct timespec * start_time, u32 work_sec)
{
#define TIME_EXPIRE_CHECK_COUNT 1000

  static u32 loop_count = 0;
  loop_count++;

  if (0 != loop_count % TIME_EXPIRE_CHECK_COUNT)
    {
      return false;
    }

  struct timespec cur_time;
  GET_CUR_TIME (&cur_time);

  if (cur_time.tv_sec - start_time->tv_sec > work_sec)
    {
      return true;
    }

  return false;
}

NSTACK_STATIC void
dump_packet (parse_msg_info * msg)
{
  g_dumped_packet++;
  if (!g_dump_fp)
    {
      print_packet (msg, g_dumped_packet);
      return;
    }

  write_packet (msg, g_dump_fp);
}

NSTACK_STATIC void
dump_msg (mring_handle dump_ring, mring_handle dump_pool,
          dump_condition * condition, struct timespec *start_time)
{
  u32 dump_count = 0;
  open_file ();

  print_head ();

  void *msg = NULL;
  while (!g_dump_exit
         && (dump_count < condition->dump_count)
         && !time_expired (start_time, condition->dump_time))
    {
      if (nsfw_mem_ring_dequeue (dump_ring, &msg) <= 0)
        {
          sys_sleep_ns (0, 10000);
          continue;
        }

      if (NULL == msg)
        {
          continue;
        }

      parse_msg_info msg_info;
      parse_msg (msg, &msg_info);

      g_captured_packet++;
      if (!condition->has_condition)
        {
          dump_packet (&msg_info);
          dump_count++;
          nsfw_mem_ring_enqueue (dump_pool, msg);
          continue;
        }

      if (filter_by_condition (condition, &msg_info))
        {
          g_filtered_packet++;
          nsfw_mem_ring_enqueue (dump_pool, msg);
          continue;
        }

      dump_packet (&msg_info);
      dump_count++;
      nsfw_mem_ring_enqueue (dump_pool, msg);
    }

  close_file ();
}

mring_handle
dump_get_mem_ring ()
{
  return nsfw_mem_ring_lookup (&g_dump_mem_ring_info);
}

mring_handle
dump_get_mem_pool ()
{
  return nsfw_mem_sp_lookup (&g_dump_mem_pool_info);
}

NSTACK_STATIC void
dump_clear_mem (mring_handle ring, mring_handle pool)
{
  void *msg = NULL;
  while (nsfw_mem_ring_dequeue (ring, &msg) > 0)
    {
      nsfw_mem_ring_enqueue (pool, msg);
      sys_sleep_ns (0, 1000);
    }
}

/* BEGIN: Added for PN:CODEDEX by l00351127, 2017/11/14 CID:50859*/
i16
dump_send_req (u16 op_type, i16 task_id, u32 task_keep_time)
/* END:   Added for PN:CODEDEX by l00351127, 2017/11/14 */
{
  nsfw_mgr_msg *req =
    (nsfw_mgr_msg *) nsfw_mgr_msg_alloc (MGR_MSG_TOOL_TCPDUMP_REQ,
                                         NSFW_PROC_MAIN);
  if (NULL == req)
    {
      printf ("all message for getting instance id failed.\n");
      return -1;
    }

  nsfw_tool_dump_msg *req_body = GET_USER_MSG (nsfw_tool_dump_msg, req);
  req_body->op_type = op_type;
  req_body->task_id = task_id;
  req_body->task_keep_time = task_keep_time;

  nsfw_mgr_msg *rsp = nsfw_mgr_null_rspmsg_alloc ();
  if (NULL == rsp)
    {
      printf ("alloc rsp message for getting memory failed.\n");
      nsfw_mgr_msg_free (req);
      return -1;
    }

  if (!nsfw_mgr_send_req_wait_rsp (req, rsp))
    {
      printf ("request memory can not get response.\n");
      nsfw_mgr_msg_free (req);
      nsfw_mgr_msg_free (rsp);
      return -1;
    }

  if (rsp->src_proc_type != NSFW_PROC_MAIN
      || rsp->dst_proc_type != NSFW_PROC_TOOLS)
    {
      printf
        ("dump get wrong response, src or dst proc type error,src proc type=%u, dst proc type=%u.\n",
         rsp->src_proc_type, rsp->dst_proc_type);
      nsfw_mgr_msg_free (req);
      nsfw_mgr_msg_free (rsp);
      return -1;
    }

  if (rsp->msg_type != MGR_MSG_TOOL_TCPDUMP_RSP)
    {
      printf ("dump get wrong response, msg type error, msg type=%d.\n",
              rsp->msg_type);
      nsfw_mgr_msg_free (req);
      nsfw_mgr_msg_free (rsp);
      return -1;
    }

  nsfw_tool_dump_msg *rsp_body = GET_USER_MSG (nsfw_tool_dump_msg, rsp);

/* BEGIN: Added for PN:CODEDEX by l00351127, 2017/11/14 CID:50859*/
  i16 new_task_id = rsp_body->task_id;
/* END:   Added for PN:CODEDEX by l00351127, 2017/11/14 */

  nsfw_mgr_msg_free (req);
  nsfw_mgr_msg_free (rsp);

  return new_task_id;
}

i16
start_dump_task (u32 task_keep_time)
{
  return dump_send_req (START_DUMP_REQ, -1, task_keep_time);
}

i16
stop_dump_task (i16 task_id)
{
  return dump_send_req (STOP_DUMP_REQ, task_id, 0);
}

NSTACK_STATIC void
init_dump_condition (dump_condition * condition)
{
  if (EOK !=
      MEMSET_S (condition, sizeof (dump_condition), 0,
                sizeof (dump_condition)))
    {
      printf ("MEMSET_S failed.\n");
    }
  condition->limit_len = DUMP_MSG_SIZE;
  condition->dump_time = DEFAULT_DUMP_TIME;
  condition->direction = 3;
  condition->dump_count = DEFAULT_DUMP_COUNT;
}

NSTACK_STATIC inline bool
send_hbt_req (u32 seq, i16 task_id)
{
  nsfw_mgr_msg *req =
    (nsfw_mgr_msg *) nsfw_mgr_msg_alloc (MGR_MSG_TOOL_HEART_BEAT,
                                         NSFW_PROC_MAIN);
  if (NULL == req)
    {
      printf ("all message for getting instance id failed.\n");
      return false;
    }

  nsfw_tool_hbt *req_body = GET_USER_MSG (nsfw_tool_hbt, req);
  req_body->seq = seq;
  req_body->task_id = task_id;

  if (!nsfw_mgr_send_msg (req))
    {
      printf ("request memory can not get response.\n");
    }

  nsfw_mgr_msg_free (req);

  return true;
}

NSTACK_STATIC bool
on_send_hbt_req (u32 timer_type, void *data)
{
  dump_timer_info *ptimer_info = (dump_timer_info *) data;
  // send heartbeat

  send_hbt_req (ptimer_info->seq, ptimer_info->task_id);
  ptimer_info->seq++;

  ptimer_info->ptimer =
    nsfw_timer_reg_timer (DUMP_HBT_TIMER, ptimer_info, on_send_hbt_req,
                          *(struct timespec *) (ptimer_info->interval));
  return true;
}

NSTACK_STATIC bool
start_dump_hbt (dump_timer_info * ptimer_info, i16 task_id)
{
  struct timespec *time_interval =
    (struct timespec *) malloc (sizeof (struct timespec));
  if (NULL == time_interval)
    {
      return false;
    }

  time_interval->tv_sec = DUMP_HBT_INTERVAL;
  time_interval->tv_nsec = 0;

  ptimer_info->interval = time_interval;
  ptimer_info->seq = 1;
  ptimer_info->task_id = task_id;

  ptimer_info->ptimer =
    nsfw_timer_reg_timer (DUMP_HBT_TIMER, ptimer_info, on_send_hbt_req,
                          *time_interval);

  return true;
}

NSTACK_STATIC bool
stop_dump_hbt (dump_timer_info * ptimer_info)
{
  free (ptimer_info->interval);
  /* fix "SET_NULL_AFTER_FREE" type codedex issue */
  ptimer_info->interval = NULL;
  nsfw_timer_rmv_timer (ptimer_info->ptimer);
  return true;
}

#ifndef NSTACK_STATIC_CHECK
int
main (int argc, char *argv[])
#else
int
ntcpdump_main (int argc, char *argv[])
#endif
{
  register_dump_signal ();

  init_dump_condition (&g_dump_condition);
  if (!get_dump_condition (argc, argv, &g_dump_condition))
    {
      printf ("dump exit because of input invalid.\n");
      return INPUT_INVALID;
    }

  printf ("parse filter condition ok.\n");

  fw_poc_type proc_type = NSFW_PROC_TOOLS;
  nsfw_mem_para stinfo = { 0 };
  stinfo.iargsnum = 0;
  stinfo.pargs = NULL;
  stinfo.enflag = proc_type;
  nstack_framework_setModuleParam (NSFW_MEM_MGR_MODULE, &stinfo);
  nstack_framework_setModuleParam (NSFW_MGR_COM_MODULE,
                                   (void *) ((u64) proc_type));
  nstack_framework_setModuleParam (NSFW_TIMER_MODULE,
                                   (void *) ((u64) proc_type));

  if (0 != nstack_framework_init ())
    {
      printf ("dump init failed.\n");
      return FRAMEWORK_INIT_FAILED;
    }

  mring_handle dump_mem_pool = dump_get_mem_pool ();
  if (NULL == dump_mem_pool)
    {
      printf ("dump init mem pool failed.\n");
      return MEMPOOL_INIT_FAILED;
    }

  mring_handle dump_mem_ring = dump_get_mem_ring ();
  if (NULL == dump_mem_ring)
    {
      printf ("dump init mem ring failed.\n");
      return MEMRING_INIT_FAILED;
    }

  // initialize queue first
  dump_clear_mem (dump_mem_ring, dump_mem_pool);

  i16 task_id = start_dump_task (g_dump_condition.dump_time);
  if (task_id < 0 || task_id > MAX_DUMP_TASK)
    {
      printf ("start dump task failed.\n");
      return START_TASK_FAILED;
    }

  if (!start_dump_hbt (&g_dump_hbt_timer, task_id))
    {
      printf ("start dump heart beat timer failed.\n");
      return START_TIMER_FAILED;
    }

  struct timespec dump_start_time;
  GET_CUR_TIME (&dump_start_time);
  dump_msg (dump_mem_ring, dump_mem_pool, &g_dump_condition,
            &dump_start_time);

  i16 new_task_id = stop_dump_task (task_id);
  if (new_task_id != task_id)
    {
      printf ("stop dump task failed.\n");
    }
  /* modify deadcode type codedex issue */
  (void) stop_dump_hbt (&g_dump_hbt_timer);

  printf ("dump complete.\n");
  printf ("captured packets=%u.\n", g_captured_packet);
  printf ("dumped packets=%u.\n", g_dumped_packet);
  printf ("filtered packets=%u.\n", g_filtered_packet);

  return 0;
}
