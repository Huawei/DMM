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
#include "nstack_log.h"
#include "nsfw_recycle_api.h"
#include "nstack_securec.h"
#include "nstack_module.h"
#include "nstack_sockops.h"
#include "nsfw_mem_api.h"
#include "nstack_fd_mng.h"
#include "nstack.h"
#include "nstack_dmm_adpt.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSTACK_EP_TRIGGLE(epFD, proFD, modInx, triggle_ops, event, pdata, ret) \
    if (nstack_extern_deal(modInx).ep_ctl != NULL) \
    { \
        ret = nstack_extern_deal(modInx).ep_ctl((epFD), (proFD), (triggle_ops), (event), pdata); \
    }

#define nstack_ep_getEvt(epFD, profd, modInx, events, ret) \
    if (nstack_extern_deal(modInx).ep_getevt != NULL)\
    {   \
        ret |= nstack_extern_deal(modInx).ep_getevt((epFD), (profd), (events)); \
    }

#define NSEP_IS_SOCK_VALID(_sock) ((_sock) >= 0 && (u32_t)(_sock) < NSTACK_KERNEL_FD_MAX)

/*add event to rdlist if event have come in*/
void
nsep_ctl_event_add (struct epitem *epi, struct eventpoll *ep,
                    int triggle_ops, unsigned int events)
{
  switch (triggle_ops)
    {
    case nstack_ep_triggle_add:
      if (events & epi->event.events)
        {
          sys_arch_lock_with_pid (&ep->lock);

          if (!EP_HLIST_NODE_LINKED (&epi->rdllink))
            {
              ep_hlist_add_tail (&ep->rdlist, &epi->rdllink);
            }

          sys_sem_s_signal (&ep->lock);
          sem_post (&ep->waitSem);
        }

      break;

    case nstack_ep_triggle_mod:
      sys_arch_lock_with_pid (&ep->lock);

      if (events & epi->event.events)
        {
          if (!EP_HLIST_NODE_LINKED (&epi->rdllink))
            {
              ep_hlist_add_tail (&ep->rdlist, &epi->rdllink);
              sem_post (&ep->waitSem);
            }
        }
      else
        {
          if (EP_HLIST_NODE_LINKED (&epi->rdllink)
              && (epi->event.events & EPOLLET))
            {
              ep_hlist_del (&ep->rdlist, &epi->rdllink);
            }
        }

      sys_sem_s_signal (&ep->lock);
      break;
    default:
      break;
    }

  return;
}

/*
 *    Triggle epoll events of stack
 *    ep - eventpoll instance
 *    fdInf - file descriptor of stack
 *    triggle_ops - why triggle
 */
NSTACK_STATIC void
nsep_epctl_triggle (struct epitem *epi, nsep_epollInfo_t * info,
                    int triggle_ops)
{
  int modInx;
  int protoFd = -1;
  int epfd = 0;
  int ret = 0;
  struct eventpoll *ep = NULL;
  nsep_epollInfo_t *epInfo = NULL;

  ep = ADDR_SHTOL (epi->ep);
  epfd = ep->epfd;
  epInfo = nsep_get_infoBySock (epfd);

  NSSOC_LOGINF ("info=%p,info->rmidx=%d,triggle_ops=%d", info, info->rmidx,
                triggle_ops);

  /* Now need to triggle userspace network stack events after add operation */
  if (info->rmidx >= 0)
    {
      /* fix overflow type codex issue */
      if ((info->rmidx >= NSEP_SMOD_MAX)
          || (info->rmidx >= NSTACK_MAX_MODULE_NUM))
        {
          return;
        }

      NSTACK_EP_TRIGGLE (epInfo->protoFD[info->rmidx], info->rlfd,
                         info->rmidx, triggle_ops, &(epi->event), info, ret);

      if ((ret >= 0)
          && ((nstack_ep_triggle_add == triggle_ops)
              || (nstack_ep_triggle_mod == triggle_ops)))
        {
          epi->revents = ret;
          nsep_ctl_event_add (epi, ep, triggle_ops, epi->revents);
          info->epaddflag |= (1 << info->rmidx);
        }
      NSSOC_LOGINF ("info=%p,module=%s,protoFd=%d,triggle_ops=%d, ret:%d",
                    info, nstack_get_module_name_by_idx (info->rmidx),
                    info->rlfd, triggle_ops, ret);
    }
  else
    {
      nstack_each_modInx (modInx)
      {
        protoFd = info->protoFD[modInx];

        if (protoFd < 0)
          {
            continue;
          }
        NSTACK_EP_TRIGGLE (epInfo->protoFD[modInx], protoFd, modInx,
                           triggle_ops, &(epi->event), info, ret);
        if ((ret >= 0)
            && ((nstack_ep_triggle_add == triggle_ops)
                || (nstack_ep_triggle_mod == triggle_ops)))
          {
            epi->revents |= ret;
            nsep_ctl_event_add (epi, ep, triggle_ops, epi->revents);
            info->epaddflag |= (1 << modInx);
          }
        NSSOC_LOGINF ("info=%p,module=%s,protoFd=%d,triggle_ops=%d, ret:%d",
                      info, nstack_get_module_name_by_idx (modInx), protoFd,
                      triggle_ops, ret);
      }
    }
  return;
}

