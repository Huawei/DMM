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

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/timerfd.h>

#include "types.h"
#include "list.h"

#include "common_mem_common.h"

#include "nstack_securec.h"
#include "nsfw_init.h"
#include "nsfw_mgr_com_api.h"
#include "nsfw_mem_api.h"
#include "nstack_log.h"
#include "nsfw_base_linux_api.h"
#include "nsfw_fd_timer_api.h"
#include "nsfw_maintain_api.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSFW_TIMER_CYCLE 1
#define NSFW_TIMER_INFO_MAX_COUNT_DEF 8191
#define NSFW_TIMER_INFO_MAX_COUNT (g_timer_cfg.timer_info_size)
/* *INDENT-OFF* */
nsfw_timer_init_cfg g_timer_cfg;

u8 g_hbt_switch = FALSE;
/* *INDENT-ON* */

/*****************************************************************************
*   Prototype    : nsfw_timer_reg_timer
*   Description  : reg timer with callback function when timeout
*   Input        : u32 timer_type
*                  void* data
*                  nsfw_timer_proc_fun fun
*                  struct timespec time_left
*   Output       : None
*   Return Value : nsfw_timer_info*
*   Calls        :
*   Called By    :
*
*****************************************************************************/
nsfw_timer_info *
nsfw_timer_reg_timer (u32 timer_type, void *data,
                      nsfw_timer_proc_fun fun, struct timespec time_left)
{
  nsfw_timer_info *tm_info = NULL;
  if (0 ==
      nsfw_mem_ring_dequeue (g_timer_cfg.timer_info_pool, (void *) &tm_info))
    {
      NSFW_LOGERR ("dequeue error]data=%p,fun=%p", data, fun);
      return NULL;
    }

  if (EOK != MEMSET_S (tm_info, sizeof (*tm_info), 0, sizeof (*tm_info)))
    {
      if (0 == nsfw_mem_ring_enqueue (g_timer_cfg.timer_info_pool, tm_info))
        {
          NSFW_LOGERR ("enqueue error]data=%p,fun=%p", data, fun);
        }
      NSFW_LOGERR ("mem set error]data=%p,fun=%p", data, fun);
      return NULL;
    }

  tm_info->fun = fun;
  tm_info->argv = data;
  tm_info->time_left = time_left;
  //tm_info->time_left.tv_sec += NSFW_TIMER_CYCLE;
  tm_info->timer_type = timer_type;
  list_add_tail (&tm_info->node, &g_timer_cfg.timer_head);
  tm_info->alloc_flag = TRUE;
  return tm_info;
}

/*****************************************************************************
*   Prototype    : nsfw_timer_rmv_timer
*   Description  : stop timer
*   Input        : nsfw_timer_info* tm_info
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
nsfw_timer_rmv_timer (nsfw_timer_info * tm_info)
{
  if (NULL == tm_info)
    {
      NSFW_LOGWAR ("tm_info nul");
      return;
    }

  if (FALSE == tm_info->alloc_flag)
    {
      NSFW_LOGERR ("tm_info refree]tm_info=%p,argv=%p,fun=%p", tm_info,
                   tm_info->argv, tm_info->fun);
      return;
    }

  tm_info->alloc_flag = FALSE;
  list_del (&tm_info->node);
  if (0 == nsfw_mem_ring_enqueue (g_timer_cfg.timer_info_pool, tm_info))
    {
      NSFW_LOGERR ("tm_info free failed]tm_info=%p,argv=%p,fun=%p", tm_info,
                   tm_info->argv, tm_info->fun);
    }
  return;
}

/*****************************************************************************
*   Prototype    : nsfw_timer_exp
*   Description  : timer expire
*   Input        : u64 count
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
nsfw_timer_exp (u64 count)
{
  nsfw_timer_info *tm_info = NULL;
  struct list_head *tNode;
  struct list_head *tPretNode;

  LINT_LIST ()list_for_each_entry (tm_info, tNode, (&g_timer_cfg.timer_head),
                                   node)
  {
    tPretNode = tm_info->node.prev;
    if (tm_info->time_left.tv_sec > (long) count * NSFW_TIMER_CYCLE)
      {
        tm_info->time_left.tv_sec -= count * NSFW_TIMER_CYCLE;
        continue;
      }

    list_del (&tm_info->node);
    list_add_tail (&tm_info->node, &g_timer_cfg.exp_timer_head);
    tNode = tPretNode;
  }

  u32 i = 0;
  while (!list_empty (&g_timer_cfg.exp_timer_head)
         && i++ < NSFW_TIMER_INFO_MAX_COUNT)
    {
      tm_info =
        (nsfw_timer_info *) list_get_first (&g_timer_cfg.exp_timer_head);
      if (NULL != tm_info->fun)
        {
          (void) tm_info->fun (tm_info->timer_type, tm_info->argv);
        }
      nsfw_timer_rmv_timer (tm_info);
    }

}

/*****************************************************************************
*   Prototype    : nsfw_get_timer_socket
*   Description  : get timer socket from kernel
*   Input        : None
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_get_timer_socket ()
{
  i32 tfd = timerfd_create (CLOCK_MONOTONIC, TFD_NONBLOCK);
  if (tfd == -1)
    {
      NSFW_LOGERR ("timerfd_create failed!]errno=%d\n", errno);
      return -1;
    }

  /* close on exe */
  if (-1 == nsfw_set_close_on_exec (tfd))
    {
      (void) nsfw_base_close (tfd);
      NSFW_LOGERR ("set exec err]fd=%d, errno=%d", tfd, errno);
      return -1;
    }

  struct itimerspec ts;
  ts.it_interval.tv_sec = NSFW_TIMER_CYCLE;
  ts.it_interval.tv_nsec = 0;
  ts.it_value.tv_sec = 0;
  ts.it_value.tv_nsec = NSFW_TIMER_CYCLE;

  if (timerfd_settime (tfd, 0, &ts, NULL) < 0)
    {
      NSFW_LOGERR ("timerfd_settime failed] errno=%d", errno);
      close (tfd);
      return -1;
    }

  return tfd;
}

