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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <memory.h>
#include <strings.h>
#include <limits.h>
#include <getopt.h>
#include "nstack_securec.h"
#include "tool_common.h"

/*======== for global variables =======*/

NSTACK_STATIC input_info g_input_info = { 0 };
static stat_info g_stat_info = { 0 };

static char *g_nping_short_options = "c:r:I:";

int g_exit = 0;
void
user_exit (int sig)
{
  g_exit = 1;
}

NSTACK_STATIC inline double
get_cost_time (struct timespec *pstart, struct timespec *pend)
{
  double sec = (double) (pend->tv_sec - pstart->tv_sec);
  double nsec = (double) (pend->tv_nsec - pstart->tv_nsec);

  return (sec * 1000 + (nsec / 1000000));
}

NSTACK_STATIC inline double
get_lost_rate (unsigned int lost_count, unsigned int send_count)
{
  if (0 == send_count)
    {
      return 0;
    }

  return ((double) lost_count / send_count);
}

void
print_stat (stat_info * info, const char *remote_ip)
{
  unsigned int send_count = info->send_seq;
  unsigned int recv_count = info->recv_ok;
  unsigned int lost_count = send_count - recv_count;
  double lost_rate = 100 * get_lost_rate (lost_count, send_count);
  double cost_time = get_cost_time (&info->start_time, &info->end_time);

  printf ("\n------ %s ping statistics ------\n", remote_ip);

  printf
    ("%u packets transmitted, %u received, %.2f%% packet loss, time %.2fms\n",
     send_count, recv_count, lost_rate, cost_time);

  if (0 != recv_count)
    {
      double min_interval = info->min_interval;
      double max_interval = info->max_interval;
      double avg_interval = info->all_interval / recv_count;
      printf ("rtt min/avg/max = %.3f/%.3f/%.3f ms\n", min_interval,
              avg_interval, max_interval);
    }
}

NSTACK_STATIC inline u16
get_chksum (icmp_head * pmsg)
{
  int len = sizeof (icmp_head);
  u32 sum = 0;
  u16 *msg_stream = (u16 *) pmsg;
  u16 check_sum = 0;

  while (len > 1)
    {
      sum += *msg_stream;
      len -= 2;
      msg_stream++;
    }

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  check_sum = ~sum;

  return check_sum;
}

#ifndef MAX_SHORT
#define MAX_SHORT 0xFFFF
#endif

NSTACK_STATIC inline void
code_icmp_req (icmp_head * pmsg, u32 send_seq, pid_t my_pid)
{
  struct timespec cur_time;

  pmsg->icmp_type = ICMP_ECHO;
  pmsg->icmp_code = 0;
  pmsg->icmp_cksum = 0;
  pmsg->icmp_seq = send_seq % (MAX_SHORT + 1);
  pmsg->icmp_id = my_pid;
  pmsg->timestamp = 0;

  if (0 != clock_gettime (CLOCK_MONOTONIC, &cur_time))
    {
      printf ("Failed to get time, errno = %d\n", errno);
    }

  pmsg->icmp_sec = cur_time.tv_sec;
  pmsg->icmp_nsec = cur_time.tv_nsec;

  pmsg->icmp_cksum = get_chksum (pmsg);
  return;
}

NSTACK_STATIC int
send_icmp_req (int socket, pid_t my_pid, stat_info * stat,
               struct sockaddr_in *remote_addr)
{
  int send_ret;
  icmp_head icmp_req;

  stat->send_seq++;
  code_icmp_req (&icmp_req, stat->send_seq, my_pid);

  send_ret = sendto (socket, &icmp_req, sizeof (icmp_head), 0,
                     (struct sockaddr *) remote_addr,
                     sizeof (struct sockaddr_in));

  if (send_ret < 0)
    {
      printf ("send icmp request failed.\n");
      return -1;
    }

  return send_ret;
}

NSTACK_STATIC inline double
cal_interval (struct timespec *psend, struct timespec *precv,
              stat_info * stat)
{
  double interval = get_cost_time (psend, precv);

  stat->all_interval += interval;

  if (interval < stat->min_interval)
    {
      stat->min_interval = interval;
    }

  if (interval > stat->max_interval)
    {
      stat->max_interval = interval;
    }

  return interval;
}

