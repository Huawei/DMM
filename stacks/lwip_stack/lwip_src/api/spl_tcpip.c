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

#include "spl_opt.h"
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include "memp.h"
#include "mem.h"
#include "spl_pbuf.h"
//#include "sockets.h"
//#include <netinet/in.h>

#include "stackx_spl_share.h"
#include "spl_api.h"
#include "spl_tcpip.h"
#include "init.h"
#include "stackx/internal_msg.h"
#include "netif/sc_dpdk.h"
#include "sharedmemory.h"
#include "stackx_instance.h"
#include "netif/common.h"
//#include "nettool/nettool.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "spl_hal.h"
#include "spl_sbr.h"
#include "spl_tcpip.h"

#include "spl_instance.h"
#include "nsfw_mem_api.h"
#include "nsfw_msg_api.h"
#include "configuration_reader.h"

#include "nsfw_ps_api.h"
#include "alarm_api.h"
#include "nstack_share_res.h"
#include "spl_timers.h"

#include "etharp.h"
#include "raw.h"
#include "udp.h"
#include "tcp.h"
#include "igmp.h"
#include "memp.h"
#include "inet.h"
#include "sys_arch.h"

#include "sys.h"

#define NSTACK_MAIN_MAX_PARA 19
#define NSTACK_MAIN_MIN_PARA 1

#define DPDK_DEFAULT_EAL_MEM_SIZE (1024)

#define OPT_EAL_MEM_SIZE "--eal-mem-size="

/********************/
/* extern variables */
/********************/
extern mring_handle spl_get_msg_pool ();
extern mring_handle spl_get_conn_pool ();
extern u32 spl_get_conn_num ();
extern err_t ethernet_input (struct pbuf *p, struct netif *netif);

/* tcpip main thread sleep time, configured by user(nStackMain "-sleep" option) */
extern int g_tcpip_thread_sleep_time;

/* netif related */
extern struct stackx_port_info *head_used_port_list;

extern u32_t uStackArgIndex;

extern int g_tcpip_thread_stat;

/********************/
/* global variables */
/********************/

/* timer thread id */
sys_thread_t g_timerThread_id = 0;

/*Add an associative mapping relationship to p_stackx_instance based on lcore_id */

int globalArgc = 0;

/* Set different mem args to PAL and EAL */
char **gArgv = NULL;

int g_dpdk_argc = 0;
char **g_dpdk_argv = NULL;

/*The sum of the four queue processing messages does not exceed TASK_BURST*/
static u32 g_highestMboxNum = 12;
static u32 g_mediumMboxNum = 8;
static u32 g_lowestMboxNum = 4;
static u32 g_primaryMboxNum = 8;

/* [set ip_tos2prio size to IPTOS_TOS_MASK >> 1) + 1] */
u8_t ip_tos2prio[(IPTOS_TOS_MASK >> 1) + 1] = {
  TC_PRIO_BESTEFFORT,
  ECN_OR_COST (FILLER),
  TC_PRIO_BESTEFFORT,
  ECN_OR_COST (BESTEFFORT),
  TC_PRIO_BULK,
  ECN_OR_COST (BULK),
  TC_PRIO_BULK,
  ECN_OR_COST (BULK),
  TC_PRIO_INTERACTIVE,
  ECN_OR_COST (INTERACTIVE),
  TC_PRIO_INTERACTIVE,
  ECN_OR_COST (INTERACTIVE),
  TC_PRIO_INTERACTIVE_BULK,
  ECN_OR_COST (INTERACTIVE_BULK),
  TC_PRIO_INTERACTIVE_BULK,
  ECN_OR_COST (INTERACTIVE_BULK)
};

/********************/
/* extern functions */
/********************/
extern err_t ethernetif_init (struct netif *pnetif);
extern int nstack_stackx_init (int flag);
extern void tcp_sys_rmem_init ();
extern void ethernetif_packets_input (struct netif *pstnetif);

/********************************/
/* function forward declaration */
/********************************/

static inline u32 priority_sched_mbox (struct stackx_stack *share_memory,
                                       void **box, u32 n);
static inline char
rt_tos2priority (u8_t tos)
{
  return ip_tos2prio[IPTOS_TOS (tos) >> 1];
}

u64
timer_get_threadid ()
{
  return g_timerThread_id;
}

#if (DPDK_MODULE != 1)
NSTACK_STATIC inline void
do_recv_task (struct disp_netif_list *dnlist, u16 index_task)
{
  struct netifExt *pnetifExt = NULL;

  while (dnlist)
    {
      struct netif *currentNetif = dnlist->netif;
      dnlist = dnlist->next;

      pnetifExt = getNetifExt (currentNetif->num);
      if (NULL == pnetifExt)
        return;

      if (currentNetif != NULL && pnetifExt->num_packets_recv > index_task)
        {
          ethernetif_packets_input (currentNetif);
        }
    }
}

static inline int
is_valid_sleep_time (int sleep_time)
{
#define TCP_IP_THREAD_MAX_SLEEP_TIME 500
  if ((sleep_time < 0) || (sleep_time > TCP_IP_THREAD_MAX_SLEEP_TIME))
    {
      return 0;
    }

  return 1;
}

static int
thread_init ()
{
  sigset_t mask;

  if (0 != sigemptyset (&mask))
    {
      NSTCP_LOGERR ("sigemptyset function call error");
      return -1;
    }

  if (0 != sigaddset (&mask, SIGRTMIN))
    {
      NSTCP_LOGERR ("sigaddset function call error");
      return -1;
    }

  if ((pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL) != 0)
      || (pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL) != 0))
    {
      NSTCP_LOGERR ("pthread setcancel function call error");
      return -1;
    }

  if (pthread_sigmask (SIG_BLOCK, &mask, NULL) != 0)
    {
      NSPOL_LOGERR ("pthread_sigmask function call error");
      return -1;
    }

  return 0;
}