/*****************************************************************************
*   Prototype    : nsfw_timer_notify_fun
*   Description  : receive timer event from kernel
*   Input        : i32 epfd
*                  i32 fd
*                  u32 events
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_timer_notify_fun (i32 epfd, i32 fd, u32 events)
{
  i32 rc;

  if ((events & EPOLLERR) || (events & EPOLLHUP) || (!(events & EPOLLIN)))
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGWAR ("timer disconnect!]epfd=%d,timer=%d,event=0x%x", epfd,
                   fd, events);

      (void) nsfw_mgr_unreg_sock_fun (fd);
      i32 timer_fd = nsfw_get_timer_socket ();
      if (timer_fd < 0)
        {
          NSFW_LOGERR ("get timer_fd failed!]epfd=%d,timer_fd=%d,event=0x%x",
                       epfd, fd, events);
          return FALSE;
        }

      (void) nsfw_mgr_reg_sock_fun (timer_fd, nsfw_timer_notify_fun);
      return TRUE;
    }

  u64 data;
  while (1)
    {
      rc = nsfw_base_read (fd, &data, sizeof (data));
      if (rc == 0)
        {
          NSFW_LOGERR ("timer_fd recv 0]timer_fd=%d,errno=%d", fd, errno);
          break;
        }
      else if (rc == -1)
        {
          if (errno == EINTR || errno == EAGAIN)
            {
              break;
            }
          NSMON_LOGERR ("timer_fd recv]timer_fd=%d,errno=%d", fd, errno);
          break;
        }

      nsfw_timer_exp (data);
    }

  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_timer_start
*   Description  : reg the timer module to epoll thread
*   Input        : None
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*
*****************************************************************************/
u8
nsfw_timer_start ()
{
  i32 timer_fd = nsfw_get_timer_socket ();
  if (timer_fd < 0)
    {
      NSFW_LOGERR ("get timer_fd failed!");
      return FALSE;
    }

  NSFW_LOGINF ("start timer_fd module!]timer_fd=%d", timer_fd);
  (void) nsfw_mgr_reg_sock_fun (timer_fd, nsfw_timer_notify_fun);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_timer_module_init
*   Description  : module init
*   Input        : void* param
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
nsfw_timer_module_init (void *param)
{
  u32 proc_type = (u32) ((long long) param);
  nsfw_timer_init_cfg *timer_cfg = &g_timer_cfg;
  NSFW_LOGINF ("ps module init]type=%u", proc_type);
  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
      (void) NSFW_REG_SOFT_INT (NSFW_DBG_MODE_PARAM, g_hbt_switch, 0, 1);
      break;
    case NSFW_PROC_TOOLS:
    case NSFW_PROC_CTRL:
      break;
    default:
      return 0;
    }

  timer_cfg->timer_info_size = NSFW_TIMER_INFO_MAX_COUNT_DEF;

  nsfw_mem_sppool pmpinfo;
  pmpinfo.enmptype = NSFW_MRING_MPMC;
  pmpinfo.usnum = timer_cfg->timer_info_size;
  pmpinfo.useltsize = sizeof (nsfw_timer_info);
  pmpinfo.isocket_id = NSFW_SOCKET_ANY;
  pmpinfo.stname.entype = NSFW_NSHMEM;
  if (-1 ==
      SPRINTF_S (pmpinfo.stname.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 "MS_TM_INFOPOOL"))
    {
      NSFW_LOGERR ("SPRINTF_S failed");
      return -1;
    }
  timer_cfg->timer_info_pool = nsfw_mem_sp_create (&pmpinfo);

  if (!timer_cfg->timer_info_pool)
    {
      NSFW_LOGERR ("alloc timer info pool_err");
      return -1;
    }

  MEM_STAT (NSFW_TIMER_MODULE, pmpinfo.stname.aname, NSFW_NSHMEM,
            nsfw_mem_get_len (timer_cfg->timer_info_pool, NSFW_MEM_SPOOL));

  INIT_LIST_HEAD (&(timer_cfg->timer_head));
  INIT_LIST_HEAD (&(timer_cfg->exp_timer_head));

  (void) nsfw_timer_start ();
  return 0;
}

/* *INDENT-OFF* */
NSFW_MODULE_NAME (NSFW_TIMER_MODULE)
NSFW_MODULE_PRIORITY (10)
NSFW_MODULE_DEPENDS (NSFW_MGR_COM_MODULE)
NSFW_MODULE_INIT (nsfw_timer_module_init)
/* *INDENT-ON* */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
