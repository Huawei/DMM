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

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif /* __cplusplus */

#include "nsfw_init.h"
#include "nsfw_mem_api.h"
#include "nsfw_recycle_api.h"
#include "nsfw_mgr_com_api.h"
#include "nsfw_ps_mem_api.h"
#include "nsfw_ps_api.h"
#include "nsfw_recycle_api.h"
#include "nsfw_fd_timer_api.h"
#include "nsfw_maintain_api.h"
#include "nstack_eventpoll.h"
#include "nstack_dmm_api.h"
#include "nstack_dmm_adpt.h"
#include "mgr_com.h"

int g_same_process = 1;

extern int nsep_adpt_attach_memory ();
extern int nstack_init_share_res ();
extern int nstack_attach_share_res ();
extern int nsep_adpt_reg_res_mgr ();

/**
 *    This just for linux kernel epoll thread
 */
int
nstack_event_callback (void *pdata, int events)
{
  nsep_epollInfo_t *epInfo = (nsep_epollInfo_t *) pdata;

  if (!epInfo)
    {
      NSSOC_LOGWAR ("!!!!!!!err pdata=%p,get null epInfo", pdata);
      return -1;
    }

  NSSOC_LOGDBG ("Got one event]fd=%d,events=%u", epInfo->fd, events);

  sys_arch_lock_with_pid (&epInfo->epiLock);
  struct list_node *fdEpiHead =
    (struct list_node *) ADDR_SHTOL (epInfo->epiList.head);
  struct list_node *node = (struct list_node *) ADDR_SHTOL (fdEpiHead->next);
  struct epitem *epi = NULL;
  struct eventpoll *ep = NULL;
  while (node)
    {

      epi = (struct epitem *) ep_list_entry (node, struct epitem, fllink);

      node = (struct list_node *) ADDR_SHTOL (node->next);
      ep = (struct eventpoll *) ADDR_SHTOL (epi->ep);
      if (!(epi->event.events & events))
        continue;

      /*event should not notice other process */
      if ((ep->pid != get_sys_pid ()) && g_same_process)
        {
          continue;
        }

      sys_arch_lock_with_pid (&ep->lock);

      if (unlikely (ep->ovflist != NSEP_EP_UNACTIVE_PTR))
        {
          if (epi->next == NSEP_EP_UNACTIVE_PTR)
            {
              epi->next = ep->ovflist;
              ep->ovflist = (struct epitem *) ADDR_LTOSH (epi);
            }
          epi->ovf_revents |= events;
          NSSOC_LOGDBG ("Add to ovflist]protoFD=%d,event=%d", epInfo->fd,
                        events);
          goto out_unlock;
        }
      if (!EP_HLIST_NODE_LINKED (&epi->rdllink))
        {
          ep_hlist_add_tail (&ep->rdlist, &epi->rdllink);
          sem_post (&ep->waitSem);
        }
      epi->revents |= (epi->event.events & events);
    out_unlock:
      sys_sem_s_signal (&ep->lock);
    }
  sys_sem_s_signal (&epInfo->epiLock);
  /*  [Remove fdInf->event_sem post] */
  return 0;
}

int
nstack_adpt_init (nstack_dmm_para * para)
{
  nsfw_mem_para stinfo = { 0 };
  i32 init_ret = 0;

  if (!para)
    {
      return -1;
    }
  stinfo.iargsnum = para->argc;
  stinfo.pargs = para->argv;
  stinfo.enflag = para->proc_type;
  if (para->deploy_type != NSTACK_MODEL_TYPE1
      && para->deploy_type != NSTACK_MODEL_TYPE_SIMPLE_STACK)
    {
      g_same_process = 0;
    }

  nsfw_com_attr_set (para->attr.policy, para->attr.pri);

  (void) nstack_framework_setModuleParam (NSFW_MEM_MGR_MODULE, &stinfo);
  (void) nstack_framework_setModuleParam (NSFW_MGR_COM_MODULE,
                                          (void *) ((u64) para->proc_type));
  (void) nstack_framework_setModuleParam (NSFW_TIMER_MODULE,
                                          (void *) ((u64) para->proc_type));
  (void) nstack_framework_setModuleParam (NSFW_PS_MODULE,
                                          (void *) ((u64) para->proc_type));
  (void) nstack_framework_setModuleParam (NSFW_PS_MEM_MODULE,
                                          (void *) ((u64) para->proc_type));
  (void) nstack_framework_setModuleParam (NSFW_RECYCLE_MODULE,
                                          (void *) ((u64) para->proc_type));
  (void) nstack_framework_setModuleParam (NSFW_RES_MGR_MODULE,
                                          (void *) ((u64) para->proc_type));
  (void) nstack_framework_setModuleParam (NSFW_SOFT_PARAM_MODULE,
                                          (void *) ((u64) para->proc_type));
  (void) nstack_framework_setModuleParam (NSFW_LOG_CFG_MODULE,
                                          (void *) ((u64) para->proc_type));

  init_ret = nstack_framework_init ();
  if (init_ret < 0)
    {
      NSFW_LOGERR
        ("######################init failed!!!!######################");
      return -1;
    }

  if ((para->proc_type != NSFW_PROC_APP)
      && (para->proc_type != NSFW_PROC_MAIN))
    {
      return 0;
    }

  if (para->proc_type == NSFW_PROC_MAIN)
    {
      if (nstack_init_share_res () != 0)
        {
          NSFW_LOGERR ("nstack_init_share_res failed");
          return -1;
        }
      if (nsep_create_memory () != 0)
        {
          NSFW_LOGERR ("nsep_create_memory failed");
          return -1;
        }
    }
  else
    {
      if (nstack_attach_share_res () != 0)
        {
          NSFW_LOGERR ("nstack_attach_share_res failed");
          return -1;
        }

        /**
        * the share memory for epoll is created and used by app, don't clear
        * it in fault case.
        */
      if (0 != nsep_adpt_attach_memory ())
        {
          NSFW_LOGERR ("nsep_adpt_attach_memory failed");
          return -1;
        }
    }

  void *pret =
    nsfw_recycle_reg_obj (NSFW_REC_PRO_LOWEST, NSFW_REC_NSOCKET_EPOLL,
                          NULL);
  if (!pret)
    {
      NSFW_LOGERR ("regist recycle failed");
      return -1;
    }
  (void) nsep_adpt_reg_res_mgr ();

  return 0;
}

/*just to used to dependence by other module*/
int
nstack_init_module (void *para)
{
  return 0;
}

NSFW_MODULE_NAME (NSTACK_DMM_MODULE)
NSFW_MODULE_PRIORITY (10)
NSFW_MODULE_DEPENDS (NSFW_MEM_MGR_MODULE)
NSFW_MODULE_DEPENDS (NSFW_MGR_COM_MODULE)
NSFW_MODULE_DEPENDS (NSFW_TIMER_MODULE)
NSFW_MODULE_DEPENDS (NSFW_PS_MODULE)
NSFW_MODULE_DEPENDS (NSFW_PS_MEM_MODULE)
NSFW_MODULE_DEPENDS (NSFW_RECYCLE_MODULE)
NSFW_MODULE_DEPENDS (NSFW_LOG_CFG_MODULE)
NSFW_MODULE_DEPENDS (NSFW_RES_MGR_MODULE)
NSFW_MODULE_DEPENDS (NSFW_SOFT_PARAM_MODULE)
NSFW_MODULE_INIT (nstack_init_module)
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
