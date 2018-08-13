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

#include "nsfw_init.h"
#include "nsfw_maintain_api.h"
#include "nsfw_mem_api.h"
#include "nsfw_fd_timer_api.h"
#include "nstack_log.h"
#include "nstack_securec.h"

#include "dump_tool.h"
#include "nstack_dmm_adpt.h"

NSTACK_STATIC u32 g_dump_task_mask = 0;
NSTACK_STATIC dump_task_info g_dump_task[MAX_DUMP_TASK];
static dump_timer_info g_dump_timer;

static nsfw_mem_name g_dump_mem_ring_info =
  { NSFW_SHMEM, NSFW_PROC_MAIN, DUMP_SHMEM_RIGN_NAME };
static nsfw_mem_name g_dump_mem_pool_info =
  { NSFW_SHMEM, NSFW_PROC_MAIN, DUMP_SHMEM_POOL_NAME };

NSTACK_STATIC inline void
dump_task_init (dump_task_info * task)
{
  task->task_state = 0;
  task->task_id = -1;
  task->task_keep_time = 0;
  task->task_pool = NULL;
  task->task_queue = NULL;
}

NSTACK_STATIC inline void
clear_dump_task ()
{
  int i = 0;
  for (; i < MAX_DUMP_TASK; i++)
    {
      dump_task_init (&g_dump_task[i]);
    }
}

NSTACK_STATIC i16
get_dump_task (nsfw_tool_dump_msg * req)
{
  // base version just support 1 dump task
  if (req->task_keep_time > MAX_DUMP_TIME
      || req->task_keep_time < MIN_DUMP_TIME)
    {
      NSPOL_DUMP_LOGERR ("task keep time invalid] time=%u.",
                         req->task_keep_time);
      return -1;
    }

  if (1 == g_dump_task[0].task_state)
    {
      NSPOL_DUMP_LOGERR
        ("start tcpdump task failed, task still in work state] task id=%d.",
         0);
      return -1;
    }

  struct timespec cur_time;
  GET_CUR_TIME (&cur_time);     /*do not need return value */
  g_dump_task[0].task_start_sec = cur_time.tv_sec;
  g_dump_task[0].last_hbt_sec = cur_time.tv_sec;

  g_dump_task[0].task_state = 1;
  g_dump_task[0].task_id = 0;
  g_dump_task[0].task_keep_time = req->task_keep_time;

  g_dump_task_mask |= (1 << g_dump_task[0].task_id);
  NSPOL_DUMP_LOGINF ("start tcpdump task success] task id=%d.", 0);
  return 0;
}

NSTACK_STATIC i16
close_dump_task (i16 task_id)
{
  if (task_id < 0 || task_id >= MAX_DUMP_TASK)
    {
      NSPOL_DUMP_LOGERR
        ("receive invalid task id in close dump task req] task id=%d.",
         task_id);
      return -1;
    }

  g_dump_task[task_id].task_state = 0;

  g_dump_task_mask &= ~(1 << task_id);

  NSPOL_DUMP_LOGINF ("stop tcpdump task success] task id=%d.", 0);

  return task_id;
}

NSTACK_STATIC void
stop_expired_task (int idx, u32 now_sec)
{
  dump_task_info *ptask = &g_dump_task[idx];
  if (0 == ptask->task_state)
    {
      return;
    }

  if (now_sec - ptask->task_start_sec > ptask->task_keep_time)
    {
      NSPOL_DUMP_LOGERR
        ("stop dump task because task time expired] task id=%d, now_sec=%u, task start time=%u, dump time=%u.",
         ptask->task_id, now_sec, ptask->task_start_sec,
         ptask->task_keep_time);
      close_dump_task (ptask->task_id);
      return;
    }

  if (now_sec - ptask->last_hbt_sec > DUMP_TASK_HBT_TIME_OUT)
    {
      NSPOL_DUMP_LOGERR
        ("stop dump task because heart beat time out] task id=%d, now_sec=%u, last hbt time=%u, hbt timeout=%u.",
         ptask->task_id, now_sec, ptask->last_hbt_sec,
         DUMP_TASK_HBT_TIME_OUT);
      close_dump_task (ptask->task_id);
    }

  return;
}

