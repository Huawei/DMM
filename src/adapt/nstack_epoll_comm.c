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

#include "nstack_eventpoll.h"
#include "nsfw_mem_api.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "nsfw_recycle_api.h"
#include "nsfw_maintain_api.h"
#include <stdio.h>
#include <stdlib.h>

nsep_epollManager_t g_epollMng = {
  .infoSockMap = NULL,
};

/*
 *    This function will find the epitem of fd in eventpool ep
 *    This is only used in epoll_ctl add
 */
struct epitem *
nsep_find_ep (struct eventpoll *ep, int fd)
{
  struct ep_rb_node *rbp;
  struct epitem *epi, *epir = NULL;
  u32_t loopCnt = 0;
  for (rbp = ADDR_SHTOL (ep->rbr.rb_node); rbp;)
    {
      ++loopCnt;
      if (loopCnt > NSTACK_MAX_EPITEM_NUM)
        break;

      epi = (struct epitem *) ep_rb_entry (rbp, struct epitem, rbn);
      if (fd > epi->fd)
        {
          rbp = (struct ep_rb_node *) ADDR_SHTOL (rbp->rb_right);
        }
      else if (fd < epi->fd)
        {
          rbp = (struct ep_rb_node *) ADDR_SHTOL (rbp->rb_left);
        }
      else
        {
          epir = epi;
          break;
        }
    }

  if (loopCnt > NSTACK_MAX_EPITEM_NUM)
    {
      NSSOC_LOGERR ("Loop out of range!!!!");
    }

  return epir;
}

int
nstack_ep_unlink (struct eventpoll *ep, struct epitem *epi)
{
  int error = ENOENT;

  if (ep_rb_parent (&epi->rbn) ==
      (struct ep_rb_node *) ADDR_LTOSH_EXT (&epi->rbn))
    {
      NSSOC_LOGWAR ("ep_rb_parent == epi->rbn");
      return error;
    }

  epi->event.events = 0;

  ep_rb_erase (&epi->rbn, &ep->rbr);
  ep_rb_set_parent (&epi->rbn, &epi->rbn);

  if (EP_HLIST_NODE_LINKED (&epi->rdllink))
    {
      ep_hlist_del (&ep->rdlist, &epi->rdllink);
    }

  return 0;
}

/**
 * @Function        nsep_free_epitem
 * @Description     free nstack epitem
 * @param in        data - the epitem to be free
 * @return          0 on success, -1 on error
 */
int
nsep_free_epitem (struct epitem *data)
{
  struct epitem *epiEntry = (struct epitem *) data;
  struct epitem_pool *pool = &nsep_getManager ()->epitemPool;
  epiEntry->pid = 0;
  NSSOC_LOGDBG ("nsep_free_epitem data:%p", data);
  if (res_free (&epiEntry->res_chk))
    {
      NSFW_LOGERR ("epitem refree!]epitem=%p", epiEntry);
      return 0;
    }

  if (nsfw_mem_ring_enqueue (pool->ring, (void *) epiEntry) != 1)
    {
      NSSOC_LOGERR ("Error to free epitem");
    }
  return 0;
}

NSTACK_STATIC void
nsep_initEpInfo (nsep_epollInfo_t * info)
{
  int iindex = 0;
  EP_LIST_INIT (&info->epiList);
  NSTACK_SEM_MALLOC (info->epiLock, 1);
  NSTACK_SEM_MALLOC (info->freeLock, 1);

  info->rlfd = -1;
  info->rmidx = -1;
  info->fd = -1;
  info->ep = NULL;
  info->fdtype = 0;
  info->private_data = NULL;
  info->epaddflag = 0;
  for (iindex = 0; iindex < NSEP_SMOD_MAX; iindex++)
    {
      info->protoFD[iindex] = -1;
    }
  (void) nsep_for_pidinfo_init (&(info->pidinfo));
}

NSTACK_STATIC void
nsep_destroy_epinfo (nsep_epollInfo_t * info)
{
  return;
}

/**
 * @Function        nstack_eventpoll_allocShareInfo
 * @Description     alloc nstack share info
 * @param out       data - the return value alloced
 * @return          0 on success, -1 on error
 */