alarm_result
spl_socket_resource_check_func (void)
{

  mring_handle conn_ring = spl_get_conn_pool ();
  unsigned int rate_use = 0;
  alarm_result ret_alarm;
  u32 ring_size = 0, ring_not_used_count = 0;
  u32 socket_num = spl_get_conn_num ();

  ring_size = nsfw_mem_ring_size (conn_ring);

  if (0 == socket_num)
    {
      /* assign a valid id, then return */
      ret_alarm.alarm_id_get = ALARM_EVENT_MAX;
      return ret_alarm;
    }

  ring_not_used_count = nsfw_mem_ring_using_count (conn_ring);

  rate_use = (ring_size - ring_not_used_count) * 100 / socket_num;

  ret_alarm.alarm_id_get = ALARM_EVENT_NSTACK_RESOURCE_ALARM;

  if (rate_use >= USEAGE_HIGHT)
    {
      ret_alarm.alarm_flag_get = ALARM_PRODUCT;
      ret_alarm.alarm_id_get = ALARM_EVENT_NSTACK_RESOURCE_ALARM;
      NSPOL_LOGWAR (TCPIP_DEBUG,
                    "app using too much socket resources,the rate is bigger than 80%");
    }
  else if (rate_use <= USEAGE_LOW)
    {
      ret_alarm.alarm_flag_get = ALARM_CLEAN;
      ret_alarm.alarm_id_get = ALARM_EVENT_NSTACK_RESOURCE_ALARM;
    }
  else
    {
      ret_alarm.alarm_id_get = ALARM_EVENT_MAX;
    }

  ret_alarm.alarm_reason_get = ALARM_REASON_SOCKET;

  return ret_alarm;

}

alarm_result
spl_msg_buf_resource_check_func (void)
{

  mring_handle msg_ring = spl_get_msg_pool ();
  unsigned int rate_use = 0;
  alarm_result ret_alarm;
  u32 ring_size = 0, ring_not_used_count = 0;

  ring_size = nsfw_mem_ring_size (msg_ring);

  if (0 == ring_size)
    {
      /* assign a valid id, then return */
      ret_alarm.alarm_id_get = ALARM_EVENT_MAX;
      return ret_alarm;
    }

  ring_not_used_count = nsfw_mem_ring_using_count (msg_ring);

  rate_use = (ring_size - ring_not_used_count) * 100 / ring_size;

  ret_alarm.alarm_id_get = ALARM_EVENT_NSTACK_RESOURCE_ALARM;

  if (rate_use >= USEAGE_HIGHT)
    {
      ret_alarm.alarm_flag_get = ALARM_PRODUCT;
      ret_alarm.alarm_id_get = ALARM_EVENT_NSTACK_RESOURCE_ALARM;
    }
  else if (rate_use <= USEAGE_LOW)
    {
      ret_alarm.alarm_flag_get = ALARM_CLEAN;
      ret_alarm.alarm_id_get = ALARM_EVENT_NSTACK_RESOURCE_ALARM;
    }
  else
    {
      ret_alarm.alarm_id_get = ALARM_EVENT_MAX;
    }

  ret_alarm.alarm_reason_get = ALARM_REASON_MSG_BUF;

  return ret_alarm;

}

NSTACK_STATIC int
tcpip_thread_init ()
{
  if (thread_init () != 0)
    {
      return -1;
    }

  if (!is_valid_sleep_time (g_tcpip_thread_sleep_time))
    {
      g_tcpip_thread_sleep_time = 0;
    }

  alarm_para tcp_alarm_para;
  tcp_alarm_para.period_alarm.time_length = 5;  /* 5 second period */
  tcp_alarm_para.alarm_reason_count = 2;        /*both resource */
  tcp_alarm_para.func_alarm_check_period[0] = spl_socket_resource_check_func;
  tcp_alarm_para.alarm_reason_set[0] = ALARM_REASON_SOCKET;
  tcp_alarm_para.func_alarm_check_period[1] = spl_msg_buf_resource_check_func;
  tcp_alarm_para.alarm_reason_set[1] = ALARM_REASON_MSG_BUF;
  (void) ns_reg_alarm (ALARM_EVENT_NSTACK_RESOURCE_ALARM, ALARM_PERIOD_CHECK,
                       &tcp_alarm_para);

  ns_send_init_alarm (ALARM_EVENT_NSTACK_RESOURCE_ALARM);

  printmeminfo ();
  return 0;
}

sys_mbox_t
get_primary_box ()
{
  return &p_def_stack_instance->lstack.primary_mbox;
}

NSTACK_STATIC inline u16
tcpip_netif_recv (struct disp_netif_list * nif_list)
{

  u16 num_recv_task = 0, netif_packet_num;

  struct netif *tmpnetif = NULL;
  struct netifExt *pnetifExt = NULL;

  while (nif_list)
    {
      tmpnetif = nif_list->netif;

      netif_packet_num = spl_hal_recv (tmpnetif, 0);
      pnetifExt = getNetifExt (tmpnetif->num);
      if (NULL == pnetifExt)
        return 0;

      /* store the packet number in each netif */
      pnetifExt->num_packets_recv = netif_packet_num;

        /**
         * num_recv_task is the maximum packets of the netif among
         * all the netifs.
         */
      if (num_recv_task < netif_packet_num)
        {
          num_recv_task = netif_packet_num;
        }

      nif_list = nif_list->next;
    }

  return num_recv_task;
}

NSTACK_STATIC inline void
no_task_in_one_loop ()
{
}

#endif

/**
 * Pass a received packet to tcpip_thread for input processing
 *
 * @param p the received packet, p->payload pointing to the Ethernet header or
 *          to an IP header (if inp doesn't have NETIF_FLAG_ETHARP or
 *          SPL_NETIF_FLAG_ETHERNET flags)
 * @param inp the network interface on which the packet was received
 */
err_t
spl_tcpip_input (struct pbuf *p, struct netif *inp)
{
  err_t ret;
  NSPOL_LOGINF (TCPIP_DEBUG, "PACKET]p=%p,inp=%p", (void *) p, (void *) inp);
  print_pbuf_payload_info (p, false);

  /* every netif has been set NETIF_FLAG_ETHARP flag, no need else branch */
  ret = ethernet_input (p, inp);
  return ret;
}

