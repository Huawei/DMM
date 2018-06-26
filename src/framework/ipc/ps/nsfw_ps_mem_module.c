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

#include "nsfw_ps_module.h"
#include "nsfw_mgr_com_api.h"
#include "nsfw_ps_mem_api.h"
#include "nsfw_ps_mem_module.h"
#include "nsfw_ps_api.h"
#include "nsfw_maintain_api.h"
#include "nsfw_fd_timer_api.h"

#include "nstack_log.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

ns_mem_mng_init_cfg g_mem_cfg;

int mem_ps_exiting (void *pps_info, void *argv);

int
nsfw_mem_ps_exit_resend_timeout (u32 timer_type, void *data)
{
  nsfw_ps_info *ps_info = data;
  if (NULL == ps_info)
    {
      NSFW_LOGERR ("ps_info nul!");
      return FALSE;
    }

  if (NSFW_PROC_APP != ps_info->proc_type)
    {
      return FALSE;
    }

  if (NSFW_PS_EXITING != ps_info->state)
    {
      return FALSE;
    }

  ps_info->resend_timer_ptr = NULL;
  (void) mem_ps_exiting (ps_info, NULL);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : mem_ps_exiting
*   Description  : send exiting message when ps_info exiting
*   Input        : void *pps_info
*                  void* argv
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
mem_ps_exiting (void *pps_info, void *argv)
{
  if (NULL == pps_info)
    {
      NSFW_LOGERR ("ps_info nul!");
      return FALSE;
    }

  if (TRUE == NSFW_SRV_STATE_SUSPEND)
    {
      NSFW_LOGERR ("main suspend]ps_info=%d,pid=%u", pps_info,
                   ((nsfw_ps_info *) pps_info)->host_pid);
      return FALSE;
    }

  nsfw_mgr_msg *msg =
    nsfw_mgr_msg_alloc (MGR_MSG_APP_EXIT_REQ, NSFW_PROC_MAIN);
  if (NULL == msg)
    {
      NSFW_LOGERR ("ps_exit alloc msg failed]ps_info=%p,pid=%u", pps_info,
                   ((nsfw_ps_info *) pps_info)->host_pid);
      return FALSE;
    }

  nsfw_ps_info_msg *ps_msg = GET_USER_MSG (nsfw_ps_info_msg, msg);
  ps_msg->host_pid = ((nsfw_ps_info *) pps_info)->host_pid;

  (void) nsfw_mgr_send_msg (msg);
  NSFW_LOGINF ("ps_exiting send msg]ps_info=%p,pid=%u", pps_info,
               ps_msg->host_pid);
  nsfw_mgr_msg_free (msg);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : mem_alloc_ps_info
*   Description  : alloc ps_info
*   Input        : u32 pid
*                  u8 proc_type
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
mem_alloc_ps_info (u32 pid, u8 proc_type)
{
  nsfw_ps_info *pps_info = NULL;
  pps_info = nsfw_ps_info_get (pid);
  if (NULL != pps_info)
    {
      return TRUE;
    }

  pps_info = nsfw_ps_info_alloc (pid, proc_type);
  if (NULL == pps_info)
    {
      NSFW_LOGERR ("alloc ps_info failed!]pid=%u,proc_type=%u", pid,
                   proc_type);
      return FALSE;
    }

  NSFW_LOGINF ("alloc new ps_info]pps_info=%p,pid=%u", pps_info, pid);
  return TRUE;
}

/*mem alloc by msg begin*/
void *
mem_item_zone_create (void *mem_info)
{
  return (void *) nsfw_mem_zone_create ((nsfw_mem_zone *) mem_info);
}

void *
mem_item_mbfmp_create (void *mem_info)
{
  return (void *) nsfw_mem_mbfmp_create ((nsfw_mem_mbfpool *) mem_info);
}

void *
mem_item_sp_create (void *mem_info)
{
  return (void *) nsfw_mem_sp_create ((nsfw_mem_sppool *) mem_info);
}

void *
mem_item_ring_create (void *mem_info)
{
  return (void *) nsfw_mem_ring_create ((nsfw_mem_mring *) mem_info);
}

nsfw_ps_mem_item_cfg g_ps_mem_map[] = {
  {
   NSFW_RESERV_REQ_MSG,
   sizeof (nsfw_shmem_reserv_req),
   NSFW_MEM_MZONE,
   mem_item_zone_create,
   mem_item_get_callargv}
  ,

  {
   NSFW_MBUF_REQ_MSG,
   sizeof (nsfw_shmem_mbuf_req),
   NSFW_MEM_MBUF,
   mem_item_mbfmp_create,
   mem_item_get_callargv}
  ,
  {
   NSFW_SPPOOL_REQ_MSG,
   sizeof (nsfw_shmem_sppool_req),
   NSFW_MEM_SPOOL,
   mem_item_sp_create,
   mem_item_get_callargv}
  ,
  {
   NSFW_RING_REQ_MSG,
   sizeof (nsfw_shmem_ring_req),
   NSFW_MEM_RING,
   mem_item_ring_create,
   mem_item_get_callargv}
  ,
  {
   NSFW_RELEASE_REQ_MSG,
   sizeof (nsfw_shmem_free_req),
   0xFFFF,
   mem_item_free,
   mem_item_get_callargv,
   }
  ,
  {
   NSFW_MEM_LOOKUP_REQ_MSG,
   sizeof (nsfw_shmem_lookup_req),
   0xFFFF,
   mem_item_lookup,
   mem_item_get_callargv,
   }
};

/*****************************************************************************
*   Prototype    : mem_item_get_cfg_from_msg
*   Description  : get memory config
*   Input        : u16 msg_type
*   Output       : None
*   Return Value : nsfw_ps_mem_item_cfg *
*   Calls        :
*   Called By    :
*****************************************************************************/
nsfw_ps_mem_item_cfg *
mem_item_get_cfg_from_msg (u16 msg_type)
{
  int idx;
  int map_count = sizeof (g_ps_mem_map) / sizeof (nsfw_ps_mem_item_cfg);
  for (idx = 0; idx < map_count; idx++)
    {
      if (g_ps_mem_map[idx].usmsg_type == msg_type)
        {
          return &g_ps_mem_map[idx];
        }
    }

  return NULL;
}

/*****************************************************************************
*   Prototype    : mem_item_get_callargv
*   Description  : change the message value to structure value
*   Input        : u16 msg_type
*                  char* msg_body
*                  char *memstr_buf
*                  i32 buf_len
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
mem_item_get_callargv (u16 msg_type, char *msg_body, char *memstr_buf,
                       i32 buf_len)
{
  switch (msg_type)
    {
    case NSFW_RESERV_REQ_MSG:
      MEM_GET_CALLARGV (length, length, nsfw_mem_zone, nsfw_shmem_reserv_req,
                        memstr_buf, msg_body);
      MEM_GET_CALLARGV (isocket_id, isocket_id, nsfw_mem_zone,
                        nsfw_shmem_reserv_req, memstr_buf, msg_body);
      break;
    case NSFW_MBUF_REQ_MSG:
      MEM_GET_CALLARGV (usnum, usnum, nsfw_mem_mbfpool, nsfw_shmem_mbuf_req,
                        memstr_buf, msg_body);
      MEM_GET_CALLARGV (uscash_size, uscash_size, nsfw_mem_mbfpool,
                        nsfw_shmem_mbuf_req, memstr_buf, msg_body);
      MEM_GET_CALLARGV (uspriv_size, uspriv_size, nsfw_mem_mbfpool,
                        nsfw_shmem_mbuf_req, memstr_buf, msg_body);
      MEM_GET_CALLARGV (usdata_room, usdata_room, nsfw_mem_mbfpool,
                        nsfw_shmem_mbuf_req, memstr_buf, msg_body);
      MEM_GET_CALLARGV (enmptype, enmptype, nsfw_mem_mbfpool,
                        nsfw_shmem_mbuf_req, memstr_buf, msg_body);
      MEM_GET_CALLARGV (isocket_id, isocket_id, nsfw_mem_mbfpool,
                        nsfw_shmem_mbuf_req, memstr_buf, msg_body);
      break;
    case NSFW_SPPOOL_REQ_MSG:
      MEM_GET_CALLARGV (usnum, usnum, nsfw_mem_sppool,
                        nsfw_shmem_sppool_req, memstr_buf, msg_body);
      MEM_GET_CALLARGV (useltsize, useltsize, nsfw_mem_sppool,
                        nsfw_shmem_sppool_req, memstr_buf, msg_body);
      MEM_GET_CALLARGV (enmptype, enmptype, nsfw_mem_sppool,
                        nsfw_shmem_sppool_req, memstr_buf, msg_body);
      MEM_GET_CALLARGV (isocket_id, isocket_id, nsfw_mem_sppool,
                        nsfw_shmem_sppool_req, memstr_buf, msg_body);
      break;
    case NSFW_RING_REQ_MSG:
      MEM_GET_CALLARGV (usnum, usnum, nsfw_mem_mring, nsfw_shmem_ring_req,
                        memstr_buf, msg_body);
      MEM_GET_CALLARGV (enmptype, enmptype, nsfw_mem_mring,
                        nsfw_shmem_ring_req, memstr_buf, msg_body);
      MEM_GET_CALLARGV (isocket_id, isocket_id, nsfw_mem_mring,
                        nsfw_shmem_ring_req, memstr_buf, msg_body);
      break;
    case NSFW_RELEASE_REQ_MSG:
      MEM_GET_CALLARGV (ustype, ustype, nsfw_mem_type_info,
                        nsfw_shmem_free_req, memstr_buf, msg_body);
      break;
    case NSFW_MEM_LOOKUP_REQ_MSG:
      MEM_GET_CALLARGV (ustype, ustype, nsfw_mem_type_info,
                        nsfw_shmem_lookup_req, memstr_buf, msg_body);
      break;
    default:
      NSFW_LOGERR ("error msg]type=%u", msg_type);
      return FALSE;
    }
  if (EOK !=
      STRCPY_S (((nsfw_mem_zone *) memstr_buf)->stname.aname,
                NSFW_MEM_NAME_LENGTH,
                ((nsfw_shmem_reserv_req *) msg_body)->aname))
    {
      NSFW_LOGERR ("STRCPY_S failed]msg_type=%u", msg_type);
      return FALSE;
    }

  ((nsfw_mem_zone *) memstr_buf)->stname.entype = NSFW_SHMEM;

  return TRUE;
}

/*****************************************************************************
*   Prototype    : mem_item_free
*   Description  : free memory item
*   Input        : void *pdata
*   Output       : None
*   Return Value : void*
*   Calls        :
*   Called By    :
*****************************************************************************/
void *
mem_item_free (void *pdata)
{
  nsfw_mem_type_info *mem_free = (nsfw_mem_type_info *) pdata;
  i32 ret;
  NSFW_LOGINF ("free mem]type=%u,name=%s", mem_free->ustype,
               mem_free->stname.aname);
  switch (mem_free->ustype)
    {
    case NSFW_MEM_MZONE:
      ret = nsfw_mem_zone_release (&mem_free->stname);
      break;
    case NSFW_MEM_MBUF:
      ret = nsfw_mem_mbfmp_release (&mem_free->stname);
      break;
    case NSFW_MEM_SPOOL:
      ret = nsfw_mem_sp_release (&mem_free->stname);
      break;
    case NSFW_MEM_RING:
      ret = nsfw_mem_ring_release (&mem_free->stname);
      break;
    default:
      NSFW_LOGERR ("free mem err type]type=%u", mem_free->ustype);
      return NULL;
    }

  if (NSFW_MEM_OK != ret)
    {
      NSFW_LOGERR ("mem free failed!]ret=%d", ret);
      return NULL;
    }

  return pdata;
}

/*****************************************************************************
*   Prototype    : mem_item_lookup
*   Description  : lookup memory item
*   Input        : void *pdata
*   Output       : None
*   Return Value : void*
*   Calls        :
*   Called By    :
*****************************************************************************/
void *
mem_item_lookup (void *pdata)
{
  nsfw_mem_type_info *mem_lookup = (nsfw_mem_type_info *) pdata;
  void *ret;
  NSFW_LOGDBG ("lookup mem]type=%u,name=%s", mem_lookup->ustype,
               mem_lookup->stname.aname);
  switch (mem_lookup->ustype)
    {
    case NSFW_MEM_MZONE:
      ret = nsfw_mem_zone_lookup (&mem_lookup->stname);
      break;
    case NSFW_MEM_MBUF:
      ret = nsfw_mem_mbfmp_lookup (&mem_lookup->stname);
      break;
    case NSFW_MEM_SPOOL:
      ret = nsfw_mem_sp_lookup (&mem_lookup->stname);
      break;
    case NSFW_MEM_RING:
      ret = nsfw_mem_ring_lookup (&mem_lookup->stname);
      break;
    default:
      NSFW_LOGERR ("lookup mem err type]type=%d", mem_lookup->ustype);
      return NULL;
    }

  if (NULL == ret)
    {
      NSFW_LOGERR ("mem lookup failed!]ret=%p,name=%s", ret,
                   mem_lookup->stname.aname);
      return NULL;
    }

  return ret;
}

/*****************************************************************************
*   Prototype    : mem_item_proc_by_msg
*   Description  : call memory item process function
*   Input        : void *pdata
*                  nsfw_ps_mem_item_cfg* item_cfg
*   Output       : None
*   Return Value : void*
*   Calls        :
*   Called By    :
*****************************************************************************/
void *
mem_item_proc_by_msg (void *pdata, nsfw_ps_mem_item_cfg * item_cfg)
{
  char argv_buf[NSFW_MEM_CALL_ARG_BUF] = { 0 };

  if ((NULL == item_cfg->change_fun) || (NULL == item_cfg->create_fun))
    {
      NSFW_LOGERR ("item error]change_fun=%p,create_fun=%p",
                   item_cfg->change_fun, item_cfg->create_fun);
      return NULL;
    }

  if (FALSE ==
      item_cfg->change_fun (item_cfg->usmsg_type, pdata, argv_buf,
                            NSFW_MEM_CALL_ARG_BUF))
    {
      NSFW_LOGERR ("call change_fun failed!]type=%u", item_cfg->usmsg_type);
      return NULL;
    }

  void *pdataret = NULL;
  pdataret = (item_cfg->create_fun) ((void *) argv_buf);
  return pdataret;
}

/*****************************************************************************
*   Prototype    : mem_init_rsp_msg
*   Description  : init the rsp message
*   Input        : nsfw_shmem_msg_head* msg
*                  nsfw_shmem_msg_head *rsp
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*
*****************************************************************************/
u8
mem_init_rsp_msg (nsfw_shmem_msg_head * msg, nsfw_shmem_msg_head * rsp)
{
  nsfw_ps_mem_item_cfg *item_cfg =
    mem_item_get_cfg_from_msg (msg->usmsg_type);
  if (NULL == item_cfg)
    {
      NSFW_LOGERR ("get item cfg failed!]msg_type=%u", msg->usmsg_type);
      return FALSE;
    }

  int idx;
  int mem_count = msg->uslength / item_cfg->item_size;

  rsp->usmsg_type = msg->usmsg_type + 1;
  rsp->uslength = mem_count * sizeof (nsfw_shmem_ack);
  nsfw_shmem_ack *pack = (nsfw_shmem_ack *) & (rsp->aidata[0]);
  char *pdata = NULL;
  for (idx = 0; idx < mem_count; idx++)
    {
      pdata = (char *) msg->aidata + idx * item_cfg->item_size;
      pack->pbase_addr = NULL;
      pack->usseq = ((nsfw_shmem_reserv_req *) pdata)->usseq;
      pack->cstate = NSFW_MEM_ALLOC_FAIL;
      pack++;
    }

  NSFW_LOGDBG ("init all rsp ack]mem_count=%d,msg_type=%u", mem_count,
               msg->usmsg_type);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : mem_rel_mem_by_msg
*   Description  : release memory by message
*   Input        : nsfw_shmem_msg_head* req_msg
*                  nsfw_shmem_msg_head *rsp
*                  u32 pid
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
mem_rel_mem_by_msg (nsfw_shmem_msg_head * req_msg,
                    nsfw_shmem_msg_head * rsp, u32 pid)
{
  u32 i;
  nsfw_ps_mem_item_cfg *item_cfg =
    mem_item_get_cfg_from_msg (req_msg->usmsg_type);
  if (NULL == item_cfg)
    {
      NSFW_LOGERR ("get item cfg failed!]msg_type=%u", req_msg->usmsg_type);
      return FALSE;
    }

  unsigned int mem_count = req_msg->uslength / item_cfg->item_size;
  char *pdata = NULL;
  nsfw_shmem_ack *pack = (nsfw_shmem_ack *) & (rsp->aidata[0]);
  for (i = 0; i < mem_count; i++)
    {
      pdata = (char *) req_msg->aidata + i * item_cfg->item_size;
      if (NULL != mem_item_proc_by_msg ((void *) pdata, item_cfg))
        {
          pack->cstate = NSFW_MEM_ALLOC_SUCC;
          pack->pbase_addr = NULL;
        }
      pack++;
    }

  return TRUE;
}

/*****************************************************************************
*   Prototype    : mem_lookup_mem_by_msg
*   Description  : lookup memory by message
*   Input        : nsfw_shmem_msg_head* mgr_msg
*                  nsfw_shmem_msg_head *rsp
*                  u32 pid
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
mem_lookup_mem_by_msg (nsfw_shmem_msg_head * mgr_msg,
                       nsfw_shmem_msg_head * rsp, u32 pid)
{
  i32 idx;
  nsfw_ps_mem_item_cfg *item_cfg =
    mem_item_get_cfg_from_msg (mgr_msg->usmsg_type);
  if (NULL == item_cfg)
    {
      NSFW_LOGERR ("get item cfg failed!]msg_type=%u", mgr_msg->usmsg_type);
      return FALSE;
    }

  int mem_count = mgr_msg->uslength / item_cfg->item_size;
  char *pdata = NULL;
  void *paddr = NULL;
  nsfw_shmem_ack *pack = (nsfw_shmem_ack *) & (rsp->aidata[0]);

  for (idx = 0; idx < mem_count; idx++)
    {
      pdata = (char *) mgr_msg->aidata + idx * item_cfg->item_size;
      paddr = mem_item_proc_by_msg ((void *) pdata, item_cfg);
      if (NULL != paddr)
        {
          pack->cstate = NSFW_MEM_ALLOC_SUCC;
          pack->pbase_addr = paddr;
        }
      pack++;
    }

  return TRUE;
}

/*****************************************************************************
*   Prototype    : mem_alloc_mem_by_msg
*   Description  : alloc memory by message
*   Input        : nsfw_shmem_msg_head* mem_msg
*                  nsfw_shmem_msg_head *rsp
*   Output       : None
*   Return Value : ns_mem_info*
*   Calls        :
*   Called By    :
*****************************************************************************/
void *
mem_alloc_mem_by_msg (nsfw_shmem_msg_head * mem_msg,
                      nsfw_shmem_msg_head * rsp)
{
  nsfw_ps_mem_item_cfg *item_cfg =
    mem_item_get_cfg_from_msg (mem_msg->usmsg_type);
  if (NULL == item_cfg)
    {
      NSFW_LOGERR ("get item cfg failed!]msg_type=%u", mem_msg->usmsg_type);
      return NULL;
    }

  int i;
  int j;
  nsfw_mem_type_info mem_free;
  char *pdata = NULL;
  void *p_addr = NULL;

  int mem_count = mem_msg->uslength / item_cfg->item_size;
  nsfw_shmem_ack *pack = (nsfw_shmem_ack *) & (rsp->aidata[0]);
  for (i = 0; i < mem_count; i++)
    {
      pdata = (char *) mem_msg->aidata + i * item_cfg->item_size;
      p_addr = mem_item_proc_by_msg ((void *) pdata, item_cfg);
      if (NULL == p_addr)
        {
          NSFW_LOGERR
            ("alloc mem failed!]type=%u,mem_count=%d,item=%d,name=%s",
             mem_msg->usmsg_type, mem_count, i,
             ((nsfw_shmem_reserv_req *) pdata)->aname);
          goto fail_free_mem;
        }

      pack->cstate = NSFW_MEM_ALLOC_SUCC;
      pack->pbase_addr = p_addr;
      NSFW_LOGINF
        ("alloc mem suc!]addr=%p,type=%u,mem_count=%d,item=%d,name=%s",
         p_addr, mem_msg->usmsg_type, mem_count, i,
         ((nsfw_shmem_reserv_req *) pdata)->aname);
      pack++;
    }
  return p_addr;

fail_free_mem:
  for (j = 0; j < i; j++)
    {
      pdata = (char *) mem_msg->aidata + j * item_cfg->item_size;
      if (EOK !=
          STRCPY_S (mem_free.stname.aname, NSFW_MEM_NAME_LENGTH,
                    ((nsfw_shmem_reserv_req *) pdata)->aname))
        {
          NSFW_LOGERR ("STRCPY_S failed]j=%d", j);
          continue;
        }

      mem_free.ustype = item_cfg->mem_type;
      mem_free.stname.entype = NSFW_SHMEM;
      (void) mem_item_free (&mem_free);
      NSFW_LOGINF ("free mem]addr=%p,type=%u,mem_count=%d,item=%d,name=%s",
                   p_addr, mem_msg->usmsg_type, mem_count, j,
                   ((nsfw_shmem_reserv_req *) pdata)->aname);
      pack++;
    }

  return NULL;

}

/*****************************************************************************
*   Prototype    : mem_alloc_req_proc
*   Description  : memory message
*   Input        : nsfw_mgr_msg* msg
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
mem_alloc_req_proc (nsfw_mgr_msg * msg)
{
  void *mem_addr = NULL;

  if (NULL == msg)
    {
      NSFW_LOGERR ("msg nul");
      return FALSE;
    }

  nsfw_shmem_msg_head *mem_msg = GET_USER_MSG (nsfw_shmem_msg_head, msg);
  nsfw_mgr_msg *rsp_msg = nsfw_mgr_rsp_msg_alloc (msg);
  if (NULL == rsp_msg)
    {
      NSFW_LOGERR ("alloc rsp msg failed]msg_type=%u", mem_msg->usmsg_type);
      return FALSE;
    }

  nsfw_shmem_msg_head *mem_rsp_msg =
    GET_USER_MSG (nsfw_shmem_msg_head, rsp_msg);

  if (!mem_init_rsp_msg (mem_msg, mem_rsp_msg))
    {
      NSFW_LOGERR ("init rsp msg failed]msg_type=%u", mem_msg->usmsg_type);
      nsfw_mgr_msg_free (rsp_msg);
      return FALSE;
    }

  switch (mem_msg->usmsg_type)
    {
    case NSFW_MEM_LOOKUP_REQ_MSG:
      if (!mem_lookup_mem_by_msg (mem_msg, mem_rsp_msg, msg->src_pid))
        {
          NSFW_LOGERR ("lookup mem msg failed]msg_type=%u",
                       mem_msg->usmsg_type);
          goto sendrspmsg;
        }
      (void) mem_alloc_ps_info (msg->src_pid, msg->src_proc_type);
      break;
    case NSFW_RESERV_REQ_MSG:
    case NSFW_MBUF_REQ_MSG:
    case NSFW_SPPOOL_REQ_MSG:
    case NSFW_RING_REQ_MSG:
      mem_addr = mem_alloc_mem_by_msg (mem_msg, mem_rsp_msg);
      if (NULL == mem_addr)
        {
          NSFW_LOGERR ("alloc mem msg failed]msg_type=%u",
                       mem_msg->usmsg_type);
          (void) mem_init_rsp_msg (mem_msg, mem_rsp_msg);
          goto sendrspmsg;
        }
      (void) mem_alloc_ps_info (msg->src_pid, msg->src_proc_type);
      break;
    case NSFW_RELEASE_REQ_MSG:
      if (!mem_rel_mem_by_msg (mem_msg, mem_rsp_msg, msg->src_pid))
        {
          NSFW_LOGERR ("rel mem msg failed]msg_type=%u", mem_msg->usmsg_type);
          goto sendrspmsg;
        }
      break;
    default:
      break;
    }

sendrspmsg:
  (void) nsfw_mgr_send_msg (rsp_msg);
  nsfw_mgr_msg_free (rsp_msg);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : mem_ps_exiting_resend
*   Description  : send the exiting message again
*   Input        : void *pps_info
*                  void* argv
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
mem_ps_exiting_resend (void *pps_info, void *argv)
{
  u32 *count = argv;
  nsfw_ps_info *ps_info = pps_info;
  if (NULL == ps_info)
    {
      NSFW_LOGERR ("ps_info nul!");
      return FALSE;
    }

  if (NSFW_PROC_APP != ps_info->proc_type)
    {
      return FALSE;
    }

  if (NSFW_PS_EXITING != ps_info->state)
    {
      return FALSE;
    }

  if (NULL != count)
    {
      NSFW_LOGINF ("send count]count=%u,pid=%u", *count, ps_info->host_pid);
      if (NSFW_PS_SEND_PER_TIME < (*count)++)
        {
          struct timespec time_left =
            { NSFW_PS_MEM_RESEND_TVLAUE +
((*count) / NSFW_PS_SEND_PER_TIME), 0
          };
          ps_info->resend_timer_ptr =
            (void *) nsfw_timer_reg_timer (NSFW_PS_MEM_RESEND_TIMER,
                                           pps_info,
                                           nsfw_mem_ps_exit_resend_timeout,
                                           time_left);
          return TRUE;
        }
    }

  (void) mem_ps_exiting (ps_info, NULL);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_srv_restore_timeout
*   Description  : service waiting resume timeout
*   Input        : u32 timer_type
*                  void* data
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_mem_srv_restore_timeout (u32 timer_type, void *data)
{
  u32 max_count = 0;

  g_mem_cfg.p_restore_timer = NULL;
  if (TRUE == NSFW_SRV_STATE_SUSPEND)
    {
      NSFW_SRV_STATE_SUSPEND = FALSE;
      (void) nsfw_ps_iterator (mem_ps_exiting_resend, &max_count);
    }
  return TRUE;
}

/*****************************************************************************
*   Prototype    : mem_srv_ctrl_proc
*   Description  : service control message process
*   Input        : nsfw_mgr_msg* msg
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
mem_srv_ctrl_proc (nsfw_mgr_msg * msg)
{
  if (NULL == msg)
    {
      NSFW_LOGERR ("msg nul");
      return FALSE;
    }

  nsfw_srv_ctrl_msg *ctrl_msg = GET_USER_MSG (nsfw_srv_ctrl_msg, msg);
  nsfw_mgr_msg *rsp_msg = nsfw_mgr_rsp_msg_alloc (msg);
  if (NULL == rsp_msg)
    {
      NSFW_LOGERR ("alloc rsp msg failed]msg=%p", msg);
      return FALSE;
    }
  nsfw_srv_ctrl_msg *ctrl_rsp_msg = GET_USER_MSG (nsfw_srv_ctrl_msg, rsp_msg);
  NSFW_LOGINF ("get srv ctrl state] state=%d", ctrl_msg->srv_state);

  ctrl_rsp_msg->rsp_code = NSFW_MGR_SUCCESS;

  (void) nsfw_mgr_send_msg (rsp_msg);
  nsfw_mgr_msg_free (rsp_msg);

  if (NSFW_SRV_CTRL_RESUME == ctrl_msg->srv_state)
    {
      if (TRUE == NSFW_SRV_STATE_SUSPEND)
        {
          NSFW_SRV_STATE_SUSPEND = FALSE;
        }
      u32 max_count = 0;
      (void) nsfw_ps_iterator (mem_ps_exiting_resend, &max_count);
      if (NULL != g_mem_cfg.p_restore_timer)
        {
          nsfw_timer_rmv_timer ((nsfw_timer_info *)
                                g_mem_cfg.p_restore_timer);
          g_mem_cfg.p_restore_timer = NULL;
        }
    }
  else
    {
      NSFW_SRV_STATE_SUSPEND = TRUE;
      struct timespec time_left = { NSFW_SRV_RESTORE_TVALUE, 0 };
      g_mem_cfg.p_restore_timer =
        (void *) nsfw_timer_reg_timer (0, NULL,
                                       nsfw_mem_srv_restore_timeout,
                                       time_left);
    }

  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_ps_mem_module_init
*   Description  : module init
*   Input        : void* param
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_ps_mem_module_init (void *param)
{
  u32 proc_type = (u32) ((long long) param);
  NSFW_LOGINF ("ps_mem module init]type=%u", proc_type);
  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
      (void) nsfw_ps_reg_global_fun (NSFW_PROC_APP, NSFW_PS_EXITING,
                                     mem_ps_exiting, NULL);
      (void) nsfw_mgr_reg_msg_fun (MGR_MSG_MEM_ALLOC_REQ, mem_alloc_req_proc);
      (void) NSFW_REG_SOFT_INT (NSFW_SRV_RESTORE_TIMER,
                                NSFW_SRV_RESTORE_TVALUE, 1, 0xFFFF);
      (void) NSFW_REG_SOFT_INT (NSFW_APP_RESEND_TIMER,
                                NSFW_PS_MEM_RESEND_TVLAUE, 1, 0xFFFF);
      (void) NSFW_REG_SOFT_INT (NSFW_APP_SEND_PER_TIME,
                                NSFW_PS_SEND_PER_TIME, 1, 0xFFFF);
      break;
    default:
      if (proc_type < NSFW_PROC_MAX)
        {
          return 0;
        }
      return -1;
    }

  g_mem_cfg.srv_restore_tvalue = NSFW_SRV_RESTORE_TVALUE_DEF;
  g_mem_cfg.ps_exit_resend_tvalue = NSFW_PS_MEM_RESEND_TVLAUE_DEF;
  g_mem_cfg.ps_send_per_time = NSFW_PS_SEND_PER_TIME_DEF;

  return 0;
}

/* *INDENT-OFF* */
NSFW_MODULE_NAME (NSFW_PS_MEM_MODULE)
NSFW_MODULE_PRIORITY (10)
NSFW_MODULE_DEPENDS (NSFW_PS_MODULE)
NSFW_MODULE_INIT (nsfw_ps_mem_module_init)
/* *INDENT-ON* */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