NSTACK_STATIC void
nsep_rbtree_insert (struct eventpoll *ep, struct epitem *epi)
{
  struct ep_rb_node **p = &ep->rbr.rb_node, *parent = NULL;
  struct epitem *epic;
  u32_t loopCnt = 0;

  while (*p)
    {
      ++loopCnt;
      if (loopCnt > NSTACK_MAX_EPITEM_NUM)
        {
          NSSOC_LOGERR ("Loop out of range!!!!");
          break;
        }

      parent = (struct ep_rb_node *) ADDR_SHTOL (*p);
      epic = ep_rb_entry (parent, struct epitem, rbn);
      if (epi->fd > epic->fd)
        {
          p = &(parent->rb_right);
        }
      else
        {
          p = &(parent->rb_left);
        }
    }

  ep_rb_link_node (&epi->rbn, parent, p);
  ep_rb_insert_color (&epi->rbn, &ep->rbr);
}

/*
 *    This function is called by epctl_add , it will create one epitem of fd, and insert to eventpoll
 */
NSTACK_STATIC int
nsep_insert_node (struct eventpoll *ep, struct epoll_event *event,
                  nsep_epollInfo_t * fdInfo)
{
  struct epitem *epi;

  if (nsep_alloc_epitem (&epi))
    {
      NSSOC_LOGERR ("Can't alloc epitem");
      errno = ENOMEM;
      return -1;
    }

  EP_HLIST_INIT_NODE (&epi->rdllink);
  EP_HLIST_INIT_NODE (&epi->lkFDllink);

  epi->nwait = 0;
  epi->ep = (struct eventpoll *) ADDR_LTOSH_EXT (ep);
  epi->epInfo = (nsep_epollInfo_t *) ADDR_LTOSH_EXT (fdInfo);
  epi->revents = 0;
  epi->ovf_revents = 0;
  epi->event = *event;
  EP_LIST_INIT_NODE (&epi->fllink);
  epi->revents = 0;
  epi->fd = fdInfo->fd;

  /* Add the current item to the list of active epoll hook for this file
     This should lock because file descriptor may be called by other eventpoll */

  sys_arch_lock_with_pid (&fdInfo->epiLock);
  ep_list_add_tail (&fdInfo->epiList, &epi->fllink);
  epi->private_data = (void *) ADDR_LTOSH_EXT (fdInfo);
  sys_sem_s_signal (&fdInfo->epiLock);

  /* Add epitem to eventpoll fd list, don't need lock here because epoll_ctl will lock before calling this function */
  nsep_rbtree_insert (ep, epi);

  /* Need to poll the events already stored in stack */
  nsep_epctl_triggle (epi, fdInfo, nstack_ep_triggle_add);

  NSSOC_LOGINF ("epfd=%d,ep=%p,fd=%d,epi=%p", ep->epfd, ep, fdInfo->fd, epi);

  return 0;
}

NSTACK_STATIC int
nsep_isAddValid (int fd)
{

  if (nstack_fix_fd_check ())
    {
      return nstack_fix_fd_check ()(fd, STACK_FD_INVALID_CHECK);
    }
  return 0;
}

int
nsep_epctl_add (struct eventpoll *ep, int fd, struct epoll_event *events)
{

  int ret = 0;

  NSSOC_LOGINF ("epfd=%d,fd=%d,events=%u", ep->epfd, fd, events->events);

  nsep_epollInfo_t *fdInfo = nsep_get_infoBySock (fd);

  if (NULL == fdInfo)
    {
      if (0 == nsep_isAddValid (fd))
        {
          NSSOC_LOGERR ("Invalid add check nfd=%d]", fd);
          return -1;
        }
      if (-1 == nsep_alloc_infoWithSock (fd))
        {
          NSSOC_LOGERR ("Can't alloc epInfo for nfd]nfd=%d", fd);
          return -1;
        }
      nsep_set_infoProtoFD (fd, nstack_get_fix_mid (), fd);
      fdInfo = nsep_get_infoBySock (fd);
    }

  ret = nsep_insert_node (ep, events, fdInfo);
  if (0 != ret)
    {
      NSSOC_LOGWAR ("insert fail]epfd=%d,fd=%d ", ep->epfd, fdInfo->fd);
      return -1;
    }

  return 0;
}