int
_do_spl_callback_msg (data_com_msg * m)
{
  NSPOL_LOGDBG (TCPIP_DEBUG, "tcpip_thread: CALLBACK]msg=%p", (void *) m);

  m->param.err = ERR_OK;

  msg_internal_callback *callback = (msg_internal_callback *) (m->buffer);
  if (!callback->function)
    {
      NSTCP_LOGERR ("function ptr is null in SPL_TCPIP_MSG_CALLBACK msg");
      ASYNC_MSG_FREE (m);
      return 0;
    }

  callback->function (callback->ctx);
  ASYNC_MSG_FREE (m);

  return 0;
}

/*****************************************************************************
*   Prototype    : get_msgbox
*   Description  : According to the socket tos value to determine which priority queue to send
*   Input        : struct stackx_stack *sharedmemory, struct netconn *conn, enum api_msg_type type
*   Output       : Queue pointer
*   Return Value : Queue pointer
*   Calls        :
*   Called By    :
*
*****************************************************************************/
struct queue *
get_msgbox (int tos)
{
  struct stackx_stack *sharedmemory = &p_def_stack_instance->lstack;

  if ((MSG_PRIO_QUEUE_NUM < 3) || (0 == tos))
    {
      return &sharedmemory->primary_mbox;
    }

  char prio = rt_tos2priority ((u8_t) tos);
  if ((TC_PRIO_INTERACTIVE == prio))
    {
      NSPOL_LOGDBG (SOCKETS_DEBUG, "sent to the highest mbox.....");
      return &sharedmemory->priority_mbox[0];
    }
  else if ((TC_PRIO_BESTEFFORT == prio) || (TC_PRIO_INTERACTIVE_BULK == prio))
    {
      NSPOL_LOGDBG (SOCKETS_DEBUG, "sent to the medium mbox.....");
      return &sharedmemory->priority_mbox[1];
    }
  else if ((TC_PRIO_BULK == prio) || (TC_PRIO_FILLER == prio))
    {
      NSPOL_LOGDBG (SOCKETS_DEBUG, "sent to the lowest mbox.....");
      return &sharedmemory->priority_mbox[2];
    }

  NSPOL_LOGDBG (SOCKETS_DEBUG, "sent to the primary mbox.....");
  return &sharedmemory->primary_mbox;
}

/*****************************************************************************
*   Prototype    : priority_sched_mbox
*   Description  : According to the priority from the message queue to take
*   the message to deal with each cycle to take the total number of messages
*   not exceed to TASK_BURST
*   Input        : struct stackx_stack *sharedmemory, struct netconn *conn, enum api_msg_type type
*   Output       : Queue pointer
*   Return Value : Queue pointer
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline u32
priority_sched_mbox (struct stackx_stack *share_memory, void **box, u32 n)
{
  /* high:primary:medium:low -> 3:2:2:1(total:TASK_BURST) */
  if (unlikely
      ((g_highestMboxNum + g_mediumMboxNum + g_primaryMboxNum +
        g_lowestMboxNum) != n))
    {
      g_mediumMboxNum = n >> 2;
      g_lowestMboxNum = n >> 3;
      g_primaryMboxNum = n >> 2;
      g_highestMboxNum =
        n - g_mediumMboxNum - g_primaryMboxNum - g_lowestMboxNum;
    }

  u32 highestMboxNum = g_highestMboxNum;
  u32 primaryMboxNum = g_primaryMboxNum;
  u32 lowestMboxNum = g_lowestMboxNum;
  u32 mediumMboxNum = g_mediumMboxNum;

  u32 totalNum = 0;
  u32 num = 0;
  u32 oldLowestNum = 0;

  num = nsfw_mem_ring_dequeuev (share_memory->priority_mbox[0].llring,
                                (box + totalNum), highestMboxNum);
  if (unlikely (num > highestMboxNum))
    {
      num = highestMboxNum;
      NSTCP_LOGERR
        ("something wrong:num>highestMboxNum]num=%u,highestMboxNum=%u", num,
         highestMboxNum);
    }

  totalNum += num;

  u32 temp = highestMboxNum - num;
  /* bisect the left number of highest */
  primaryMboxNum += (temp >> 1);
  mediumMboxNum += temp - (temp >> 1);

  num = nsfw_mem_ring_dequeuev (share_memory->priority_mbox[1].llring,
                                (box + totalNum), mediumMboxNum);
  if (unlikely (num > mediumMboxNum))
    {
      num = mediumMboxNum;
      NSTCP_LOGERR
        ("something wrong.num>mediumMboxNum]num=%u,mediumMboxNum=%u", num,
         mediumMboxNum);
    }

  totalNum += num;
  primaryMboxNum += mediumMboxNum - num;        //occupy the left number of medium

  /* dynamically adjust g_mediumMboxNum and g_highestMboxNum, according to 'num' */
  oldLowestNum = g_mediumMboxNum;
  if (0 == num)
    {
      g_mediumMboxNum = 1;
    }
  else if (num < g_mediumMboxNum)
    {
      g_mediumMboxNum = num;
    }
  else
    {
      g_mediumMboxNum = n >> 2;
    }

  g_highestMboxNum += oldLowestNum - g_mediumMboxNum;

  num = nsfw_mem_ring_dequeuev (share_memory->primary_mbox.llring,
                                (box + totalNum), primaryMboxNum);
  if (unlikely (num > primaryMboxNum))
    {
      num = primaryMboxNum;
      NSTCP_LOGERR
        ("something wrong.num>primaryMboxNum]num=%u,primaryMboxNum=%u", num,
         primaryMboxNum);
    }

  totalNum += num;
  lowestMboxNum += primaryMboxNum - num;        //occupy the left number of primary

  /* dynamically adjust g_primaryMboxNum and g_highestMboxNum, according to 'num' */
  oldLowestNum = g_primaryMboxNum;
  if (0 == num)
    {
      g_primaryMboxNum = 1;
    }
  else if (num < g_primaryMboxNum)
    {
      g_primaryMboxNum = num;
    }
  else
    {
      g_primaryMboxNum = n >> 2;
    }

  g_highestMboxNum += oldLowestNum - g_primaryMboxNum;

  if (lowestMboxNum > (n - totalNum))
    {
      lowestMboxNum = n - totalNum;
    }

  num = nsfw_mem_ring_dequeuev (share_memory->priority_mbox[2].llring,
                                (box + totalNum), lowestMboxNum);
  if (unlikely (num > lowestMboxNum))
    {
      num = lowestMboxNum;
      NSTCP_LOGERR
        ("something wrong.num>lowestMboxNum]num=%u,lowestMboxNum=%u", num,
         lowestMboxNum);
    }

  totalNum += num;

  /* dynamically adjust g_lowestMboxNum and g_highestMboxNum, according to 'num' */
  oldLowestNum = g_lowestMboxNum;
  if (0 == num)
    {
      g_lowestMboxNum = 1;
    }
  else if (num < g_lowestMboxNum)
    {
      g_lowestMboxNum = num;
    }
  else
    {
      g_lowestMboxNum = n >> 3;
    }

  g_highestMboxNum += oldLowestNum - g_lowestMboxNum;

  return totalNum;
}