NSTACK_STATIC inline bool
check_dump_alive (u32 timer_type, void *data)
{
  dump_timer_info *ptimer_info = (dump_timer_info *) data;

  struct timespec cur_time;
  GET_CUR_TIME (&cur_time);     /*do not need return value */

  int i = 0;
  for (; i < MAX_DUMP_TASK; i++)
    {
      stop_expired_task (i, cur_time.tv_sec);
    }

  ptimer_info->ptimer =
    nsfw_timer_reg_timer (1, ptimer_info, check_dump_alive,
                          *(struct timespec *) (ptimer_info->interval));

  return true;
}

NSTACK_STATIC bool
dump_init_timer (dump_timer_info * ptimer_info)
{
  struct timespec *trigger_time =
    (struct timespec *) malloc (sizeof (struct timespec));
  if (NULL == trigger_time)
    {
      NSPOL_DUMP_LOGERR ("alloc memory for timer failed.");
      return false;
    }

  trigger_time->tv_sec = DUMP_HBT_CHK_INTERVAL;
  trigger_time->tv_nsec = 0;

  ptimer_info->interval = trigger_time;
  ptimer_info->ptimer =
    nsfw_timer_reg_timer (1, ptimer_info, check_dump_alive, *trigger_time);
  return true;
}

NSTACK_STATIC inline u32
copy_limit_buf (char *dst_buf, int dst_buf_len, char *src_buf,
                u32 src_buf_len, u32 limit_len)
{
  if (src_buf_len < limit_len)
    {
      NSPOL_DUMP_LOGERR ("message too short] len=%d", src_buf_len);
      return 0;
    }

  if (EOK != MEMCPY_S (dst_buf, dst_buf_len, src_buf, limit_len))
    {
      NSPOL_DUMP_LOGERR ("MEMCPY_S failed");
      return 0;
    }

  return limit_len;
}

NSTACK_STATIC inline u32
get_packet_buf (char *dst_buf, int dst_buf_len, char *src_buf,
                u32 src_buf_len, u32 eth_head_len)
{
#define TCP_BUF_LEN (eth_head_len + IP_HEAD_LEN + TCP_HEAD_LEN)
#define UDP_BUF_LEN (eth_head_len + IP_HEAD_LEN + UDP_HEAD_LEN)
#define ICMP_BUF_LEN (eth_head_len + IP_HEAD_LEN + ICMP_HEAD_LEN)

  if (NULL == dst_buf || NULL == src_buf)
    {
      return 0;
    }

  struct ip_hdr *ip_head = (struct ip_hdr *) (src_buf + eth_head_len);
  if (ip_head->_proto == IP_PROTO_TCP)
    {
      return copy_limit_buf (dst_buf, dst_buf_len, src_buf, src_buf_len,
                             TCP_BUF_LEN);
    }

  if (ip_head->_proto == IP_PROTO_UDP)
    {
      return copy_limit_buf (dst_buf, dst_buf_len, src_buf, src_buf_len,
                             UDP_BUF_LEN);
    }

  if (ip_head->_proto == IP_PROTO_ICMP)
    {
      return copy_limit_buf (dst_buf, dst_buf_len, src_buf, src_buf_len,
                             ICMP_BUF_LEN);
    }

  if (EOK != MEMCPY_S (dst_buf, dst_buf_len, src_buf, src_buf_len))
    {
      NSPOL_DUMP_LOGERR ("MEMCPY_S failed");
      return 0;
    }

  return src_buf_len;
}