int
nsep_epctl_del (struct eventpoll *ep, struct epitem *epi)
{
  int ret = 0;

  nsep_epollInfo_t *epInfo =
    (nsep_epollInfo_t *) ADDR_SHTOL (epi->private_data);
  NSSOC_LOGINF ("epfd=%d,fd=%d,epi=%p", ep->epfd, epi->fd, epi);        // modify log format error

  nsep_epctl_triggle (epi, epInfo, nstack_ep_triggle_del);

  sys_arch_lock_with_pid (&epInfo->epiLock);
  ep_list_del (&epInfo->epiList, &epi->fllink);

  sys_sem_s_signal (&epInfo->epiLock);

  sys_arch_lock_with_pid (&ep->lock);
  ret = nstack_ep_unlink (ep, epi);
  sys_sem_s_signal (&ep->lock);
  nsep_free_epitem (epi);

  return ret;
}

int
nsep_epctl_mod (struct eventpoll *ep,
                nsep_epollInfo_t * epInfo,
                struct epitem *epi, struct epoll_event *events)
{
  if (NULL == epInfo)
    {
      errno = EINVAL;
      NSSOC_LOGWAR ("epfd=%d, intput epInfo is null err", ep->epfd);
      return -1;
    }

  NSSOC_LOGINF ("epfd=%d,fd=%d,events=%u", ep->epfd, epInfo->fd,
                events->events);

  sys_arch_lock_with_pid (&ep->lock);
  epi->event = *events;         /* kernel tells me that I need to modify epi->event in lock context */
  sys_sem_s_signal (&ep->lock);

  nsep_epctl_triggle (epi, epInfo, nstack_ep_triggle_mod);
  return 0;
}

/*
 * Called by nsep_ep_poll
 * proccess the event in the list
 */
static int
nsep_events_proc (struct eventpoll *ep,
                  struct ep_node_list *nhead,
                  struct epoll_event *events, int maxevents, int *eventflag,
                  int num)
{
  struct ep_hlist_node *node = NULL;
  struct epitem *epi = NULL;
  struct ep_hlist_node *head = nhead->head;
  int evt = 0;

  while (head)
    {
      node = head;
      epi = ep_hlist_entry (node, struct epitem, rdllink);
      head = (struct ep_hlist_node *) ADDR_SHTOL (node->next);
      EP_HLIST_INIT_NODE (node);

      nsep_epollInfo_t *fdInfo =
        (nsep_epollInfo_t *) ADDR_SHTOL (epi->private_data);

      if (fdInfo->rmidx != -1)
        {
          nstack_ep_getEvt (ep->epfd, fdInfo->rlfd, fdInfo->rmidx,
                            epi->event.events, (epi->revents));
        }

      if (epi->revents)
        {
          /*set the flag that already have event int the rdlist */
          if (eventflag && (fdInfo->rmidx >= 0) && (fdInfo->rmidx < num))
            {
              eventflag[fdInfo->rmidx] = 1;
            }
          events[evt].events = epi->revents;
          events[evt].data = epi->event.data;
          NSSOC_LOGDBG ("Add event]epfd=%d,fd=%d,events=%u", ep->epfd,
                        epi->fd, events[evt].events);
          evt++;
        }

      if (0 != epi->revents && EPOLLET != (epi->event.events & EPOLLET))
        {
          NSSOC_LOGDBG ("Add epi->rdllink,epfd=%d,fd=%d", ep->epfd, epi->fd);
          ep_hlist_add_tail (&ep->rdlist, &epi->rdllink);
        }
      epi->revents = 0;

      if (evt >= maxevents)
        break;
    }
  nhead->head = head;
  if (!head)
    {
      nhead->tail = NULL;
    }
  return evt;
}

/*
 * Called by epoll_wait
 * Wait until events come or timeout
 */