/**
 * call ltt_apimsg in STACKX TIMER THREAD
 *
 * @param h function to be called on timeout
 * @param arg argument to pass to timeout function h
 * @return error code given back by the function that was called
 */
err_t
ltt_apimsg (sys_timeout_handler h, void *arg)
{
  data_com_msg *p_msg_entry;
  sys_mbox_t mbox = get_primary_box ();
  if (sys_mbox_valid (&mbox))
    {
      if (-1 == spl_msg_malloc (&p_msg_entry))
        {
          NSPOL_LOGERR ("ltt_apimsg:spl_msg_malloc failed.");
          return -1;
        }

      p_msg_entry->param.module_type = MSG_MODULE_TIMER;
      p_msg_entry->param.major_type = SPL_TCPIP_MSG_TIMER;
      p_msg_entry->param.minor_type = TIMER_MSG_TIMEOUT;
      p_msg_entry->param.op_type = MSG_ASYN_POST;

      sys_sem_init (&p_msg_entry->param.op_completed);

      msg_timer *tmsg = (msg_timer *) (p_msg_entry->buffer);
      tmsg->act = h;
      tmsg->arg = arg;

      if (msg_post (p_msg_entry, mbox->llring) < 0)
        {
          NSPOL_LOGERR
            ("msg post is failed]module_type=%u, major_type=%u, minor_type=%u",
             p_msg_entry->param.module_type, p_msg_entry->param.major_type,
             p_msg_entry->param.minor_type);
          spl_msg_free (p_msg_entry);
          return ERR_VAL;
        }

      return ERR_OK;
    }

  NSPOL_LOGERR ("mbox is invalid");
  return ERR_VAL;
}

int
_do_spl_timer_msg (data_com_msg * m)
{
  NSPOL_LOGDBG (TESTSOCKET_DEBUG | STACKX_DBG_TRACE,
                "the msg is from TIMER module, minor(%u)",
                m->param.minor_type);
  return 0;
}

static int
_do_timeout_handle (data_com_msg * m)
{
  m->param.err = ERR_OK;
  msg_timer *tmo_msg = (msg_timer *) m->buffer;
  if (!tmo_msg->act)
    {
      NSTCP_LOGERR ("TIMER_MSG_TIMEOUT msg act is NULL");
      ASYNC_MSG_FREE (m);
      return -1;
    }

  timeout_phandler (tmo_msg->act, tmo_msg->arg);
  ASYNC_MSG_FREE (m);
  return 0;
}

static int
_do_clear_timer (data_com_msg * m)
{
  NSTCP_LOGDBG ("TIMER_CLEAR API]msg=%p", (void *) m);

  msg_timer *handle = (msg_timer *) (m->buffer);
  if (!handle->act)
    {
      NSTCP_LOGERR ("function ptr is null in TIMER_MSG_CLEAR msg");
      SET_MSG_ERR (m, ERR_VAL);
      return 0;
    }

  //stackxTcpPcbClearTimer((struct tcp_pcb *)handle->act, (unsigned long)handle->arg);
  spl_msg_free (m);

  return 0;
}

err_t
ltt_clearTmrmsg (void *pcb, void *arg)
{
  data_com_msg *p_msg_entry;
  sys_mbox_t mbox = get_primary_box ();
  if (sys_mbox_valid (&mbox))
    {
      if (-1 == spl_msg_malloc (&p_msg_entry) || (NULL == p_msg_entry))
        {
          NSPOL_LOGERR ("ltt_clearTmrmsg:spl_msg_malloc failed.");
          return -1;
        }

      p_msg_entry->param.module_type = MSG_MODULE_TIMER;
      p_msg_entry->param.major_type = SPL_TCPIP_MSG_TIMER;
      p_msg_entry->param.minor_type = TIMER_MSG_CLEAR;
      p_msg_entry->param.op_type = MSG_ASYN_POST;
      p_msg_entry->param.receiver = 0;
      sys_sem_init (&p_msg_entry->param.op_completed);

      msg_timer *tmo = (msg_timer *) (p_msg_entry->buffer);
      tmo->act = pcb;
      tmo->arg = arg;

      if (msg_post (p_msg_entry, mbox->llring) < 0)
        {
          NSPOL_LOGERR
            ("msg post is failed]module_type=%u, major_type=%u, minor_type=%u",
             p_msg_entry->param.module_type, p_msg_entry->param.major_type,
             p_msg_entry->param.minor_type);
          spl_msg_free (p_msg_entry);
          return ERR_VAL;
        }

      return ERR_OK;
    }

  NSPOL_LOGERR ("mbox is invalid");
  return ERR_VAL;
}