NSTACK_STATIC inline void
print_info_on_reply (long send_sec, long send_usec, u32 send_seq,
                     stat_info * stat, struct sockaddr_in *dst_addr)
{
  struct timespec send_time;
  struct timespec recv_time;
  if (0 != clock_gettime (CLOCK_MONOTONIC, &recv_time))
    {
      printf ("Failed to get time, errno = %d\n", errno);
    }

  send_time.tv_sec = send_sec;
  send_time.tv_nsec = send_usec;

  double interval = cal_interval (&send_time, &recv_time, stat);
  const char *remote_ip = inet_ntoa (dst_addr->sin_addr);
  printf ("%lu bytes from %s: icmp_seq=%u, time=%.3f ms\n",
          sizeof (icmp_head), remote_ip, send_seq % (MAX_SHORT + 1),
          interval);
}

NSTACK_STATIC inline void
print_info_on_no_reply (u32 send_seq, const char *remote_ip)
{
  printf ("No data from %s, icmp_seq=%u, Destination Host Unreachable\n",
          remote_ip, send_seq);
}

static inline int
expect_addr (int expect, int addr)
{
  return (expect == addr);
}

NSTACK_STATIC inline int
parse_icmp_reply (char *buf, unsigned int buf_len, u32 my_pid, u32 send_seq,
                  stat_info * stat, struct sockaddr_in *dst_addr)
{
  unsigned int ip_head_len;
  ip_head *recv_ip_head;
  icmp_head *recv_icmp_head;

  // parse all received ip package
  while (buf_len > sizeof (ip_head))
    {
      recv_ip_head = (ip_head *) buf;

      ip_head_len = recv_ip_head->ihl << 2;
      recv_icmp_head = (icmp_head *) (buf + ip_head_len);
      buf_len -= htons (recv_ip_head->tot_len);

      if (!expect_addr (dst_addr->sin_addr.s_addr, recv_ip_head->local_ip))
        {
          return 0;
        }

      if ((recv_icmp_head->icmp_type == ICMP_REPLY)
          && (recv_icmp_head->icmp_id == my_pid)
          && (recv_icmp_head->icmp_seq == send_seq % (MAX_SHORT + 1)))
        {
          print_info_on_reply (recv_icmp_head->icmp_sec,
                               recv_icmp_head->icmp_nsec, send_seq, stat,
                               dst_addr);

          return 1;
        }

      buf += ip_head_len;
    }

  return 0;
}

NSTACK_STATIC inline int
recv_icmp_reply_ok (int socket, int my_pid, u32 send_seq, stat_info * stat,
                    struct sockaddr_in *dst_addr)
{
#define MAX_RECV_BUFF_SIZE (200 * sizeof(icmp_head))

  struct sockaddr_in remote_addr;
  unsigned int addr_len = sizeof (remote_addr);
  char recv_buf[MAX_RECV_BUFF_SIZE];

  int recv_ret = recvfrom (socket, recv_buf, MAX_RECV_BUFF_SIZE, 0,
                           (struct sockaddr *) &remote_addr, &addr_len);

  if (recv_ret < 0)
    {
      return 0;
    }

  if (!parse_icmp_reply
      (recv_buf, (unsigned int) recv_ret, my_pid, send_seq, stat, dst_addr))
    {
      return 0;
    }

  return 1;
}

// check recv msg in 2 us
/* BEGIN: Added for PN:CODEDEX by l00351127, 2017/11/14 CID:50811*/
NSTACK_STATIC void
recv_icmp_reply (int socket, pid_t my_pid, input_info * input,
                 stat_info * stat, struct sockaddr_in *dst_addr)