int
nsep_ep_poll (struct eventpoll *ep, struct epoll_event *events, int maxevents,
              int *eventflag, int num)
{

  struct ep_hlist_node *tail = NULL;
  struct ep_hlist_node *head = NULL;
  struct epitem *epi = NULL;
  struct epitem *nepi = NULL;
  struct ep_node_list nhead = { 0 };
  int evt = 0;
  nsep_epollInfo_t *fdInfo = NULL;

  if (EP_HLIST_EMPTY (&ep->rdlist))
    {
      NSSOC_LOGDBG ("ep->rdlist is Empty, epfd=%d", ep->epfd);
      return 0;
    }

  sys_arch_lock_with_pid (&ep->sem);

  if (EP_HLIST_EMPTY (&ep->rdlist))
    {
      goto out;
    }

  sys_arch_lock_with_pid (&ep->lock);
  head = (struct ep_hlist_node *) ADDR_SHTOL (ep->rdlist.head);
  if (!head)
    {
      sys_sem_s_signal (&ep->lock);
      goto out;
    }
  nhead.head = (struct ep_hlist_node *) ADDR_SHTOL (head->next);
  nhead.tail = (struct ep_hlist_node *) ADDR_SHTOL (ep->rdlist.tail);
  /*unlink from ep->rdlist */
  EP_HLIST_INIT (&(ep->rdlist));
  ep->ovflist = NULL;
  sys_sem_s_signal (&ep->lock);

  /*get event list */
  evt = nsep_events_proc (ep, &nhead, events, maxevents, eventflag, num);

  sys_arch_lock_with_pid (&ep->lock);
  /*put rest epitem back to the rdlist */
  if (nhead.head)
    {
      tail = (struct ep_hlist_node *) ADDR_SHTOL (ep->rdlist.tail);
      tail->next = (struct ep_hlist_node *) ADDR_LTOSH (nhead.head);
      nhead.head->pprev = (struct ep_hlist_node **) ADDR_LTOSH (&tail->next);
      ep->rdlist.tail = (struct ep_hlist_node *) ADDR_LTOSH (nhead.tail);
    }
  /*put the epitem in ep->ovflist to rdlist */
  for (nepi = (struct epitem *) ADDR_SHTOL (ep->ovflist);
       (epi = nepi) != NULL;
       nepi = (struct epitem *) ADDR_SHTOL (epi->next), epi->next =
       NSEP_EP_UNACTIVE_PTR)
    {
      epi->revents |= epi->ovf_revents;
      /*set the flag that already have event int the rdlist */
      fdInfo = (nsep_epollInfo_t *) ADDR_SHTOL (epi->private_data);
      if (eventflag && fdInfo && (fdInfo->rmidx >= 0)
          && (fdInfo->rmidx < num))
        {
          eventflag[fdInfo->rmidx] = 1;
        }
      epi->ovf_revents = 0;
      if (!EP_HLIST_NODE_LINKED (&epi->rdllink))
        {
          ep_hlist_add_tail (&ep->rdlist, &epi->rdllink);
        }
    }
  ep->ovflist = NSEP_EP_UNACTIVE_PTR;
  sys_sem_s_signal (&ep->lock);
out:
  sys_sem_s_signal (&ep->sem);
  NSSOC_LOGDBG ("Return epfd=%d,evt=%d,EP_HLIST_EMPTY(&ep->rdlist)=%d",
                ep->epfd, evt, EP_HLIST_EMPTY (&ep->rdlist));
  return evt;
}

#define FREE_LIST_SIZE 128
#ifdef FREE_LIST_SIZE
struct free_list
{
  struct free_list *next;
  struct list_node *node[FREE_LIST_SIZE];
};
#endif