int
nsep_alloc_epinfo (nsep_epollInfo_t ** data)
{
  nsep_epollInfo_t *head_info = NULL;

  if (NULL == data)
    return -1;
  NSSOC_LOGDBG ("epinfo alloc begin");

  nsep_infoPool_t *pool = &nsep_getManager ()->infoPool;
  if (0 == nsfw_mem_ring_dequeue (pool->ring, (void *) &head_info)
      || NULL == head_info)
    {
      NSSOC_LOGERR ("epinfo ring alloc failed]pool->ring=%p", pool->ring);
      return -1;
    }

  res_alloc (&head_info->res_chk);

  nsep_initEpInfo (head_info);
  if (0 != nsep_add_pid (&head_info->pidinfo, get_sys_pid ()))
    {
      NSSOC_LOGERR ("epinfo pid add to headinfo failed]pid=%d,headinfo=%p",
                    get_sys_pid (), head_info);
    }
  NSSOC_LOGDBG ("epinfo ring:%p alloc epinfo:%p end", pool->ring, head_info);
  *data = head_info;
  return 0;
}

/**
 * @Function        nstack_eventpoll_freeShareInfo
 * @Description     free nstack share info
 * @param in        info - the info to be free
 * @return          0 on success, -1 on error
 */
int
nsep_free_epinfo (nsep_epollInfo_t * info)
{

  if (NULL == info)
    return -1;

  nsep_infoPool_t *pool = &nsep_getManager ()->infoPool;
  NSSOC_LOGDBG ("nsep_free_epinfo info:%p, pool->ring:%p", info, pool->ring);
  nsep_destroy_epinfo (info);

  (void) nsep_for_pidinfo_init (&(info->pidinfo));
  if (res_free (&info->res_chk))
    {
      NSFW_LOGERR ("epinfo refree!]epitem=%p", info);
      return 0;
    }

  if (nsfw_mem_ring_enqueue (pool->ring, (void *) info) != 1)
    {
      NSSOC_LOGERR ("Error to free epinfo");
    }

  return 0;
}

int
nsep_force_epinfo_free (void *data)
{
  nsep_epollInfo_t *info = data;
  if (NULL == info)
    {
      return FALSE;
    }

  if (!nsep_is_pid_array_empty (&info->pidinfo))
    {
      return FALSE;
    }

  res_alloc (&info->res_chk);
  (void) nsep_free_epinfo (info);
  NSFW_LOGINF ("free epinfo]%p", data);
  return TRUE;
}

int
nsep_force_epitem_free (void *data)
{
  struct epitem *item = data;
  if (NULL == item)
    {
      return FALSE;
    }

  if (0 != item->pid)
    {
      return FALSE;
    }

  res_alloc (&item->res_chk);
  (void) nsep_free_epitem (item);
  NSFW_LOGINF ("free epitem]%p", data);
  return TRUE;
}

int
nsep_force_epevent_free (void *data)
{
  struct eventpoll *epevent = data;
  if (NULL == epevent)
    {
      return FALSE;
    }

  if (0 != epevent->pid)
    {
      return FALSE;
    }

  res_alloc (&epevent->res_chk);
  (void) nsep_free_eventpoll (epevent);
  NSFW_LOGINF ("free event pool]%p", data);
  return TRUE;
}

NSTACK_STATIC int
nsep_init_eventpoll (struct eventpoll *ep)
{
  if (0 != sem_init (&ep->waitSem, 1, 0))
    {
      return -1;
    }

  NSTACK_SEM_MALLOC (ep->lock, 1);
  NSTACK_SEM_MALLOC (ep->sem, 1);

  EP_HLIST_INIT (&ep->rdlist);
  ep->ovflist = NSEP_EP_UNACTIVE_PTR;
  ep->rbr.rb_node = NULL;
  ep->epfd = -1;
  return 0;
}

NSTACK_STATIC void
nsep_destroy_eventpoll (struct eventpoll *ep)
{
  (void) sem_destroy (&ep->waitSem);
}

/**
 * @Function        nsep_free_eventpoll
 * @Description     free nstack eventpoll
 * @param in        ep - the eventpoll to be free
 * @return          0 on success, -1 on error
 */