#if STACKX_NETIF_API
err_t
tcpip_netif_post (data_com_msg * m)
{
  err_t err = ERR_OK;
  sys_mbox_t mbox = get_primary_box ();

  if (NULL == mbox || !sys_mbox_valid (&mbox))
    {
      NSPOL_LOGERR ("mbox not initialed well");
      err = ERR_MEM;
      goto err_exit;
    }

  if (NULL == mbox->llring)
    {
      int tryCount = 0;
#define TCP_NETIF_WAITINIT_MAX_COUNT 10
      NSPOL_LOGWAR (TCPIP_DEBUG, "mbox->llring not initialed yet...");
      sys_sleep_ns (0, 100000000);
      while (!mbox->llring && (++tryCount < TCP_NETIF_WAITINIT_MAX_COUNT))
        {
          NSPOL_LOGWAR (TCPIP_DEBUG, "mbox->llring not initialed yet...");
          sys_sleep_ns (0, 100000000);
        }

      if (NULL == mbox->llring)
        {
          NSPOL_LOGERR ("failed to get a valid mbox->llring");
          err = ERR_MEM;
          goto err_exit;
        }
    }

  NSPOL_LOGINF (TCPIP_DEBUG, "post tcpip_netif_msg...");

  if (msg_post_with_lock_rel (m, mbox->llring) < 0)
    {
      err = ERR_VAL;
    }
  else
    {
      err = m->param.err;
    }

err_exit:
  /* it's a sync message */
  spl_msg_free (m);
  return err;
}

/**
 * Much like tcpip_apimsg, but calls the lower part of a netifapi_*
 * function.
 *
 * @param netifapimsg a struct containing the function to call and its parameters
 * @return error code given back by the function that was called
 */
err_t
tcpip_netif_add (msg_add_netif * tmp)
{
  data_com_msg *p_msg_entry = NULL;
  msg_add_netif *nmsg = NULL;

  if (spl_msg_malloc (&p_msg_entry) == -1)
    {
      NSPOL_LOGERR ("tcpip_netifapi:spl_msg_malloc failed.");
      return ERR_MEM;
    }

  p_msg_entry->param.module_type = MSG_MODULE_HAL;
  p_msg_entry->param.major_type = SPL_TCPIP_MSG_NETIFAPI;
  p_msg_entry->param.minor_type = NETIF_DO_ADD;
  p_msg_entry->param.op_type = MSG_SYN_POST;
  p_msg_entry->param.receiver = 0;
  sys_sem_init (&p_msg_entry->param.op_completed);

  nmsg = (msg_add_netif *) p_msg_entry->buffer;
  nmsg->function = tmp->function;
  nmsg->netif = tmp->netif;
  nmsg->ipaddr = tmp->ipaddr;
  nmsg->netmask = tmp->netmask;
  nmsg->gw = tmp->gw;
  nmsg->state = tmp->state;
  nmsg->init = tmp->init;
  nmsg->input = tmp->input;
  nmsg->voidfunc = tmp->voidfunc;

  return tcpip_netif_post (p_msg_entry);
}

#endif /* STACKX_NETIF_API */

/* Added for congestion control, maybe not used  end */

extern err_t init_ptimer (void);
extern struct cfg_item_info g_cfg_item_info[CFG_SEG_MAX][MAX_CFG_ITEM];

/*=========== set share config for nStackMain =============*/
static inline mzone_handle
create_mem_zone (nsfw_mem_zone * zone_info)
{
  return nsfw_mem_zone_create (zone_info);
}

static inline int
set_zone_info (nsfw_mem_zone * zone_info, nsfw_mem_name * name_info, u32 len)
{
  if (EOK !=
      MEMCPY_S (&(zone_info->stname), sizeof (nsfw_mem_name), name_info,
                sizeof (nsfw_mem_name)))
    {
      NSPOL_DUMP_LOGERR ("create pool failed, MEMCPY_S failed.");
      return -1;
    }

  zone_info->length = len;
  zone_info->isocket_id = NSFW_SOCKET_ANY;

  return 0;
}

int
set_share_config ()
{
  static nsfw_mem_name g_cfg_mem_info =
    { NSFW_SHMEM, NSFW_PROC_MAIN, NSTACK_SHARE_CONFIG };

  nsfw_mem_zone zone_info;
  if (set_zone_info (&zone_info, &g_cfg_mem_info, get_cfg_share_mem_size ()) <
      0)
    {
      return -1;
    }

  mzone_handle base_cfg_mem = create_mem_zone (&zone_info);
  if (NULL == base_cfg_mem)
    {
      NSPOL_LOGERR ("get config share mem failed.");
      return -1;
    }

  if (set_share_cfg_to_mem (base_cfg_mem) < 0)
    {
      NSPOL_LOGERR ("set share config failed.");
      return -1;
    }

  return 0;
}

int
init_by_tcpip_thread ()
{
  if (spl_init_app_res () != 0)
    {
      NSPOL_LOGERR ("spl_init_app_res failed");
      return -1;
    }

  if (init_instance () != 0)
    {
      return -1;
    }
  if (tcpip_thread_init () != 0)
    {
      NSTCP_LOGERR ("tcpip_thread_init failed!");
      return -1;
    }

  init_stackx_lwip ();

  return 0;
}

void
spl_tcpip_thread (void *arg)
{
  g_tcpip_thread_stat = 1;

  if (init_by_tcpip_thread () != 0)
    {
      return;
    }

  struct stackx_stack *share_memory = &p_def_stack_instance->lstack;
  struct disp_netif_list **iflist = &p_def_stack_instance->netif_list;

  u32 run_count = 0;
  u16 task_loop;
  u16 num_recv_task = 0, num_send_timer_task = 0;
  u16 index_task = 0;
  int tcpip_thread_sleep_interval = g_tcpip_thread_sleep_time * 1000;
  void *task_queue[TASK_BURST];
  data_com_msg *msg;

  while (1)
    {
      /* try to receive packet from hal layer */
      num_recv_task = tcpip_netif_recv (*iflist);

      /* try to receive message from sbr layer */
      num_send_timer_task = priority_sched_mbox (share_memory,
                                                 task_queue, TASK_BURST);

      if (num_recv_task)
        {
          NSPOL_LOGINF (TCPIP_DEBUG, "num_recv_task %d", num_recv_task);
        }

      /* Statistics the total number of messages */

      /* Separate statistics on socket api messages */
      /* handle the request from upper layer and lower layer one by one */
      task_loop = (num_send_timer_task > num_recv_task ?
                   num_send_timer_task : num_recv_task);
      if (task_loop == 0)
        {
          no_task_in_one_loop ();

          if (run_count++ > 20)
            {
              sys_sleep_ns (0, tcpip_thread_sleep_interval);
            }
          continue;
        }

      run_count = 0;
      index_task = 0;

      /* at least one task to be handled */
      while (task_loop > index_task)
        {
          /* handle one packet from hal layer (for multi-nic case, do it for each) */
          if (num_recv_task > index_task)
            {
              do_recv_task (*iflist, index_task);
            }

          /* handle one message from sbr layer */
          if (num_send_timer_task > index_task)
            {
              msg = (data_com_msg *) (task_queue[index_task]);

              spl_process (msg);
            }

          do_update_pcbstate ();

          index_task++;
        }
    }
}