void
nsep_remove_epfd (nsep_epollInfo_t * pinfo)
{
  pid_t pid = get_sys_pid ();
  struct list_node *prenode = NULL;
  struct list_node *nextnode = NULL;
#ifdef FREE_LIST_SIZE
  struct free_list flist;
  struct free_list *fcurr = &flist;
#else
  struct list_node **node_arry = NULL;
  int length = NSTACK_MAX_EPOLL_INFO_NUM * sizeof (struct list_node *);
#endif
  struct epitem *epi = NULL;
  struct epitem *tepi = NULL;
  struct eventpoll *ep = NULL;
  u32_t i = 0;
  u32_t icnt = 0;

  if (!pinfo)
    {
      return;
    }
  /*malloc a block memory to store epitem node, do not use list for maybe free item */
#ifdef FREE_LIST_SIZE
  flist.next = 0;
#else
  node_arry = (struct list_node **) malloc (length);
  if (!node_arry)
    {
      NSSOC_LOGERR ("remove fd from ep malloc mem fail]fd=%d,ep=%p",
                    pinfo->fd, pinfo->ep);
      return;
    }

  int retVal = MEMSET_S (node_arry, length, 0, length);
  if (EOK != retVal)
    {
      NSSOC_LOGERR ("MEMSET_S failed]retVal=%d", retVal);
      free (node_arry);
      return;
    }
#endif
  sys_arch_lock_with_pid (&pinfo->epiLock);
  /*list head must be not null */
  prenode = (struct list_node *) ADDR_SHTOL (pinfo->epiList.head);
  nextnode = (struct list_node *) ADDR_SHTOL (prenode->next);
  icnt = 0;

  /*find all node that pid is belong to itself */
  while (nextnode)
    {
      if (++i > NSTACK_MAX_EPOLL_INFO_NUM)
        {
          /*record the exception log */
          NSSOC_LOGERR ("error maybe happen]free pinfo=%p", pinfo);
          break;
        }
      epi = ep_list_entry (nextnode, struct epitem, fllink);
      if (pid == epi->pid)
        {
          prenode->next = nextnode->next;
          nextnode->next = NULL;
          /*put into release list */
#ifdef FREE_LIST_SIZE
          if ((icnt % FREE_LIST_SIZE) == 0 && icnt)
            {
              struct free_list *fnext =
                (struct free_list *) malloc (sizeof (struct free_list));
              if (!fnext)
                {
                  NSSOC_LOGERR ("Out of memory for freelist]fd=%d icnt=%u",
                                pinfo->fd, icnt);
                  break;
                }
              fnext->next = NULL;
              fcurr->next = fnext;
              fcurr = fnext;
            }
          fcurr->node[icnt % FREE_LIST_SIZE] = nextnode;
#else
          node_arry[icnt] = nextnode;
#endif
          icnt++;
        }
      else
        {
          prenode = nextnode;
        }
      nextnode = (struct list_node *) ADDR_SHTOL (prenode->next);
    }

  sys_sem_s_signal (&pinfo->epiLock);

  /*free all epitem */
#ifdef FREE_LIST_SIZE
  fcurr = &flist;
#endif
  for (i = 0; i < icnt; i++)
    {
#ifdef FREE_LIST_SIZE
      if ((i % FREE_LIST_SIZE) == 0 && i)
        {
          fcurr = fcurr->next;
          if (fcurr == NULL)
            {
              NSSOC_LOGERR ("freelist invalid NULL, fd=%d icnt=%u i=%u",
                            pinfo->fd, icnt, i);
              break;
            }
        }
      epi =
        ep_list_entry (fcurr->node[i % FREE_LIST_SIZE], struct epitem,
                       fllink);
#else
      epi = ep_list_entry (node_arry[i], struct epitem, fllink);
#endif
      ep = (struct eventpoll *) ADDR_SHTOL (epi->ep);
      if (ep)
        {
          sys_arch_lock_with_pid (&ep->sem);
          /* Here don't use epi you find before, use fd and ep to find the epi again.that is multithread safe */
          tepi = nsep_find_ep (ep, pinfo->fd);
          /*record the exception log */
          if (epi != tepi)
            {
              NSSOC_LOGERR ("remove fd:%d epi:%p tepi:%p erro maybe happen",
                            pinfo->fd, epi, tepi);
            }
          /*if tepi is null, epi maybe free by nsep_close_epfd, so no need to free again */
          if (tepi)
            {
              nsep_epctl_triggle (tepi, pinfo, nstack_ep_triggle_del);
              sys_arch_lock_with_pid (&ep->lock);
              (void) nstack_ep_unlink (ep, tepi);
              sys_sem_s_signal (&ep->lock);

              nsep_free_epitem (epi);
            }
          sys_sem_s_signal (&ep->sem);
        }
    }
#ifdef FREE_LIST_SIZE
  for (fcurr = flist.next; fcurr; fcurr = flist.next)
    {
      flist.next = fcurr->next;
      free (fcurr);
    }
#else
  free (node_arry);
#endif
  return;
}

void
nsep_close_epfd (struct eventpoll *ep)
{

  if (!ep)
    return;

  struct epitem *epi = NULL;
  struct ep_rb_node *node = NULL;

  sys_arch_lock_with_pid (&ep->sem);
  while ((node = ep_rb_first (&ep->rbr)))
    {

      epi = ep_rb_entry (node, struct epitem, rbn);
      int ret = nsep_epctl_del (ep, epi);

      if (ret)
        {
          NSSOC_LOGERR
            ("nstack epctl del fail, will break to avoid dead loop]ep->fd=%d,epi->fd=%d",
             ep->epfd, epi->fd);
          break;
        }
    }
  sys_sem_s_signal (&ep->sem);
  nsep_free_eventpoll (ep);
}