int
nsep_free_eventpoll (struct eventpoll *ep)
{
  if (!ep)
    return -1;
  struct eventpoll *epEntry = (struct eventpoll *) ep;
  struct eventpoll_pool *pool = &nsep_getManager ()->epollPool;
  NSSOC_LOGDBG ("nsep_free_eventpoll ep:%p, epollPool:%p", ep, pool);
  nsep_destroy_eventpoll (ep);
  ep->pid = 0;
  NSSOC_LOGDBG ("Free eventpool");
  if (res_free (&ep->res_chk))
    {
      NSFW_LOGERR ("ep refree!]epitem=%p", epEntry);
      return 0;
    }

  if (nsfw_mem_ring_enqueue (pool->ring, epEntry) != 1)
    {
      NSSOC_LOGERR ("Error to free eventpoll");
    }

  return 0;
}

/**
 * @Function        nsep_alloc_eventpoll
 * @Description     alloc nstack eventpoll
 * @param out       data - the eventpoll alloced
 * @return          0 on success, -1 on error
 */
int
nsep_alloc_eventpoll (struct eventpoll **data)
{
  struct eventpoll *p_head = NULL;
  struct eventpoll_pool *pool = &nsep_getManager ()->epollPool;

  NSSOC_LOGDBG ("ring:%p alloc eventpool begin", pool->ring);
  if (0 == nsfw_mem_ring_dequeue (pool->ring, (void *) &p_head)
      || NULL == p_head)
    {
      NSSOC_LOGERR ("ring alloc eventpool failed]ring=%p", pool->ring);
      return -1;
    }

  NSSOC_LOGDBG ("alloc eventpool, pid=%u", get_sys_pid ());
  res_alloc (&p_head->res_chk);
  p_head->pid = get_sys_pid ();

  if (0 != nsep_init_eventpoll ((struct eventpoll *) p_head))
    {
      NSSOC_LOGERR ("p_head init pid alloc failed]p_head=%p,pid=%d", p_head,
                    get_sys_pid ());
      (void) nsep_free_eventpoll ((struct eventpoll *) p_head);
      return -1;
    }

  NSSOC_LOGDBG ("ring:%p eventpoll:%p alloc eventpool end", pool->ring,
                p_head);
  *data = p_head;
  return 0;
}

NSTACK_STATIC int
nsep_init_epitem (struct epitem *epi)
{
  int retVal;
  epi->rbn.rb_parent = (struct ep_rb_node *) ADDR_LTOSH_EXT (&epi->rbn);
  EP_HLIST_INIT_NODE (&epi->rdllink);
  EP_HLIST_INIT_NODE (&epi->lkFDllink);
  epi->nwait = 0;
  epi->ep = NULL;
  epi->next = NSEP_EP_UNACTIVE_PTR;
  retVal =
    MEMSET_S (&epi->event, sizeof (epi->event), 0, sizeof (epi->event));
  if (EOK != retVal)
    {
      NSSOC_LOGERR ("MEMSET_S failed]ret=%d", retVal);
      return -1;
    }

  EP_LIST_INIT_NODE (&epi->fllink);
  epi->revents = 0;
  epi->ovf_revents = 0;
  epi->fd = -1;
  epi->private_data = NULL;

  return 0;
}

/**
 * @Function        nsep_alloc_epitem
 * @Description     alloc nstack epitem
 * @param out       data - the epitem alloced
 * @return          0 on success, -1 on error
 */
int
nsep_alloc_epitem (struct epitem **data)
{
  struct epitem *p_head_entry = NULL;
  struct epitem_pool *pool = &nsep_getManager ()->epitemPool;

  NSSOC_LOGDBG ("epitem alloc begin..");

  if (0 == nsfw_mem_ring_dequeue (pool->ring, (void *) &p_head_entry)
      || NULL == p_head_entry)
    {
      NSSOC_LOGERR ("epitem ring alloc failed]ring=%p", pool->ring);
      return -1;
    }

  res_alloc (&p_head_entry->res_chk);
  p_head_entry->pid = get_sys_pid ();

  if (nsep_init_epitem ((struct epitem *) p_head_entry))
    {
      (void) nsep_free_epitem ((struct epitem *) p_head_entry);
      p_head_entry = NULL;
      NSSOC_LOGERR ("ring epitem init failed]ring=%p,epitem=%p", pool->ring,
                    p_head_entry);
      return -1;
    }

  NSSOC_LOGDBG ("epitem alloc success..ring:%p head:%p", pool->ring,
                p_head_entry);
  *data = p_head_entry;
  return 0;
}

typedef int (*nsep_shem_initFn_t) (void *, size_t);