int
create_tcpip_thread ()
{
  int ret;
  u64 arg = 0;

  /* main tcpip thread */
  char thread_name[32] = TCPIP_THREAD_NAME;

  sys_thread_t thread = sys_thread_new (thread_name, spl_tcpip_thread,
                                        (void *) arg,
                                        TCPIP_THREAD_STACKSIZE,
                                        g_cfg_item_info[CFG_SEG_PRI]
                                        [CFG_SEG_THREAD_PRI_PRI].value);

  cpu_set_t cpuset;
  CPU_ZERO (&cpuset);           /*lint !e534 */
  CPU_SET (g_nstack_bind_cpu, &cpuset); /*lint !e522 */
  ret = pthread_setaffinity_np (thread, sizeof (cpuset), &cpuset);
  if (ret != 0)
    {
      NSPOL_LOGERR ("pthread_setaffinity_np failed]thread_name=%s,ret=%d",
                    TCPIP_THREAD_NAME, ret);
    }

  return 0;
}

int
create_timer_thread ()
{
#if 1
  if (init_ptimer () != ERR_OK)
    {
      NSPOL_LOGERR ("init_ptimer failed");
      return -1;
    }

  sys_thread_t thread_timer =
    sys_thread_new (PTIMER_THREAD_NAME, ptimer_thread, NULL,
                    TCPIP_THREAD_STACKSIZE, 0);
  cpu_set_t cpuset_timer;
  CPU_ZERO (&cpuset_timer);
  CPU_SET (1, &cpuset_timer);
  int ret = pthread_setaffinity_np (thread_timer, sizeof (cpuset_timer),
                                    &cpuset_timer);
  if (ret != 0)
    {
      NSPOL_LOGERR ("TCP init Timer Trhead Failed!");
    }

  g_timerThread_id = thread_timer;
#endif
  return 0;
}

int
init_ip_module_reader ()
{
  output_api api = { 0 };
  api.post_to = post_ip_module_msg;
  api.add_netif_ip = add_netif_ip;
  api.del_netif_ip = del_netif_ip;
  regist_output_api (&api);

  if (init_configuration_reader () < 0)
    {
      NSPOL_LOGERR ("init_configuration_reader failed");
      return -1;
    }

  return 0;
}

extern int init_unmatch_version (void);
extern int init_stackx_global_tick (void);

int
init_by_main_thread ()
{
  NSPOL_LOGINF (TCPIP_DEBUG, "init_by_main_thread start version 1.4");

  uStackArgIndex++;
  if (spl_hal_init (g_dpdk_argc, (char **) g_dpdk_argv) < 0)
    {
      NSPOL_LOGERR ("spl_hal_init failed");
      return -1;
    }
  NSPOL_LOGINF (TCPIP_DEBUG, "stage 1");

  if (init_unmatch_version () != 0)
    {
      return -1;
    }

  NSPOL_LOGINF (TCPIP_DEBUG, "stage 2");
  if (init_stackx_global_tick () != 0)
    {
      return -1;
    }
  NSPOL_LOGINF (TCPIP_DEBUG, "stage 3");

  if (spl_init_group_array () != 0)
    {
      NSPOL_LOGERR ("spl_init_group_array failed");
      return -1;
    }
  NSPOL_LOGINF (TCPIP_DEBUG, "stage 4");

  if (set_share_config () != 0)
    {
      NSPOL_LOGERR ("set_share_config failed.");
      return -1;
    }
  NSPOL_LOGINF (TCPIP_DEBUG, "stage 5");

  if (create_timer_thread () != 0)
    {
      //NSPOL_LOGERR(TCPIP_DEBUG,"init_timer_thread failed.");
      return -1;
    }
  NSPOL_LOGINF (TCPIP_DEBUG, "stage 6");

  if (create_tcpip_thread () != 0)
    {
      NSPOL_LOGERR ("init_tcpip_thread failed.");
      return -1;
    }
  NSPOL_LOGINF (TCPIP_DEBUG, "stage 7");

  if (init_ip_module_reader () != 0)
    {
      return -1;
    }
  NSPOL_LOGINF (TCPIP_DEBUG, "stage end");

  return 0;
}