static inline int
nsp_epoll_close_kernel_fd (int sock, nsep_epollInfo_t * epInfo)
{
  NSSOC_LOGINF ("fd=%d,type=%d", sock, epInfo->fdtype);
  int ret = 0;
  nsep_remove_epfd (epInfo);

  u32_t pid = get_sys_pid ();
  sys_arch_lock_with_pid (&epInfo->freeLock);
  int left_count = nsep_del_last_pid (&epInfo->pidinfo, pid);
  sys_sem_s_signal (&epInfo->freeLock);
  if (-1 == left_count)
    {
      NSSOC_LOGERR ("pid not exist]fd=%d,type=%d,pid=%d", sock,
                    epInfo->fdtype, pid);
    }

  if (0 == left_count)
    {
      ret = nsep_free_epinfo (epInfo);
      NSSOC_LOGINF ("epinfo removed]fd=%d,type=%d", sock, epInfo->fdtype);
    }

  return ret;
}

static inline int
nsp_epoll_close_spl_fd (int sock, nsep_epollInfo_t * epInfo)
{
  NSSOC_LOGINF ("fd=%d,type=%d", sock, epInfo->fdtype);
  nsep_remove_epfd (epInfo);
  return 0;
}

static inline int
nsp_epoll_close_ep_fd (int sock, nsep_epollInfo_t * epInfo)
{
  struct eventpoll *ep = ADDR_SHTOL (epInfo->ep);
  u32_t pid = get_sys_pid ();
  NSSOC_LOGINF ("fd:%d is epoll fd ep:%p]", sock, ep);
  sys_arch_lock_with_pid (&epInfo->freeLock);
  int left_count = nsep_del_last_pid (&epInfo->pidinfo, pid);
  sys_sem_s_signal (&epInfo->freeLock);
  if (0 == left_count)
    {
      epInfo->ep = NULL;
      nsep_close_epfd (ep);
      nsep_free_epinfo (epInfo);
    }
  return 0;
}

int
nsep_epoll_close (int sock)
{
  int ret = 0;
  int modInx = 0;
  int flag = 0;
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (sock);
  if (!epInfo)
    {
      NSSOC_LOGDBG ("epollsock close sock:%d is not exist", sock);
      return 0;
    }

  if (NSTACK_EPOL_FD == epInfo->fdtype)
    {
      ret = nsp_epoll_close_ep_fd (sock, epInfo);
      nsep_set_infoSockMap (sock, NULL);
      return ret;
    }

  nsep_set_infoSockMap (sock, NULL);

  nstack_each_modInx (modInx)
  {
    if (0 == (epInfo->epaddflag & (1 << modInx)))
      {
        flag = 1;
      }
  }
  /*if no stack reference the epinfo, just free epInfo,
   *else wait until the stack closed completely
   */
  if (0 == flag)
    {
      ret = nsp_epoll_close_kernel_fd (sock, epInfo);
    }
  else
    {
      ret = nsp_epoll_close_spl_fd (sock, epInfo);
    }
  return ret;
}

void
nsep_set_infoSockMap (int sock, nsep_epollInfo_t * info)
{
  nsep_epollManager_t *manager = nsep_getManager ();
  if (NULL == manager->infoSockMap)
    return;

  if (sock < 0 || (u32_t) sock >= NSTACK_KERNEL_FD_MAX)
    return;

  manager->infoSockMap[sock] = info;
}

nsep_epollInfo_t *
nsep_get_infoBySock (int sock)
{
  nsep_epollManager_t *manager = nsep_getManager ();
  if ((NULL == manager) || (NULL == manager->infoSockMap))
    return NULL;

  if (sock < 0 || (u32_t) sock >= NSTACK_KERNEL_FD_MAX)
    return NULL;

  return manager->infoSockMap[sock];
}

int
nsep_alloc_infoWithSock (int nfd)
{

  nsep_epollInfo_t *epInfo = NULL;

  if (!NSEP_IS_SOCK_VALID (nfd))
    {
      return -1;
    }

  if (-1 == nsep_alloc_epinfo (&epInfo))
    {
      NSSOC_LOGERR ("Alloc share info fail,[return]");
      return -1;
    }

  epInfo->fd = nfd;

  nsep_set_infoSockMap (nfd, epInfo);

  return 0;
}

void
nsep_set_infoProtoFD (int fd, int modInx, int protoFD)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return;

  if (modInx < 0 || modInx >= NSTACK_MAX_MODULE_NUM)
    return;

  epInfo->protoFD[modInx] = protoFD;
}

int
nsep_get_infoProtoFD (int fd, int modInx)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return -1;

  return epInfo->protoFD[modInx];
}

void
nsep_set_infomdix (int fd, int rmidx)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return;

  epInfo->rmidx = rmidx;
}