NSTACK_STATIC int
nsep_epPoolInit (void *addr, size_t length)
{
  u32_t pos;
  int ret;

  NSSOC_LOGDBG ("Start to init eventpoll pool");

  ret = MEMSET_S (addr, length, 0, length);
  if (EOK != ret)
    {
      NSSOC_LOGERR ("MEMSET_S failed]ret=%d", ret);
      return -1;
    }
  struct eventpoll *pool = (struct eventpoll *) addr;
  nsep_epollManager_t *manager = nsep_getManager ();
  manager->epollPool.pool = pool;

  /* init g_nStackInfo.sockPool->nstack_block_array */
  for (pos = 0; pos < NSTACK_MAX_EPOLL_INFO_NUM; pos++)
    {
      pool[pos].pid = 0;
      if (-1 == nsfw_mem_ring_enqueue (manager->epollPool.ring, &pool[pos]))
        {
          NSSOC_LOGERR ("init fail to enqueue epitem]pos=%u", pos);
          return -1;
        }
    }
  return 0;
}

NSTACK_STATIC int
nsep_epitemPoolInit (void *addr, size_t length)
{
  u32_t pos;
  int ret;

  NSSOC_LOGDBG ("Start to init epitem pool");

  ret = MEMSET_S (addr, length, 0, length);
  if (EOK != ret)
    {
      NSSOC_LOGERR ("MEMSET_S failed]ret=%d", ret);
      return -1;
    }
  struct epitem *pool = (struct epitem *) addr;
  nsep_epollManager_t *manager = nsep_getManager ();
  manager->epitemPool.pool = pool;

  /* init g_nStackInfo.sockPool->nstack_block_array */
  for (pos = 0; pos < NSTACK_MAX_EPITEM_NUM; pos++)
    {
      pool[pos].pid = 0;
      if (-1 == nsfw_mem_ring_enqueue (manager->epitemPool.ring, &pool[pos]))
        {
          NSSOC_LOGERR ("init fail to enqueue epitem]pos=%u", pos);
          return -1;
        }
    }
  return 0;
}

NSTACK_STATIC int
nsep_epInfoPoolInit (void *addr, size_t length)
{
  u32_t pos;
  int ret;

  NSSOC_LOGDBG ("shmem info init start");

  ret = MEMSET_S (addr, length, 0, length);
  if (EOK != ret)
    {
      NSSOC_LOGERR ("MEMSET_S failed]ret=%d", ret);
      return -1;
    }
  nsep_epollInfo_t *pool = (nsep_epollInfo_t *) addr;
  nsep_epollManager_t *manager = nsep_getManager ();
  manager->infoPool.pool = pool;

  /* init g_nStackInfo.sockPool->nstack_block_array */
  for (pos = 0; pos < NSTACK_MAX_EPOLL_INFO_NUM; pos++)
    {
      if (nsep_for_pidinfo_init (&(pool[pos].pidinfo)))
        {
          NSSOC_LOGERR ("pid info init failed]pos=%u", pos);
          return -1;
        }

      if (-1 == nsfw_mem_ring_enqueue (manager->infoPool.ring, &pool[pos]))
        {
          NSSOC_LOGERR ("init fail to enqueue epInfo]pos=%u", pos);
          return -1;
        }
    }

  NSSOC_LOGDBG ("nstack_shmen_info_init success");
  return 0;
}

NSTACK_STATIC int
nsep_create_shmem (size_t length, char *name, nsep_shem_initFn_t initFn)
{
  nsfw_mem_zone pmeminfo;
  mzone_handle phandle;
  int ret;

  pmeminfo.ireserv = 0;
  pmeminfo.isocket_id = NSFW_SOCKET_ANY;
  pmeminfo.length = length;
  ret =
    STRCPY_S (pmeminfo.stname.aname, sizeof (pmeminfo.stname.aname), name);
  if (EOK != ret)
    {
      NSSOC_LOGERR ("STRCPY_S failed]name=%s,ret=%d", name, ret);
      return -1;
    }
  pmeminfo.stname.entype = NSFW_SHMEM;

  phandle = nsfw_mem_zone_create (&pmeminfo);
  if (NULL == phandle)
    {
      NSSOC_LOGERR ("create nstack epoll memory failed]name=%s", name);
      return -1;
    }

  if (0 != initFn ((void *) phandle, length))
    {
      NSSOC_LOGERR ("Fail to init memory]name=%s", name);
      return -1;
    }

  return 0;
}