/* END:   Added for PN:CODEDEX by l00351127, 2017/11/14 */
{
#define MAX_SLEEP_TIME (2 * US_TO_NS)
#define MAX_WAIT_TIME (1000 * MS_TO_NS)

  int recv_ok = 0;
  int retry = 0;
  long sleep_all = 0;
  long sleep_time = 0;

  u32 expect_seq = stat->send_seq;

  while (retry < input->retry_count)
    {
      if (recv_icmp_reply_ok (socket, my_pid, expect_seq, stat, dst_addr))
        {
          recv_ok = 1;
          stat->recv_ok++;
          break;
        }

      sleep_all += MAX_SLEEP_TIME;
      sys_sleep_ns (0, MAX_SLEEP_TIME);
      retry++;
    }

  if (!recv_ok)
    {
      print_info_on_no_reply (expect_seq, input->dst_ip);
    }

  if (sleep_all < MAX_WAIT_TIME)
    {
      sleep_time = MAX_WAIT_TIME - sleep_all;
      sys_sleep_ns (0, sleep_time);
    }
}

NSTACK_STATIC inline int
is_digit_nping (char *str)
{
  if (NULL == str || '\0' == str[0])
    {
      return 0;
    }

  while (*str)
    {
      if (*str > '9' || *str < '0')
        {
          return 0;
        }

      str++;
    }

  return 1;
}

#define MIN_IP_LEN_NPING 7      //the length of string ip addr x.x.x.x is 7, 4 numbers + 3 dots = 7
#define MAX_IP_LEN_NPING 15     //the length of string ip addr xxx.xxx.xxx.xxx is 15, 12 numbers + 3 dots = 15

NSTACK_STATIC inline int
is_ip_addr_nping (char *param_addr)
{
  int ipseg1;
  int ipseg2;
  int ipseg3;
  int ipseg4;

  if (NULL == param_addr)
    {
      return 0;
    }

  size_t len = strlen (param_addr);
  if (len < MIN_IP_LEN_NPING || len > MAX_IP_LEN_NPING) // valid ip MIN_IP_LEN=7, MAX_IP_LEN=15
    {
      printf ("Input IP format error.\n");
      return 0;
    }

  if (SSCANF_S (param_addr, "%d.%d.%d.%d", &ipseg1, &ipseg2, &ipseg3, &ipseg4)
      != 4)
    {
      return 0;
    }

  if ((ipseg1 & 0xffffff00)
      || (ipseg2 & 0xffffff00)
      || (ipseg3 & 0xffffff00) || (ipseg4 & 0xffffff00))
    {
      return 0;
    }

  return 1;
}

NSTACK_STATIC inline bool
check_nping_input_para (input_info * input)
{
  if (input->send_count < 1)
    {
      return false;
    }

  if (input->retry_count < NPING_RETRY_COUNT
      || input->retry_count > MAX_NPING_RETRY_COUNT)
    {
      return false;
    }

  if (0 == input->src_ip[0] || 0 == input->dst_ip[0])
    {
      return false;
    }

  return true;
}

NSTACK_STATIC inline bool
get_nping_input_para (int argc, char **argv, input_info * input)
{
  int opt = 0;
  bool arg_invalid = false;

  if (argc < 2)
    {
      return false;
    }

  if (!is_ip_addr_nping (argv[1]))
    {
      return false;
    }

  /* CID 36687 (#1 of 2): Unchecked return value (CHECKED_RETURN) */
  if (EOK != STRCPY_S (input->dst_ip, sizeof (input->dst_ip), argv[1]))
    {
      printf ("STRCPY_S failed.\n");
      return false;
    }

  while (1)
    {
      opt = getopt (argc - 1, &argv[1], g_nping_short_options);
      if (-1 == opt)
        {
          break;
        }

      switch (opt)
        {
          // for short options
        case 'c':
          input->send_count = atoi (optarg);
          break;
        case 'r':
          input->retry_count = atoi (optarg);
          break;
        case 'I':
          /* CID 36687 (#2 of 2): Unchecked return value (CHECKED_RETURN) */
          if (!is_ip_addr_nping (optarg))
            {
              return false;
            }
          if (EOK != STRCPY_S (input->src_ip, sizeof (input->src_ip), optarg))
            {
              printf ("STRCPY_S failed.\n");
              return false;
            }
          break;
        default:
          arg_invalid = true;
          break;
        }

      if (arg_invalid)
        {
          return false;
        }
    }

  if (!check_nping_input_para (input))
    {
      return false;
    }

  return true;
}