int
nsep_get_infoMidx (int fd)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return -1;

  return epInfo->rmidx;
}

void
nsep_set_infoRlfd (int fd, int rlfd)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return;

  epInfo->rlfd = rlfd;
}

int
nsep_get_infoRlfd (int fd)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return -1;

  return epInfo->rlfd;
}

void
nsep_set_infoSleepTime (int fd, u32 sleepTime)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return;

  epInfo->sleepTime = sleepTime;
}

int
nsep_get_infoSleepTime (int fd)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return -1;

  return epInfo->sleepTime;
}

void
nsep_set_infoEp (int fd, struct eventpoll *ep)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return;

  epInfo->ep = (struct eventpoll *) ADDR_LTOSH (ep);
  epInfo->fdtype = NSTACK_EPOL_FD;
}

struct eventpoll *
nsep_get_infoEp (int fd)
{
  nsep_epollInfo_t *epInfo = nsep_get_infoBySock (fd);

  if (NULL == epInfo)
    return NULL;

  return (struct eventpoll *) ADDR_SHTOL (epInfo->ep);
}

int
nsep_free_infoWithSock (int nfd)
{
  if ((u32_t) nfd >= NSTACK_KERNEL_FD_MAX || nfd < 0)
    return -1;

  nsep_epollInfo_t *info = nsep_get_infoBySock (nfd);

  if (NULL == info)
    return 0;

  nsep_set_infoSockMap (nfd, NULL);

  NSSOC_LOGDBG ("nsep_free_infoWithSock info:%p, nfd:%d", info, nfd);
  /* If this not just used by linux, it should be freed in stack-x */
  if (-1 == nsep_free_epinfo (info))
    {
      NSSOC_LOGERR ("Error to free ep info");
      return -1;
    }
  return 0;
}

/**
 * @Function        nsep_init_infoSockMap
 * @Description     initial map of epoll info and socket
 * @param           none
 * @return          0 on success, -1 on error
 */
int
nsep_init_infoSockMap ()
{
  nsep_epollManager_t *manager = nsep_getManager ();
  nsep_epollInfo_t **map =
    (nsep_epollInfo_t **) malloc (NSTACK_KERNEL_FD_MAX *
                                  sizeof (nsep_epollInfo_t *));

  if (!map)
    {
      NSSOC_LOGERR ("malloc epInfoPool fail");
      return -1;
    }

  u32_t iindex;
  for (iindex = 0; iindex < NSTACK_KERNEL_FD_MAX; iindex++)
    {
      map[iindex] = NULL;
    }

  manager->infoSockMap = map;

  return 0;
}

NSTACK_STATIC mzone_handle
nsep_ring_lookup (char *name)
{
  if (NULL == name)
    {
      NSSOC_LOGERR ("param error]name=%p", name);
      return NULL;
    }

  nsfw_mem_name mem_name;
  if (EOK != STRCPY_S (mem_name.aname, sizeof (mem_name.aname), name))
    {
      NSSOC_LOGERR ("Error to lookup ring by name, strcpy fail]name=%s",
                    name);
      return NULL;
    }
  mem_name.enowner = NSFW_PROC_MAIN;
  mem_name.entype = NSFW_SHMEM;

  return nsfw_mem_ring_lookup (&mem_name);
}

NSTACK_STATIC mzone_handle
nsep_attach_shmem (char *name)
{
  if (NULL == name)
    {
      NSSOC_LOGERR ("param error]name=%p", name);
      return NULL;
    }

  nsfw_mem_name mem_name;
  int retVal = STRCPY_S (mem_name.aname, sizeof (mem_name.aname), name);
  if (EOK != retVal)
    {
      NSSOC_LOGERR ("STRCPY_S failed]");
      return NULL;
    }
  mem_name.enowner = NSFW_PROC_MAIN;
  mem_name.entype = NSFW_SHMEM;

  return nsfw_mem_zone_lookup (&mem_name);
}

NSTACK_STATIC int
nsep_attach_infoMem ()
{
  mzone_handle hdl = nsep_attach_shmem (MP_NSTACK_EPOLL_INFO_NAME);
  if (NULL == hdl)
    return -1;

  nsep_epollManager_t *manager = nsep_getManager ();
  manager->infoPool.pool = (nsep_epollInfo_t *) hdl;

  hdl = nsep_ring_lookup (MP_NSTACK_EPINFO_RING_NAME);
  if (NULL == hdl)
    {
      NSSOC_LOGERR ("Fail to lock up epoll info ring]name=%s",
                    MP_NSTACK_EPINFO_RING_NAME);
      return -1;
    }

  manager->infoPool.ring = hdl;

  return 0;
}