NSTACK_STATIC int
nsep_create_epInfoMem ()
{
  nsfw_mem_mring pringinfo;
  pringinfo.enmptype = NSFW_MRING_MPMC;
  pringinfo.isocket_id = NSFW_SOCKET_ANY;
  pringinfo.stname.entype = NSFW_SHMEM;
  pringinfo.usnum = NSTACK_MAX_EPOLL_INFO_NUM;

  if (-1 ==
      SPRINTF_S (pringinfo.stname.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 MP_NSTACK_EPINFO_RING_NAME))
    {
      NSSOC_LOGERR ("Error to create ring]name=%s", pringinfo.stname.aname);
      return -1;
    }

  mring_handle ring_handle = nsfw_mem_ring_create (&pringinfo);

  if (NULL == ring_handle)
    {
      NSSOC_LOGERR ("Error to create ring]name=%s", pringinfo.stname.aname);
      return -1;
    }

  nsep_epollManager_t *manager = nsep_getManager ();
  manager->infoPool.ring = ring_handle;

  return nsep_create_shmem (sizeof (nsep_epollInfo_t) *
                            NSTACK_MAX_EPOLL_INFO_NUM,
                            MP_NSTACK_EPOLL_INFO_NAME, nsep_epInfoPoolInit);
}

NSTACK_STATIC int
nsep_adpt_attach_epInfoMem ()
{
  nsfw_mem_name name;
  name.entype = NSFW_SHMEM;
  name.enowner = NSFW_PROC_MAIN;

  if (-1 ==
      SPRINTF_S (name.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 MP_NSTACK_EPINFO_RING_NAME))
    {
      NSSOC_LOGERR ("Error to attach ring]name=%s", name.aname);
      return -1;
    }
  mring_handle ring_handle = nsfw_mem_ring_lookup (&name);

  if (NULL == ring_handle)
    {
      NSSOC_LOGERR ("Error to attach ring]name=%s", name.aname);
      return -1;
    }

  nsep_epollManager_t *manager = nsep_getManager ();
  manager->infoPool.ring = ring_handle;

  if (-1 ==
      SPRINTF_S (name.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 MP_NSTACK_EPOLL_INFO_NAME))
    {
      NSSOC_LOGERR ("SPRINTF_S failed]");
      return -1;
    }
  manager->infoPool.pool = nsfw_mem_zone_lookup (&name);
  if (NULL == manager->infoPool.pool)
    {
      NSSOC_LOGERR ("Error to attach memzone]name=%s",
                    MP_NSTACK_EPOLL_INFO_NAME);
      return -1;
    }
  return 0;
}

NSTACK_STATIC int
nsep_create_epItemMem ()
{
  nsfw_mem_mring pringinfo;
  pringinfo.enmptype = NSFW_MRING_MPMC;
  pringinfo.isocket_id = NSFW_SOCKET_ANY;
  pringinfo.stname.entype = NSFW_SHMEM;
  pringinfo.usnum = NSTACK_MAX_EPITEM_NUM;

  if (-1 ==
      SPRINTF_S (pringinfo.stname.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 MP_NSTACK_EPITEM_RING_NAME))
    {
      NSSOC_LOGERR ("Error to create ring]name=%s", pringinfo.stname.aname);
      return -1;
    }

  mring_handle ring_handle = nsfw_mem_ring_create (&pringinfo);

  if (NULL == ring_handle)
    {
      NSSOC_LOGERR ("Error to create ring]name=%s", pringinfo.stname.aname);
      return -1;
    }

  nsep_epollManager_t *manager = nsep_getManager ();
  manager->epitemPool.ring = ring_handle;

  return nsep_create_shmem (sizeof (struct epitem) * NSTACK_MAX_EPITEM_NUM,
                            MP_NSTACK_EPITEM_POOL, nsep_epitemPoolInit);
}