NSTACK_STATIC int
pack_msg_inout (void *dump_buf, char *msg_buf, u32 len, u16 direction,
                void *para)
{
  (void) para;
  dump_msg_info *pmsg = (dump_msg_info *) dump_buf;
  if (!pmsg)
    {
      return 0;
    }

  pmsg->direction = direction;
  struct timeval cur_time;
  gettimeofday (&cur_time, NULL);
  pmsg->dump_sec = cur_time.tv_sec;
  pmsg->dump_usec = cur_time.tv_usec;

  /* msg content can not be captured */
  u32 real_len =
    get_packet_buf (pmsg->buf, DUMP_MSG_SIZE, msg_buf, len, ETH_HEAD_LEN);
  if (0 == real_len)
    {
      return 0;
    }

  pmsg->len = real_len;

  return 1;
}

NSTACK_STATIC int
pack_msg_loop (void *dump_buf, char *msg_buf, u32 len, u16 direction,
               void *para)
{
  dump_msg_info *pmsg = (dump_msg_info *) dump_buf;
  if (!pmsg)
    {
      return 0;
    }
  pmsg->direction = direction;
  struct timeval cur_time;
  gettimeofday (&cur_time, NULL);
  pmsg->dump_sec = cur_time.tv_sec;
  pmsg->dump_usec = cur_time.tv_usec;

  eth_head pack_eth_head;
  int retVal =
    MEMCPY_S (pack_eth_head.dest_mac, MAC_ADDR_LEN, para, MAC_ADDR_LEN);
  if (EOK != retVal)
    {
      NSPOL_DUMP_LOGERR ("MEMCPY_S failed]retVal=%d", retVal);
      return 0;
    }
  retVal = MEMCPY_S (pack_eth_head.src_mac, MAC_ADDR_LEN, para, MAC_ADDR_LEN);
  if (EOK != retVal)
    {
      NSPOL_DUMP_LOGERR ("MEMCPY_S failed]retVal=%d", retVal);
      return 0;
    }
  pack_eth_head.eth_type = htons (PROTOCOL_IP);

  retVal =
    MEMCPY_S (pmsg->buf, DUMP_MSG_SIZE, &pack_eth_head, sizeof (eth_head));
  if (EOK != retVal)
    {
      NSPOL_DUMP_LOGERR ("MEMCPY_S failed]retVal=%d", retVal);
      return 0;
    }

  u32 buf_len = DUMP_MSG_SIZE - ETH_HEAD_LEN;

  /* msg content can not be captured- Begin */
  u32 real_len =
    get_packet_buf (pmsg->buf + ETH_HEAD_LEN, buf_len, msg_buf, len, 0);
  if (0 == real_len)
    {
      return 0;
    }

  pmsg->len = real_len + ETH_HEAD_LEN;

  return 1;
}

NSTACK_STATIC void
dump_enqueue (int task_idx, void *buf, u32 len, u16 direction,
              pack_msg_fun pack_msg, void *para)
{
  mring_handle queue = (mring_handle *) g_dump_task[task_idx].task_queue;
  mring_handle pool = (mring_handle *) g_dump_task[task_idx].task_pool;

  void *msg = NULL;

  if (nsfw_mem_ring_dequeue (pool, &msg) <= 0)
    {
      // such log may be too much if queue is empty
      NSPOL_DUMP_LOGDBG ("get msg node from mem pool failed] pool=%p.", pool);
      return;
    }

  if (NULL == msg)
    {
      NSPOL_DUMP_LOGWAR ("get NULL msg node from mem pool] pool=%p.", pool);
      return;
    }

  if (!pack_msg (msg, buf, len, direction, para))
    {
      NSPOL_DUMP_LOGWAR ("get dump msg failed");
      return;
    }

  if (nsfw_mem_ring_enqueue (queue, msg) < 0)
    {
      NSPOL_DUMP_LOGWAR ("dump mem ring enqueue failed] ring=%p.", queue);
    }

  return;
}