void
create_netif (struct stackx_port_info *p_port_info)
{
  int ret;
  struct spl_ip_addr ip, mask, gw;
  struct netifExt *netifEx = NULL;
  struct netif *netif = malloc (sizeof (struct netif));
  if (!netif)
    {
      NSPOL_LOGERR ("malloc failed");
      return;
    }

  if (MEMSET_S (netif, sizeof (struct netif), 0, sizeof (struct netif)) < 0)
    {
      NSPOL_LOGERR ("MEMSET_S failed");
      goto error;
    }

  NSPOL_LOGINF (TCPIP_DEBUG, "I get netif]ip=%s,gw=%s,mask=%s,name=%c %c",
                p_port_info->linux_ip.ip_addr_linux,
                p_port_info->linux_ip.ip_addr_linux,
                p_port_info->linux_ip.mask_linux,
                netif->name[0], netif->name[1]);

  if (inet_aton (p_port_info->linux_ip.ip_addr_linux, &ip)
      && inet_aton (p_port_info->linux_ip.ip_addr_linux, &gw)
      && inet_aton (p_port_info->linux_ip.mask_linux, &mask))
    {
      ret =
        spl_netifapi_netif_add (netif, &ip, &mask, &gw, NULL, ethernetif_init,
                                spl_tcpip_input, netif_set_up);
      if (ERR_OK != ret)
        {
          NSPOL_LOGERR ("netifapi_netif_add failed]ret=%d", ret);
          goto error;
        }
#if 1
      if (0 != netifExt_add (netif))
        return;

#endif
      netifEx = getNetifExt (netif->num);
      if (NULL == netifEx)
        return;

      netifEx->hdl = p_port_info->linux_ip.hdl;
      ret =
        STRCPY_S (netifEx->if_name, sizeof (netifEx->if_name),
                  p_port_info->linux_ip.if_name);
      if (EOK != ret)
        {
          NSPOL_LOGERR ("STRCPY_S failed]ret=%d", ret);
          goto error;
        }
      return;
    }

error:
  free (netif);
}

int
post_ip_module_msg (void *arg, ip_module_type Type,
                    ip_module_operate_type operate_type)
{
  data_com_msg *p_msg_entry;
  msg_ip_module *imsg;
  sys_mbox_t mbox = get_primary_box ();

  if (!mbox)
    {
      NSOPR_LOGERR ("get_cur_mbox failed");
      return ERR_MEM;
    }

  if (sys_mbox_valid (&mbox))
    {
      if (spl_msg_malloc (&p_msg_entry) == -1)
        {
          NSOPR_LOGERR ("ip_route_apimsg:spl_msg_malloc failed.");
          return ERR_MEM;
        }

      sys_sem_init (&p_msg_entry->param.op_completed);
      p_msg_entry->param.module_type = MSG_MODULE_IP;
      p_msg_entry->param.op_type = MSG_SYN_POST;

      imsg = (msg_ip_module *) (p_msg_entry->buffer);
      imsg->arg = arg;
      imsg->type = Type;
      imsg->operate_type = operate_type;

      NSOPR_LOGINF ("post ip_module msg to tcpip_thread]action=%d,type=%d",
                    operate_type, Type);

      if (msg_post (p_msg_entry, mbox->llring) != 0)
        {
          NSOPR_LOGERR
            ("msg_post failed,this can not happen]action=%d,type=%d",
             operate_type, Type);
        }

      spl_msg_free (p_msg_entry);

      return ERR_OK;
    }

  NSOPR_LOGERR ("mbox is invalid");
  return ERR_VAL;
}

int
process_ip_module_msg (void *arg, ip_module_type module_type,
                       ip_module_operate_type operate_type)
{
  int retval = process_configuration (arg, module_type, operate_type);

  NSOPR_LOGINF ("tcpip_thread: ip_module cmd exec over, send SYNC_MSG_ACK");
  return retval;
}

int
init_new_network_configuration ()
{
  if (spl_hal_port_init () < 0)
    {
      return -1;
    }

  return 0;
}

static int
_process_ip_module_msg (data_com_msg * m)
{
  m->param.err = ERR_OK;
  msg_ip_module *_m = (msg_ip_module *) m->buffer;
  int ret = process_ip_module_msg (_m->arg, _m->type, _m->operate_type);
  SYNC_MSG_ACK (m);
  return ret;
}

int
spl_post_msg (u16 mod, u16 maj, u16 min, u16 op, char *data, u16 data_len,
              u32 src_pid)
{
  data_com_msg *p_msg_entry;

  sys_mbox_t mbox = get_primary_box ();
  if (!sys_mbox_valid (&mbox))
    {
      NSPOL_LOGERR
        ("get mbox null!]mod=%u,maj=%u,min=%u,op=%u,data=%p,len=%u", mod, maj,
         min, op, data, data_len);
      return ERR_MEM;
    }

  if (spl_msg_malloc (&p_msg_entry) == -1)
    {
      NSPOL_LOGERR ("get msg null!]mod=%u,maj=%u,min=%u,op=%u,data=%p,len=%u",
                    mod, maj, min, op, data, data_len);
      return ERR_MEM;
    }

  sys_sem_init (&p_msg_entry->param.op_completed);
  p_msg_entry->param.module_type = mod;
  p_msg_entry->param.major_type = maj;
  p_msg_entry->param.minor_type = min;
  p_msg_entry->param.op_type = op;
  p_msg_entry->param.src_pid = src_pid;

  int retVal;
  if (NULL != data)
    {
      retVal =
        MEMCPY_S ((char *) p_msg_entry->buffer, sizeof (p_msg_entry->buffer),
                  data, data_len);
      if (EOK != retVal)
        {
          NSPOL_LOGERR ("MEMCPY_S failed %d.", retVal);
          spl_msg_free (p_msg_entry);
          return ERR_MEM;
        }
    }

  if (msg_post_with_lock_rel (p_msg_entry, mbox->llring) != 0)
    {
      NSPOL_LOGERR
        ("post msg error!]mod=%u,maj=%u,min=%u,op=%u,data=%p,len=%u", mod,
         maj, min, op, data, data_len);
      spl_msg_free (p_msg_entry);
      return ERR_MEM;
    }

  if (MSG_SYN_POST == op)
    {
      spl_msg_free (p_msg_entry);
    }

  NSOPR_LOGDBG ("post msg suc!]mod=%u,maj=%u,min=%u,op=%u,data=%p,len=%u",
                mod, maj, min, op, data, data_len);
  return ERR_OK;
}