NSTACK_STATIC int
nsep_adpt_attach_epItemMem ()
{
  nsfw_mem_name name;
  name.entype = NSFW_SHMEM;
  name.enowner = NSFW_PROC_MAIN;

  if (-1 ==
      SPRINTF_S (name.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 MP_NSTACK_EPITEM_RING_NAME))
    {
      NSSOC_LOGERR ("Error to attach epItemMem]name=%s", name.aname);
      return -1;
    }

  mring_handle ring_handle = nsfw_mem_ring_lookup (&name);

  if (NULL == ring_handle)
    {
      NSSOC_LOGERR ("Error to attach ring]name=%s", name.aname);
      return -1;
    }

  nsep_epollManager_t *manager = nsep_getManager ();
  manager->epitemPool.ring = ring_handle;

  if (-1 ==
      SPRINTF_S (name.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 MP_NSTACK_EPITEM_POOL))
    {
      NSSOC_LOGERR ("SPRINTF_S failed]");
      return -1;
    }
  manager->epitemPool.pool = nsfw_mem_zone_lookup (&name);
  if (NULL == manager->epitemPool.pool)
    {
      NSSOC_LOGERR ("Error to attach memzone]name=%s", MP_NSTACK_EPITEM_POOL);
      return -1;
    }
  return 0;
}

NSTACK_STATIC int
nsep_create_eventpollMem ()
{
  nsfw_mem_mring pringinfo;
  pringinfo.enmptype = NSFW_MRING_MPMC;
  pringinfo.isocket_id = NSFW_SOCKET_ANY;
  pringinfo.stname.entype = NSFW_SHMEM;
  pringinfo.usnum = NSTACK_MAX_EPOLL_NUM;

  if (-1 ==
      SPRINTF_S (pringinfo.stname.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 MP_NSTACK_EVENTPOOL_RING_NAME))
    {
      NSSOC_LOGERR ("Error to create ring]name=%s", pringinfo.stname.aname);
      return -1;
    }

  mring_handle ring_handle = nsfw_mem_ring_create (&pringinfo);

  if (NULL == ring_handle)
    {
      NSSOC_LOGERR ("Error to create ring]name=%s", pringinfo.stname.aname);
      return -1;
    }

  nsep_epollManager_t *manager = nsep_getManager ();
  manager->epollPool.ring = ring_handle;

  return nsep_create_shmem (sizeof (struct eventpoll) * NSTACK_MAX_EPOLL_NUM,
                            MP_NSTACK_EVENTPOLL_POOL, nsep_epPoolInit);
}

NSTACK_STATIC int
nsep_adpt_attach_eventpollMem ()
{
  nsfw_mem_name name;
  name.entype = NSFW_SHMEM;
  name.enowner = NSFW_PROC_MAIN;

  if (-1 ==
      SPRINTF_S (name.aname, NSFW_MEM_NAME_LENGTH, "%s",
                 MP_NSTACK_EVENTPOOL_RING_NAME))
    {
      NSSOC_LOGERR ("Error to attach ring]name=%s", name.aname);
      return -1;
    }

  mring_handle ring_handle = nsfw_mem_ring_lookup (&name);

  if (NULL == ring_handle)
    {
      NSSOC_LOGERR ("Error to create ring]name=%s", name.aname);
      return -1;
    }

  nsep_epollManager_t *manager = nsep_getManager ();
  manager->epollPool.ring = ring_handle;

  int retVal = SPRINTF_S (name.aname, NSFW_MEM_NAME_LENGTH, "%s",
                          MP_NSTACK_EVENTPOLL_POOL);
  if (-1 == retVal)
    {
      NSSOC_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
      return -1;
    }
  manager->epollPool.pool = nsfw_mem_zone_lookup (&name);
  if (NULL == manager->epollPool.pool)
    {
      NSSOC_LOGERR ("Error to attach memzone]name=%s",
                    MP_NSTACK_EVENTPOLL_POOL);
      return -1;
    }

  return 0;
}

int
nsep_create_memory ()
{
  typedef int (*nsep_createMemFunc_t) (void);
  nsep_createMemFunc_t createFuncs[] = { nsep_create_epInfoMem,
    nsep_create_epItemMem,
    nsep_create_eventpollMem
  };

  int i = 0;
  for (i = 0;
       i < (int) (sizeof (createFuncs) / sizeof (nsep_createMemFunc_t)); i++)
    {
      if (-1 == createFuncs[i] ())
        return -1;
    }

  return 0;
}

int
nsep_adpt_attach_memory ()
{
  typedef int (*nsep_attachMemFunc_t) (void);
  nsep_attachMemFunc_t attachFuncs[] = { nsep_adpt_attach_epInfoMem,
    nsep_adpt_attach_epItemMem,
    nsep_adpt_attach_eventpollMem
  };

  int i = 0;
  for (i = 0;
       i < (int) (sizeof (attachFuncs) / sizeof (nsep_attachMemFunc_t)); i++)
    {
      if (-1 == attachFuncs[i] ())
        {
          NSSOC_LOGERR ("mem attach fail]idx=%d", i);
          return -1;
        }
    }

  return 0;
}