void
print_help_nping ()
{
  printf
    ("usage:nping destination [-c send_count -I src_addr -r retry_count]\n");
  printf ("send count range:1-2147483647\n");
  printf ("retry count range:1000-20000\n");
}

NSTACK_STATIC inline void
init_input_info (input_info * input)
{
  if (EOK != MEMSET_S (input, sizeof (input_info), 0, sizeof (input_info)))
    {
      printf ("MEMSET_S failed.\n");
      return;
    }

  input->send_count = 100000;
  input->retry_count = NPING_RETRY_COUNT;
}

NSTACK_STATIC inline void
init_stat_info (stat_info * stat)
{
  stat->max_interval = 0;
  stat->min_interval = 0xFFFFFFFF;
}

#ifndef NSTACK_STATIC_CHECK
int
main (int argc, char *argv[])
#else
int
nping_main (int argc, char *argv[])
#endif
{
  struct sockaddr_in local_addr;
  struct sockaddr_in remote_addr;
  int send_ret;
  int ret = -1;
  int icmp_sock;

  pid_t my_pid;

  if (EOK !=
      MEMSET_S (&local_addr, sizeof (local_addr), 0, sizeof (local_addr)))
    {
      printf ("MEMSET_S failed.\n");
      return 0;
    }
  local_addr.sin_family = AF_INET;

  init_input_info (&g_input_info);
  init_stat_info (&g_stat_info);

  if (!get_nping_input_para (argc, argv, &g_input_info))
    {
      print_help_nping ();
      return 0;
    }

  if ((icmp_sock = socket (AF_INET, CUSTOM_SOCK_TYPE, IPPROTO_ICMP)) < 0)
    {
      printf ("create socket failed.\n");
      return 0;
    }

  local_addr.sin_addr.s_addr = inet_addr (g_input_info.src_ip);

  if (0 !=
      bind (icmp_sock, (struct sockaddr *) &local_addr,
            sizeof (struct sockaddr)))
    {
      printf ("bind failed, src ip %s\n", g_input_info.src_ip);
      close (icmp_sock);
      return 0;
    }

  int opt = 1;
  ret = ioctl (icmp_sock, FIONBIO, &opt);
  if (-1 == ret)
    {
      printf ("fcntl O_NONBLOCK fail\n");
      close (icmp_sock);
      return 0;
    }

  if (EOK !=
      MEMSET_S (&remote_addr, sizeof (remote_addr), 0, sizeof (remote_addr)))
    {
      printf ("MEMSET_S failed.\n");
      close (icmp_sock);
      return 0;
    }

  remote_addr.sin_family = AF_INET;
  remote_addr.sin_addr.s_addr = inet_addr (g_input_info.dst_ip);

  my_pid = getpid ();
  printf ("nping %s %lu bytes of data, pid:%d.\n", g_input_info.dst_ip,
          sizeof (icmp_head), my_pid);

  signal (SIGINT, user_exit);

  int loop_count = 0;
  if (0 != clock_gettime (CLOCK_MONOTONIC, &g_stat_info.start_time))
    {
      printf ("Failed to get time, errno = %d\n", errno);
    }

/* BEGIN: Added for PN:CODEDEX by l00351127, 2017/11/14  CID:50811*/
  i32 send_count = g_input_info.send_count;
/* END:   Added for PN:CODEDEX by l00351127, 2017/11/14 */

  while (!g_exit && (loop_count < send_count))
    {
      send_ret =
        send_icmp_req (icmp_sock, my_pid, &g_stat_info, &remote_addr);
      if (send_ret < 0)
        {
          break;
        }

      recv_icmp_reply (icmp_sock, my_pid, &g_input_info, &g_stat_info,
                       &remote_addr);

      loop_count++;
    }

  close (icmp_sock);

  if (0 != clock_gettime (CLOCK_MONOTONIC, &g_stat_info.end_time))
    {
      printf ("Failed to get time, errno = %d\n", errno);
    }

  print_stat (&g_stat_info, g_input_info.dst_ip);

  return 0;
}