NSTACK_STATIC inline bool
dump_enabled ()
{
  return (0 != g_dump_task_mask);
}

void
ntcpdump (void *buf, u32 buf_len, u16 direction)
{
  u32 i;
  if (!dump_enabled ())
    {
      return;
    }

  /* fix Dead-code type Codedex issue here */
  for (i = 0; i < MAX_DUMP_TASK; i++)
    {
      if (g_dump_task[i].task_state)
        {
          dump_enqueue (i, buf, buf_len, direction, pack_msg_inout, NULL);
        }
    }
}

void
ntcpdump_loop (void *buf, u32 buf_len, u16 direction, void *eth_addr)
{
  u32 i;

  if (!dump_enabled ())
    {
      return;
    }

  /* fix Dead-code type Codedex issue here */
  for (i = 0; i < MAX_DUMP_TASK; i++)
    {
      if (g_dump_task[i].task_state)
        {
          dump_enqueue (i, buf, buf_len, direction, pack_msg_loop, eth_addr);
        }
    }
}

// called by nStackMain
bool
dump_create_pool ()
{
  nsfw_mem_sppool pool_info;
  if (EOK !=
      MEMCPY_S (&pool_info.stname, sizeof (nsfw_mem_name),
                &g_dump_mem_pool_info, sizeof (nsfw_mem_name)))
    {
      NSPOL_DUMP_LOGERR ("create dump mem pool failed, MEMCPY_S failed.");
      return false;
    }

  pool_info.usnum = DUMP_MSG_NUM;
  pool_info.useltsize = DUMP_MSG_SIZE + sizeof (dump_msg_info);
  pool_info.isocket_id = NSFW_SOCKET_ANY;
  pool_info.enmptype = NSFW_MRING_MPSC;

  mring_handle pool = nsfw_mem_sp_create (&pool_info);
  if (NULL == pool)
    {
      NSPOL_DUMP_LOGERR ("create dump mem pool failed, pool create failed.");
      return false;
    }

  g_dump_task[0].task_pool = pool;

  NSPOL_DUMP_LOGINF ("dump pool create success] pool=%p.", pool);

  return true;
}

bool
dump_create_ring ()
{
  nsfw_mem_mring ring_info;
  if (EOK !=
      MEMCPY_S (&ring_info.stname, sizeof (nsfw_mem_name),
                &g_dump_mem_ring_info, sizeof (nsfw_mem_name)))
    {
      NSPOL_DUMP_LOGERR ("create dump mem ring failed, MEMCPY_S failed.");
      return false;
    }

  ring_info.usnum = DUMP_MSG_NUM;
  ring_info.isocket_id = NSFW_SOCKET_ANY;
  ring_info.enmptype = NSFW_MRING_MPSC;

  mring_handle ring = nsfw_mem_ring_create (&ring_info);
  if (NULL == ring)
    {
      NSPOL_DUMP_LOGERR ("create dump mem ring failed, ring create failed.");
      return false;
    }

  g_dump_task[0].task_queue = ring;

  NSPOL_DUMP_LOGINF ("dump ring create success] ring=%p.", ring);

  return true;
}