int
nsep_adpt_reg_res_mgr ()
{

  nsep_epollManager_t *manager = nsep_getManager ();

  nsfw_res_scn_cfg scn_cfg_info = { NSFW_RES_SCAN_ARRAY, 90, 3, 16,
    NSTACK_MAX_EPOLL_INFO_NUM / 128, NSTACK_MAX_EPOLL_INFO_NUM,
    sizeof (nsep_epollInfo_t),
    offsetof (nsep_epollInfo_t, res_chk),
    manager->infoPool.pool,
    manager->infoPool.ring,
    nsep_force_epinfo_free
  };

  nsfw_res_scn_cfg scn_cfg_item = { NSFW_RES_SCAN_ARRAY, 90, 3, 16,
    NSTACK_MAX_EPITEM_NUM / 128, NSTACK_MAX_EPITEM_NUM,
    sizeof (struct epitem),
    offsetof (struct epitem, res_chk),
    manager->epitemPool.pool,
    manager->epitemPool.ring,
    nsep_force_epitem_free
  };

  nsfw_res_scn_cfg scn_cfg_event = { NSFW_RES_SCAN_ARRAY, 90, 3, 16,
    NSTACK_MAX_EPOLL_NUM / 128, NSTACK_MAX_EPOLL_NUM,
    sizeof (struct eventpoll),
    offsetof (struct eventpoll, res_chk),
    manager->epollPool.pool,
    manager->epollPool.ring,
    nsep_force_epevent_free
  };

  (void) nsfw_res_mgr_reg (&scn_cfg_info);
  (void) nsfw_res_mgr_reg (&scn_cfg_item);
  (void) nsfw_res_mgr_reg (&scn_cfg_event);
  return 0;
}

int
nsep_epitem_remove (nsep_epollInfo_t * pinfo, u32 pid)
{
  struct list_node *prenode = NULL;
  struct list_node *nextnode = NULL;
  struct epitem *epi = NULL;
  u32_t i = 0;
  int icnt = 0;
  (void) sys_arch_lock_with_pid (&pinfo->epiLock);
  /*list head must be not null */
  prenode = (struct list_node *) ADDR_SHTOL (pinfo->epiList.head);
  nextnode = (struct list_node *) ADDR_SHTOL (prenode->next);
  while ((nextnode) && (i++ <= NSTACK_MAX_EPOLL_INFO_NUM))
    {
      epi = ep_list_entry (nextnode, struct epitem, fllink);
      if (pid == epi->pid)
        {
          /*shmem equal to shmem */
          prenode->next = nextnode->next;
          nextnode->next = NULL;
          (void) nsep_free_epitem (epi);
          nextnode = ADDR_SHTOL (prenode->next);
          icnt++;
          continue;
        }
      prenode = nextnode;
      nextnode = ADDR_SHTOL (nextnode->next);
    }
  sys_sem_s_signal (&pinfo->epiLock);
  if (i >= NSTACK_MAX_EPOLL_INFO_NUM)
    {
      NSSOC_LOGERR ("free pinfo:%p pid:%u, error maybe happen", pinfo, pid);
    }
  return icnt;
}

void
nsep_recycle_epfd (void *epinfo, u32 pid)
{
  struct eventpoll *ep = NULL;
  nsep_epollInfo_t *info = (nsep_epollInfo_t *) epinfo;
  int ret = 0;
  int ileftcnt = 0;
  if (!epinfo)
    {
      NSSOC_LOGDBG ("input null, pid:%u", pid);
      return;
    }
  (void) sys_arch_lock_with_pid (&info->freeLock);
  ileftcnt = nsep_del_last_pid (&info->pidinfo, pid);
  sys_sem_s_signal (&info->freeLock);
  /*no pid exist */
  if (-1 == ileftcnt)
    {
      return;
    }
  if (NSTACK_EPOL_FD == info->fdtype)
    {
      NSSOC_LOGDBG ("recycle epfd:%d epinfo pid:%u begin...", info->fd, pid);
      if (0 == ileftcnt)
        {
          ep = ADDR_SHTOL (info->ep);
          info->ep = NULL;
          (void) nsep_free_eventpoll (ep);
          (void) nsep_free_epinfo (info);
        }
      return;
    }

  NSSOC_LOGDBG ("recycle fd:%d epinfo pid:%u begin...", info->fd, pid);

  ret = nsep_epitem_remove (info, pid);
  if (0 != ret)
    {
      NSSOC_LOGINF ("info:%p, fd:%d pid:%u, %d items was left", info,
                    info->fd, pid, ret);
    }

  if (0 == ileftcnt)
    {
      NSSOC_LOGINF ("info:%p, fd:%d pid:%u was finally freed", info, info->fd,
                    pid);
      (void) nsep_free_epinfo (info);
    }
  return;
}