/*
* Added by eliminate duplicated code degree, the function is moved to the public places
*adjust memory size for pal and eal mem size: this func do the following things:
*1. copy argv to gArgv and g_dpdk_argv
*2. remove OPT_EAL_MEM_SIZE option so that the rtp and rte options process won't reprt unrecognized option error.
*3. set eal mem size and pal_mem_size = mem - eal_mem_size
*/
int
adjust_mem_arg (int argc, char *argv[])
{
  int i = 0;
  int j = 0;
  int retVal;
  char *tmp = NULL;
  char *tmp2 = NULL;
  int arg_mem_index = -1;
  char *saveptr1 = NULL;
  int mem_size = 0;
  int mem_size_parsed = 0;      // if multi -m argument is set, then only deal with the first one
  int eal_mem_size = DPDK_DEFAULT_EAL_MEM_SIZE; // Default

  if ((argc < NSTACK_MAIN_MIN_PARA) || (argc > NSTACK_MAIN_MAX_PARA))
    {
      NSPOL_LOGERR ("The number of parameters is incorrect");
      return -1;
    }

  globalArgc = argc;
  g_dpdk_argc = argc;
  gArgv = (char **) malloc (sizeof (char *) * argc);
  if (gArgv == NULL)
    {
      NSPOL_LOGERR ("Failed to alloc memory for adjust mem args\n");
      goto ERROR_INIT;
    }

  g_dpdk_argv = (char **) malloc (sizeof (char *) * argc);
  if (g_dpdk_argv == NULL)
    {
      NSPOL_LOGERR ("Failed to alloc memory for adjust mem args\n");
      goto ERROR_INIT;
    }

  retVal =
    MEMSET_S (gArgv, sizeof (char *) * argc, 0, sizeof (char *) * argc);
  if (EOK != retVal)
    {
      NSPOL_LOGERR ("MEMSET_S failed %d.", retVal);
      goto ERROR_INIT;
    }
  retVal =
    MEMSET_S (g_dpdk_argv, sizeof (char *) * argc, 0, sizeof (char *) * argc);
  if (EOK != retVal)
    {
      NSPOL_LOGERR ("MEMSET_S failed %d.", retVal);
      goto ERROR_INIT;
    }

  for (i = 0; i < argc; i++)
    {
      if (!strcmp ("-m", argv[i]) && !mem_size_parsed && (i + 1 < argc))
        {
          gArgv[j] = argv[i];
          g_dpdk_argv[j] = argv[i];
          i++;
          j++;
          gArgv[j] = (char *) malloc (32);
          g_dpdk_argv[j] = (char *) malloc (32);
          /* gArgv[j] is NULL and g_dpdk_argv[j] isnot NULL, need free g_dpdk_argv[j] */
          arg_mem_index = j;
          if ((!gArgv[j]) || (!g_dpdk_argv[j]))
            {
              NSPOL_LOGERR ("malloc failed.");
              goto ERROR_PARSE_MALLOC;
            }
          mem_size = atoi (argv[i]);
          /* add memory range check,avoid handle wrongly later begin */
          if (mem_size < 0)
            {
              goto ERROR_PARSE_MALLOC;
            }

          j++;
          mem_size_parsed = 1;
        }
      else
        if (!strncmp (OPT_EAL_MEM_SIZE, argv[i], strlen (OPT_EAL_MEM_SIZE)))
        {
          tmp = strdup (argv[i]);
          if (tmp == NULL)
            {
              goto ERROR_PARSE_MALLOC;
            }
          /* Always use re-entrant functions in multi-threaded environments */
          tmp2 = strtok_r (tmp, "=", &saveptr1);
          tmp2 = strtok_r (NULL, "=", &saveptr1);
          if (tmp2)
            {
              eal_mem_size = atoi (tmp2);
              /* add memory range check,avoid handle wrongly later */
              if (eal_mem_size < 0)
                {
                  free (tmp);
                  goto ERROR_PARSE_MALLOC;
                }
            }
          free (tmp);
          tmp = NULL;
          /*remove this option since dpdk can't recognize it and may cause error. */
          /*so we should deduct the count by 1. */
          globalArgc -= 1;
          g_dpdk_argc -= 1;
        }
      else
        {
          gArgv[j] = argv[i];
          g_dpdk_argv[j] = argv[i];
          j++;
        }
    }

  /* -m arg is set */
  if (arg_mem_index >= 0)
    {
      retVal =
        SPRINTF_S (gArgv[arg_mem_index], 32, "%d", mem_size - eal_mem_size);
      if (-1 == retVal)
        {
          NSPOL_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
          goto ERROR_PARSE_MALLOC;
        }
      retVal = SPRINTF_S (g_dpdk_argv[arg_mem_index], 32, "%d", eal_mem_size);
      if (-1 == retVal)
        {
          NSPOL_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
          goto ERROR_PARSE_MALLOC;
        }
    }
  else
    {
      // do nothing for no mem arg and leave it for default process of pal and eal.
    }

  return 0;
ERROR_PARSE_MALLOC:
  if (arg_mem_index >= 0 && gArgv[arg_mem_index])
    {
      free (gArgv[arg_mem_index]);
      gArgv[arg_mem_index] = NULL;
    }

  if (arg_mem_index >= 0 && g_dpdk_argv[arg_mem_index])
    {
      free (g_dpdk_argv[arg_mem_index]);
      g_dpdk_argv[arg_mem_index] = NULL;
    }

ERROR_INIT:
  if (gArgv)
    {
      free (gArgv);
      gArgv = NULL;
      globalArgc = 0;
    }

  if (g_dpdk_argv)
    {
      free (g_dpdk_argv);
      g_dpdk_argv = NULL;
      g_dpdk_argc = 0;
    }

  return -1;
}

REGIST_MSG_MODULE_FUN (MSG_MODULE_IP, _process_ip_module_msg);

REGIST_MSG_MODULE_MAJOR_FUN (MSG_MODULE_SPL, SPL_TCPIP_MSG_CALLBACK,
                             _do_spl_callback_msg);

REGIST_MSG_MODULE_MAJOR_FUN (MSG_MODULE_TIMER, SPL_TCPIP_MSG_TIMER,
                             _do_spl_timer_msg);

REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_TIMER, SPL_TCPIP_MSG_TIMER,
                                   TIMER_MSG_TIMEOUT, _do_timeout_handle);

REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_TIMER, SPL_TCPIP_MSG_TIMER,
                                   TIMER_MSG_CLEAR, _do_clear_timer);
