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
#include "types.h"
#include "nstack_securec.h"
#include "nsfw_init.h"

#include "nsfw_mgr_com_api.h"
#include "nsfw_mem_api.h"
#include "nsfw_ps_api.h"
#include "nsfw_ps_mem_api.h"
#include "nsfw_fd_timer_api.h"
#include "nsfw_recycle_module.h"
#include "nsfw_maintain_api.h"
#include "nstack_log.h"
#include "common_mem_api.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

/* only work on nStackMain*/
nsfw_recycle_cfg g_rec_cfg;

nsfw_recycle_fun g_rec_fun[NSFW_REC_TYPE_MAX] = { 0 };

nsfw_rec_fun_info g_rec_lock_fun[NSFW_REC_LOCK_REL_MAX_FUN];

/*****************************************************************************
*   Prototype    : nsfw_recycle_reg_fun
*   Description  : reg one recycle type recycle function
*   Input        : u16 rec_type
*                  nsfw_recycle_fun fun
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_recycle_reg_fun (u16 rec_type, nsfw_recycle_fun fun)
{
  if (NULL == fun || rec_type >= NSFW_REC_TYPE_MAX)
    {
      NSFW_LOGERR ("argv err]fun=%p,type=%u", fun, rec_type);
      return FALSE;
    }

  g_rec_fun[rec_type] = fun;
  NSFW_LOGINF ("reg]fun=%d,type=%u", fun, rec_type);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_lock_rel_fun
*   Description  : reg lock release when app exit run
*   Input        : nsfw_recycle_fun fun
*                  void* data
*                  u8    proc_type: NULL as all
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_recycle_lock_rel_fun (nsfw_recycle_fun fun, void *data, u8 proc_type)
{
  if (NULL == fun)
    {
      NSFW_LOGERR ("argv err]fun=%p,data=%p", fun, data);
      return FALSE;
    }

  u32 i;

  for (i = 0; i < NSFW_REC_LOCK_REL_MAX_FUN; i++)
    {
      if (NULL == g_rec_lock_fun[i].rec_fun)
        {
          g_rec_lock_fun[i].rec_fun = fun;
          g_rec_lock_fun[i].data = data;
          g_rec_lock_fun[i].proc_type = proc_type;
          NSFW_LOGINF ("reg mgr_msg fun suc]fun=%p,data=%p", fun, data);
          return TRUE;
        }
    }

  NSFW_LOGINF ("reg]fun=%p,data=%p", fun, data);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_exit_pid_lock
*   Description  : release all lock fun
*   Input        : u32 pid
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_recycle_exit_pid_lock (u32 pid, u8 proc_type, void *argv)
{
  u32 i;
  NSFW_LOGINF ("release lock]pid=%d,type=%d", pid, proc_type);
  for (i = 0; i < NSFW_REC_LOCK_REL_MAX_FUN; i++)
    {
      if (NULL == g_rec_lock_fun[i].rec_fun)
        {
          break;
        }

      if ((NSFW_PROC_NULL == g_rec_lock_fun[i].proc_type)
          || (proc_type == g_rec_lock_fun[i].proc_type))
        {
          (void) g_rec_lock_fun[i].rec_fun (pid, g_rec_lock_fun[i].data, 0);
        }
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_obj_end
*   Description  : one recycle object process finished notify
*   Input        : u32 pid
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_recycle_obj_end (u32 pid)
{
  nsfw_mgr_msg *rsp_msg =
    nsfw_mgr_msg_alloc (MGR_MSG_RCC_END_REQ, NSFW_PROC_MAIN);
  if (NULL == rsp_msg)
    {
      NSFW_LOGERR ("alloc rsp msg failed]pid=%u", pid);
      return FALSE;
    }

  nsfw_ps_info_msg *ps_msg = GET_USER_MSG (nsfw_ps_info_msg, rsp_msg);
  ps_msg->host_pid = pid;
  (void) nsfw_mgr_send_msg (rsp_msg);
  nsfw_mgr_msg_free (rsp_msg);
  NSFW_LOGINF ("send obj end msg]pid=%d", pid);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_callback_all_obj
*   Description  : process all recycle object
*   Input        : u32 pid
*                  nsfw_recycle_pool *rec_pool
*   Output       : None
*   Return Value : nsfw_rcc_stat
*   Calls        :
*   Called By    :
*****************************************************************************/
nsfw_rcc_stat
nsfw_recycle_callback_all_obj (u32 pid, nsfw_recycle_pool * rec_pool)
{
  u32 match = 0;
  nsfw_recycle_obj *obj = NULL;
  if (NULL == rec_pool)
    {
      return NSFW_RCC_CONTINUE;
    }

  nsfw_recycle_obj *p_start = rec_pool->obj;
  u32 i;
  u32 size = rec_pool->pool_size;

  nsfw_ps_info *pps_info;
  pps_info = nsfw_ps_info_get (pid);
  if (NULL == pps_info)
    {
      NSFW_LOGERR ("get ps_info failed!]pid=%d", pid);
      return NSFW_RCC_CONTINUE;
    }

  i32 cur_idx = (i32) (u64) nsfw_ps_get_uv (pps_info, NSFW_REC_IDX);

  if (-1 == cur_idx)
    {
      cur_idx = 0;
      nsfw_ps_set_uv (pps_info, NSFW_REC_IDX, (void *) (u64) cur_idx);
      (void) nsfw_recycle_exit_pid_lock (pid, NSFW_PROC_APP, NULL);
    }
  else
    {
      cur_idx++;
      nsfw_ps_set_uv (pps_info, NSFW_REC_IDX, (void *) (u64) cur_idx);
    }

  for (i = cur_idx; i < size; i++)
    {
      obj = &p_start[i];
      cur_idx = i;
      nsfw_ps_set_uv (pps_info, NSFW_REC_IDX, (void *) (u64) cur_idx);
      if (FALSE == obj->alloc_flag)
        {
          continue;
        }

      if ((obj->rec_type < NSFW_REC_TYPE_MAX)
          && (NULL != g_rec_fun[obj->rec_type]))
        {
          match++;
          if (NSFW_RCC_SUSPEND ==
              g_rec_fun[obj->rec_type] (pid, obj->data, obj->rec_type))
            {
              NSFW_LOGINF
                ("call suspend]type=%d,obj_pid=%d,pid=%d,count=%d",
                 obj->rec_type, obj->host_pid, pid, match);
              return NSFW_RCC_SUSPEND;
            }
        }
      else
        {
          NSFW_LOGERR ("obj_error!drop]type=%d,obj_pid=%d,pid=%d",
                       obj->rec_type, obj->host_pid, pid);
        }
    }

  NSFW_LOGWAR ("rec obj]pid=%d,count=%d,cur_idx=%d", pid, match, cur_idx);
  return NSFW_RCC_CONTINUE;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_pid_obj
*   Description  : recycle object with pid
*   Input        : u32 pid
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_recycle_pid_obj (u32 pid)
{
  nsfw_ps_info *pps_info;
  pps_info = nsfw_ps_info_get (pid);
  if (NULL == pps_info)
    {
      NSFW_LOGERR ("get ps_info failed!]pid=%d", pid);
      return FALSE;
    }

  nsfw_recycle_pool *rec_pool = g_rec_cfg.mem_rec_obj_pool;
  void *timer_ptr = nsfw_ps_get_uv (pps_info, NSFW_REC_TIMER);
  if (NSFW_RCC_SUSPEND == nsfw_recycle_callback_all_obj (pid, rec_pool))
    {
      if (NULL != timer_ptr)
        {
          nsfw_timer_rmv_timer (timer_ptr);
          timer_ptr = NULL;
        }

      struct timespec time_left = { NSFW_REC_WEND_TVLAUE, 0 };
      timer_ptr =
        (void *) nsfw_timer_reg_timer (NSFW_REC_WEND_TIMER,
                                       (void *) (u64) pid,
                                       nsfw_recycle_obj_timeout, time_left);
      nsfw_ps_set_uv (pps_info, NSFW_REC_TIMER, timer_ptr);
      return TRUE;
    }

  if (NULL != timer_ptr)
    {
      nsfw_timer_rmv_timer (timer_ptr);
      nsfw_ps_set_uv (pps_info, NSFW_REC_TIMER, NULL);
    }

  (void) nsfw_ps_exit_end_notify (pid);
  nsfw_ps_info_free (pps_info);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_all_obj
*   Description  :
*   Input        : u32 pid
*                  nsfw_mem_addr_msg *addr_msg
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_recycle_all_obj (u32 pid)
{
  nsfw_ps_info *pps_info;
  pps_info = nsfw_ps_info_get (pid);
  if (NULL == pps_info)
    {
      pps_info = nsfw_ps_info_alloc (pid, NSFW_PROC_APP);
      if (NULL == pps_info)
        {
          NSFW_LOGERR ("alloc ps_info failed!]pid=%u", pid);
          return FALSE;
        }
    }

  nsfw_ps_set_uv (pps_info, NSFW_REC_IDX, (void *) (-1));
  nsfw_ps_set_uv (pps_info, NSFW_REC_TIMER, NULL);
  return nsfw_recycle_pid_obj (pid);
}

/*****************************************************************************
*   Prototype    : mem_app_exit_proc
*   Description  : exiting message process
*   Input        : nsfw_mgr_msg* msg
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
mem_app_exit_proc (nsfw_mgr_msg * msg)
{
  if (NULL == msg)
    {
      NSFW_LOGERR ("msg nul");
      return FALSE;
    }

  nsfw_ps_info_msg *ps_info_msg = GET_USER_MSG (nsfw_ps_info_msg, msg);

  /* look up the app rec memzone and release all resource */
  /* no need to send rsp for it will be send after stack process over */
  if (TRUE == nsfw_recycle_all_obj (ps_info_msg->host_pid))
    {
      NSFW_LOGINF ("obj found!]pid=%d", ps_info_msg->host_pid);
      return TRUE;
    }

  (void) nsfw_ps_exit_end_notify (ps_info_msg->host_pid);
  return FALSE;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_obj_end_proc
*   Description  : obj recycle finished notify process
*   Input        : nsfw_mgr_msg* msg
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_recycle_obj_end_proc (nsfw_mgr_msg * msg)
{
  if (NULL == msg)
    {
      NSFW_LOGERR ("msg nul");
      return FALSE;
    }

  nsfw_ps_info_msg *ps_info_msg = GET_USER_MSG (nsfw_ps_info_msg, msg);

  return nsfw_recycle_pid_obj (ps_info_msg->host_pid);
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_obj_timeout
*   Description  : recycle object timeout process
*   Input        : u32 timer_type
*                  void* data
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_recycle_obj_timeout (u32 timer_type, void *data)
{
  u32 pid = (u64) data;
  void *timer_ptr = NULL;
  NSFW_LOGINF ("ps_info timerout]pid=%u", pid);

  nsfw_ps_info *pps_info;
  pps_info = nsfw_ps_info_get (pid);
  if (NULL != pps_info)
    {
      nsfw_recycle_pool *rec_pool = g_rec_cfg.mem_rec_obj_pool;
      if (NULL != rec_pool)
        {
          if (TRUE == g_hbt_switch)
            {
              struct timespec time_left = { NSFW_REC_WEND_TVLAUE, 0 };
              timer_ptr =
                (void *) nsfw_timer_reg_timer (timer_type, data,
                                               nsfw_recycle_obj_timeout,
                                               time_left);
              nsfw_ps_set_uv (pps_info, NSFW_REC_TIMER, timer_ptr);
              return TRUE;
            }
        }
      nsfw_ps_set_uv (pps_info, NSFW_REC_TIMER, timer_ptr);
    }

  (void) nsfw_recycle_pid_obj (pid);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : mem_rec_zone_init
*   Description  : init recycle zone in app, only work on App
*   Input        : None
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
mem_rec_zone_init ()
{
  nsfw_mem_mring pringinfo;
  pringinfo.enmptype = NSFW_MRING_MPMC;
  pringinfo.isocket_id = NSFW_SOCKET_ANY;
  pringinfo.stname.entype = NSFW_NSHMEM;
  pringinfo.usnum = MEM_RECYCLE_PER_PRO_QUE;
  u32 i;
  for (i = 0; i < NSFW_REC_PRO_MAX; i++)
    {
      if (-1 ==
          SPRINTF_S (pringinfo.stname.aname, NSFW_MEM_NAME_LENGTH, "%s%d",
                     MEM_REC_QUEUE_NAME, i))
        {
          NSFW_LOGERR ("SPRINTF_S failed]");
          return FALSE;
        }
      g_rec_cfg.mem_rec_pro[i] = nsfw_mem_ring_create (&pringinfo);
      if (NULL == g_rec_cfg.mem_rec_pro[i])
        {
          NSFW_LOGERR ("alloc rec ring nul!");
          return FALSE;
        }
    }

  MEM_STAT (NSFW_RECYCLE_MODULE, MEM_REC_QUEUE_NAME, NSFW_NSHMEM,
            NSFW_REC_PRO_MAX * nsfw_mem_get_len (g_rec_cfg.mem_rec_pro[0],
                                                 NSFW_MEM_RING));

  nsfw_mem_zone pzoneinfo;
  pzoneinfo.isocket_id = NSFW_SOCKET_ANY;
  pzoneinfo.stname.entype = NSFW_NSHMEM;
  pzoneinfo.length =
    MEM_RECYCLE_OBJ_MAX_NUM * sizeof (nsfw_recycle_obj) +
    sizeof (nsfw_recycle_pool);
  if (-1 ==
      SPRINTF_S (pzoneinfo.stname.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 MEM_REC_POOL_NAME))
    {
      NSFW_LOGERR ("SPRINTF_S failed]");
      return FALSE;
    }

  g_rec_cfg.mem_rec_obj_pool = nsfw_mem_zone_create (&pzoneinfo);
  if (NULL == g_rec_cfg.mem_rec_obj_pool)
    {
      NSFW_LOGERR ("alloc rec pool nul!");
      return FALSE;
    }

  MEM_STAT (NSFW_RECYCLE_MODULE, MEM_REC_POOL_NAME, NSFW_NSHMEM,
            pzoneinfo.length);

  int retval;
  retval =
    MEMSET_S (g_rec_cfg.mem_rec_obj_pool, pzoneinfo.length, 0,
              pzoneinfo.length);
  if (EOK != retval)
    {
      NSFW_LOGERR ("mem set init failed!");
      return FALSE;
    }

  i32 j;
  nsfw_recycle_pool *rec_pool =
    (nsfw_recycle_pool *) g_rec_cfg.mem_rec_obj_pool;
  rec_pool->pool_size = MEM_RECYCLE_OBJ_MAX_NUM;

  nsfw_recycle_obj *p_start = rec_pool->obj;
  for (i = 0; i < NSFW_REC_PRO_MAX; i++)
    {
      for (j = 0; j < MEM_RECYCLE_PER_PRO_QUE; j++)
        {
          if (FALSE == p_start[j].alloc_flag)
            {
              if (0 ==
                  nsfw_mem_ring_enqueue (g_rec_cfg.mem_rec_pro[i],
                                         &p_start[j]))
                {
                  NSFW_LOGERR ("enqueue failed");
                  break;
                }
            }
        }
      p_start = p_start + MEM_RECYCLE_PER_PRO_QUE;
    }

  NSFW_LOGINF ("init rec pool and ring suc!");
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_reg_obj
*   Description  : reg one recycle object
*   Input        : u8 priority
*                  u16 rec_type
*                  void* data
*   Output       : None
*   Return Value : void *
*   Calls        :
*   Called By    :
*****************************************************************************/
void *
nsfw_recycle_reg_obj (u8 priority, u16 rec_type, void *data)
{
  if (NSFW_REC_PRO_MAX <= priority)
    {
      NSFW_LOGERR ("pro error]priority=%d,rec_type=%d,data=%p", priority,
                   rec_type, data);
      return NULL;
    }

  nsfw_recycle_obj *obj = NULL;
  if (0 ==
      nsfw_mem_ring_dequeue (g_rec_cfg.mem_rec_pro[priority], (void *) &obj))
    {
      NSFW_LOGERR ("dequeue error]priority=%d,rec_type=%d,data=%p",
                   priority, rec_type, data);
      return NULL;
    }

  if (EOK != MEMSET_S (obj, sizeof (*obj), 0, sizeof (*obj)))
    {
      if (0 == nsfw_mem_ring_enqueue (g_rec_cfg.mem_rec_pro[priority], obj))
        {
          NSFW_LOGERR ("enqueue error]priority=%d,rec_type=%d,data=%p",
                       priority, rec_type, data);
        }

      NSFW_LOGERR ("mem set error]priority=%d,rec_type=%d,data=%p",
                   priority, rec_type, data);
      return NULL;
    }

  obj->alloc_flag = TRUE;
  obj->rec_type = rec_type;
  obj->data = data;
  obj->host_pid = get_sys_pid ();
  obj->alloc_flag = TRUE;
  NSFW_LOGINF ("en queue obj]priority=%d,rec_type=%d,data=%p", priority,
               rec_type, data);
  return obj;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_rechk_lock
*   Description  : add for rechk lock
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_recycle_rechk_lock ()
{
  return nsfw_ps_rechk_pid_exit (nsfw_recycle_exit_pid_lock, NULL);
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_module_init
*   Description  : module init
*   Input        : void* param
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*****************************************************************************/
NSTACK_STATIC int nsfw_recycle_module_init (void *param);
NSTACK_STATIC int
nsfw_recycle_module_init (void *param)
{
  u32 proc_type = (u32) ((long long) param);
  NSFW_LOGINF ("recycle module init]type=%d", proc_type);
  g_rec_cfg.rec_waite_end_tvalue = NSFW_REC_WEND_TVLAUE_DEF;
  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
      (void) nsfw_mgr_reg_msg_fun (MGR_MSG_APP_EXIT_REQ, mem_app_exit_proc);
      (void) nsfw_mgr_reg_msg_fun (MGR_MSG_RCC_END_REQ,
                                   nsfw_recycle_obj_end_proc);
      if (TRUE == mem_rec_zone_init ())
        {
          return 0;
        }

      return 0;
    default:
      if (proc_type < NSFW_PROC_MAX)
        {
          break;
        }
      return -1;
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : nsfw_recycle_fork_init
*   Description  : fork init
*   Input        : None
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_recycle_fork_init ()
{
  /* reconnect to master after fork in child proc */
  nsfw_mgr_close_dst_proc (NSFW_PROC_MAIN, 0);
  if (0 == nsfw_recycle_module_init ((void *) ((long long) NSFW_PROC_APP)))
    {
      return TRUE;
    }
  return FALSE;
}

/* *INDENT-OFF* */
NSFW_MODULE_NAME (NSFW_RECYCLE_MODULE)
NSFW_MODULE_PRIORITY (10)
NSFW_MODULE_DEPENDS (NSFW_PS_MEM_MODULE)
NSFW_MODULE_INIT (nsfw_recycle_module_init)
/* *INDENT-ON* */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