int
nsep_recycle_ep (u32 pid)
{
  nsep_epollManager_t *manager = nsep_getManager ();
  nsep_epollInfo_t *pool = manager->infoPool.pool;

  u32_t pos;
  for (pos = 0; pos < NSTACK_MAX_EPOLL_INFO_NUM; pos++)
    {
      (void) nsep_recycle_epfd (&pool[pos], pid);
    }
  return 0;
}

NSTACK_STATIC int
nsep_recycle_epItem (u32 pid)
{
  nsep_epollManager_t *manager = nsep_getManager ();
  struct epitem *pool = manager->epitemPool.pool;

  u32_t pos;
  for (pos = 0; pos < NSTACK_MAX_EPITEM_NUM; pos++)
    {
      if (pool[pos].pid != pid)
        continue;

      if (-1 == nsep_free_epitem (&pool[pos]))
        return -1;
    }

  return 0;
}

NSTACK_STATIC int
nsep_recycle_eventpoll (u32 pid)
{
  nsep_epollManager_t *manager = nsep_getManager ();
  struct eventpoll *pool = manager->epollPool.pool;

  u32_t pos;
  for (pos = 0; pos < NSTACK_MAX_EPOLL_INFO_NUM; pos++)
    {
      if (pool[pos].pid != pid)
        continue;

      if (-1 == nsep_free_eventpoll (&pool[pos]))
        return -1;
    }

  return 0;
}

NSTACK_STATIC
  nsfw_rcc_stat nsep_recycle_resource (u32 exit_pid, void *pdata,
                                       u16 rec_type)
{
  NSSOC_LOGINF ("pid:%u recycle", exit_pid);
  (void) nsep_recycle_epItem (exit_pid);
  (void) nsep_recycle_eventpoll (exit_pid);
  return NSFW_RCC_CONTINUE;
}

NSTACK_STATIC
  nsfw_rcc_stat nsep_recycle_lock (u32 pid, void *pdata, u16 rec_type)
{
  nsep_epollManager_t *manager = nsep_getManager ();
  nsep_epollInfo_t *pool = manager->infoPool.pool;
  u32_t pos;
  if (NULL != pool)
    {
      for (pos = 0; pos < NSTACK_MAX_EPOLL_INFO_NUM; pos++)
        {
          if (pid == pool[pos].epiLock.locked)
            {
              pool[pos].epiLock.locked = 0;
              NSFW_LOGWAR ("epiLock locked]pos=%u,pid=%u", pos, pid);
            }
          if (pid == pool[pos].freeLock.locked)
            {
              pool[pos].freeLock.locked = 0;
              NSFW_LOGWAR ("freelock locked]pos=%u,pid=%u", pos, pid);
            }
        }
    }

  struct eventpoll *ev_pool = manager->epollPool.pool;
  if (NULL != ev_pool)
    {
      for (pos = 0; pos < NSTACK_MAX_EPOLL_NUM; pos++)
        {
          if (pid == ev_pool[pos].lock.locked)
            {
              ev_pool[pos].lock.locked = 0;
              NSFW_LOGWAR ("event_pollLock locked]pos=%u,pid=%u", pos, pid);
            }

          if (pid == ev_pool[pos].sem.locked)
            {
              ev_pool[pos].sem.locked = 0;
              NSFW_LOGWAR ("event_pollLock sem]pos=%u,pid=%u", pos, pid);
            }
        }
    }

  return NSFW_RCC_CONTINUE;
}

REGIST_RECYCLE_OBJ_FUN (NSFW_REC_NSOCKET_EPOLL,
                        nsep_recycle_resource)
REGIST_RECYCLE_LOCK_REL (nsep_recycle_lock, NULL, NSFW_PROC_NULL)