NSTACK_STATIC int
nsep_attach_epItemMem ()
{
  mzone_handle hdl = nsep_attach_shmem (MP_NSTACK_EPITEM_POOL);
  if (NULL == hdl)
    return -1;

  nsep_epollManager_t *manager = nsep_getManager ();
  manager->epitemPool.pool = (struct epitem *) hdl;

  hdl = nsep_ring_lookup (MP_NSTACK_EPITEM_RING_NAME);
  if (NULL == hdl)
    {
      NSSOC_LOGERR ("Fail to lock up epoll info ring]name=%s",
                    MP_NSTACK_EPITEM_RING_NAME);
      return -1;
    }

  manager->epitemPool.ring = hdl;

  return 0;
}

NSTACK_STATIC int
nsep_attach_eventpollMem ()
{
  mzone_handle hdl = nsep_attach_shmem (MP_NSTACK_EVENTPOLL_POOL);
  if (NULL == hdl)
    return -1;

  nsep_epollManager_t *manager = nsep_getManager ();
  manager->epollPool.pool = (struct eventpoll *) hdl;

  hdl = nsep_ring_lookup (MP_NSTACK_EVENTPOOL_RING_NAME);
  if (NULL == hdl)
    {
      NSSOC_LOGERR ("Fail to lock up epoll info ring]name=%s",
                    MP_NSTACK_EVENTPOOL_RING_NAME);
      return -1;
    }

  manager->epollPool.ring = hdl;

  return 0;
}

/* epinfo add pid in parent */
void
nsep_fork_parent_proc (u32_t ppid, u32_t cpid)
{
  nsep_epollManager_t *manager = nsep_getManager ();
  if (NULL == manager->infoSockMap)
    {
      NSSOC_LOGERR ("infoSockMap is NULL]ppid=%d,cpid=%d", ppid, cpid);
      return;
    }

  nstack_fd_Inf *fdInf = NULL;
  nsep_epollInfo_t *epinfo = NULL;
  int pos;
  for (pos = 0; (u32_t) pos < NSTACK_KERNEL_FD_MAX; pos++)
    {
      epinfo = manager->infoSockMap[pos];
      if (epinfo)
        {
          fdInf = nstack_getValidInf (pos);
          if (fdInf)
            {
              if (((u32_t) (fdInf->type)) & SOCK_CLOEXEC)
                {
                  NSSOC_LOGINF ("fd is SOCK_CLOEXEC]fd=%d,ppid=%d.cpid=%d",
                                pos, ppid, cpid);
                  continue;
                }
            }

          if (nsep_add_pid (&epinfo->pidinfo, cpid) != 0)
            {
              NSSOC_LOGERR ("epinfo add pid failed]fd=%d,ppid=%d.cpid=%d",
                            pos, ppid, cpid);
            }
          else
            {
              NSSOC_LOGDBG ("epinfo add pid ok]fd=%d,ppid=%d.cpid=%d", pos,
                            ppid, cpid);
            }
        }
    }
}

/* check is pid exist in child,if no,detach epinfo*/
void
nsep_fork_child_proc (u32_t ppid)
{
  nsep_epollManager_t *manager = nsep_getManager ();
  if (NULL == manager->infoSockMap)
    {
      NSSOC_LOGERR ("epinfi sockmap not be create");
      return;
    }

  u32_t cpid = get_sys_pid ();
  nsep_epollInfo_t *epinfo = NULL;
  int pos;
  for (pos = 0; (u32_t) pos < NSTACK_KERNEL_FD_MAX; pos++)
    {
      epinfo = manager->infoSockMap[pos];
      if (epinfo)
        {
          if (!nsep_is_pid_exist (&epinfo->pidinfo, cpid))
            {
              NSSOC_LOGINF
                ("unuse epinfo,happen in SOCK_CLOEXEC,!FD_OPEN,parent coredump]fd=%d,epinfo=%p,ppid=%d,cpid=%d",
                 pos, epinfo, ppid, cpid);
              nsep_set_infoSockMap (pos, NULL);
            }
        }
    }
}

int
nsep_attach_memory ()
{
  typedef int (*nsep_attachMemFunc_t) (void);
  nsep_attachMemFunc_t attachFuncs[] = { nsep_attach_infoMem,
    nsep_attach_epItemMem,
    nsep_attach_eventpollMem
  };

  int i = 0;
  for (i = 0;
       i < (int) (sizeof (attachFuncs) / sizeof (nsep_attachMemFunc_t)); i++)
    {
      if (-1 == attachFuncs[i] ())
        return -1;
    }

  return 0;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