NSTACK_STATIC int
on_dump_tool_req (nsfw_mgr_msg * req)
{
  i16 task_id = 0;
  if (!req)
    {
      return -1;
    }
  if (req->src_proc_type != NSFW_PROC_TOOLS)
    {
      NSPOL_DUMP_LOGDBG
        ("dump module receive invaild message] module type=%u.",
         req->src_proc_type);
      return -1;
    }

  if (req->msg_type != MGR_MSG_TOOL_TCPDUMP_REQ)
    {
      NSPOL_DUMP_LOGDBG ("dump module receive invaild message] msg type=%u.",
                         req->msg_type);
      return -1;
    }

  nsfw_tool_dump_msg *dump_msg_req = GET_USER_MSG (nsfw_tool_dump_msg, req);

  switch (dump_msg_req->op_type)
    {
    case START_DUMP_REQ:
      task_id = get_dump_task (dump_msg_req);
      break;

    case STOP_DUMP_REQ:
      task_id = close_dump_task (dump_msg_req->task_id);
      break;

    default:
      task_id = -1;
    }

  nsfw_mgr_msg *rsp = nsfw_mgr_rsp_msg_alloc (req);
  if (NULL == rsp)
    {
      NSPOL_DUMP_LOGDBG ("alloc response for dump request failed.");
      return -1;
    }

  nsfw_tool_dump_msg *dump_msg_rsp = GET_USER_MSG (nsfw_tool_dump_msg, rsp);
  dump_msg_rsp->op_type = dump_msg_req->op_type + DUMP_MSG_TYPE_RSP;
  dump_msg_rsp->task_id = task_id;

  nsfw_mgr_send_msg (rsp);
  nsfw_mgr_msg_free (rsp);
  return 0;

}

NSTACK_STATIC int
on_dump_hbt_req (nsfw_mgr_msg * req)
{
  if (!req)
    {
      return -1;
    }
  if (req->src_proc_type != NSFW_PROC_TOOLS)
    {
      NSPOL_DUMP_LOGDBG
        ("dump module receive invaild message] module type=%u.",
         req->src_proc_type);
      return -1;
    }

  if (req->msg_type != MGR_MSG_TOOL_HEART_BEAT)
    {
      NSPOL_DUMP_LOGDBG ("dump module receive invaild message] msg type=%u.",
                         req->msg_type);
      return -1;
    }

  nsfw_tool_hbt *dump_hbt_req = GET_USER_MSG (nsfw_tool_hbt, req);

  i16 task_id = dump_hbt_req->task_id;
  if (task_id < 0 || task_id >= MAX_DUMP_TASK)
    {
      NSPOL_DUMP_LOGERR ("dump heart beat with invalid task id] task id=%d.",
                         task_id);
      return -1;
    }

  if (0 == g_dump_task[task_id].task_state)
    {
      NSPOL_DUMP_LOGDBG
        ("dump module receive heart beat but task not enabled] task id=%d.",
         task_id);
      return 0;
    }

  struct timespec cur_time;
  GET_CUR_TIME (&cur_time);     /*no need return value */

  // update task alive time
  g_dump_task[task_id].last_hbt_seq = dump_hbt_req->seq;
  g_dump_task[task_id].last_hbt_sec = cur_time.tv_sec;

  return 0;
}

NSTACK_STATIC int dump_tool_init (void *param);
NSTACK_STATIC int
dump_tool_init (void *param)
{
  u32 proc_type = (u32) ((long long) param);
  NSPOL_DUMP_LOGINF ("dump module init] proc type=%d", proc_type);

  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
      nsfw_mgr_reg_msg_fun (MGR_MSG_TOOL_TCPDUMP_REQ, on_dump_tool_req);
      nsfw_mgr_reg_msg_fun (MGR_MSG_TOOL_HEART_BEAT, on_dump_hbt_req);
      break;
    default:
      NSPOL_DUMP_LOGERR ("dump init with unknow module] proc type=%d",
                         proc_type);
      return -1;
    }

  clear_dump_task ();

  if (!dump_create_ring ())
    {
      return -1;
    }

  if (!dump_create_pool ())
    {
      return -1;
    }

  if (!dump_init_timer (&g_dump_timer))
    {
      return -1;
    }

  NSPOL_DUMP_LOGINF ("dump module init success.");
  return 0;
}

/* *INDENT-OFF* */
NSFW_MODULE_NAME (TCPDUMP_MODULE)
NSFW_MODULE_PRIORITY (10)
NSFW_MODULE_DEPENDS (NSTACK_DMM_MODULE)
NSFW_MODULE_INIT (dump_tool_init)
/* *INDENT-ON* */
