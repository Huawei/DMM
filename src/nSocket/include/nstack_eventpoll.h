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

#ifndef _NSTACK_EVENTPOOL_H
#define _NSTACK_EVENTPOOL_H

#include "ephlist.h"
#include "eprb_tree.h"
#include "common_mem_api.h"
#include "types.h"
#include <semaphore.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include "nstack_securec.h"
#include "nstack_log.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSTACK_MAX_EPOLL_INFO_NUM 8192
#define NSTACK_MAX_EPOLL_NUM NSTACK_MAX_EPOLL_INFO_NUM
#define NSTACK_MAX_EPITEM_NUM (NSTACK_MAX_EPOLL_NUM*2)

#define MP_NSTACK_EPOLL_INFO_NAME "nsep_info"
#define MP_NSTACK_EVENTPOLL_POOL "nsep_eventpoll"       /* Pool of struct eventpoll */
#define MP_NSTACK_EPITEM_POOL "nsep_epitem"     /* Pool of struct epitem */

#define MP_NSTACK_EPINFO_RING_NAME "nsep_info_ring"
#define MP_NSTACK_EPITEM_RING_NAME "nsep_item_ring"
#define MP_NSTACK_EVENTPOOL_RING_NAME "nsep_event_ring"

#define NSTACK_FORK_NUM 32
#define NSTACK_EPOL_FD  1

#define NSEP_SMOD_MAX    8

#define NSEP_EP_UNACTIVE_PTR ((void *) -1L)

COMPAT_PROTECT (NSEP_SMOD_MAX, 8);

typedef struct
{
  u32 pid_used_size;
  u32 pid_array[NSTACK_FORK_NUM];
} nsep_pidinfo;

struct eventpoll
{

  /*
   *    Protect the this structure access
   *    This is for event add to ready list and poll out
   */
  sys_sem_st lock;

  /*
   * This semaphore is used to ensure that files are not removed
   * while epoll is using them. This is read-held during the event
   * processing loop and it is write-held during the file cleanup
   * path, the epoll file exit code and the ctl operations.
   * When we do epoll_ctl, we write lock
   * When we collecting data , we read lock
   */
  sys_sem_st sem;

  /*
   * This semaphore is used to block epoll_wait function
   */
  sem_t waitSem;

  /* List of ready file descriptors */
  struct ep_hlist rdlist;

  /* When poll data out, we need this list to store tmp epitems */
  struct epitem *ovflist;

  /* RB-Tree root used to store mastered fd structs */
  struct ep_rb_root rbr;

  /* This specifies the file descriptor value of epoll instance, currenlty it is just used for debugging */
  int epfd;
  u32 pid;
  nsfw_res res_chk;
};

struct eventpoll_pool
{
  void *ring;
  struct eventpoll *pool;
};

typedef struct
{
  int iindex;
  int iNext;
  int fd;
  i32 epaddflag;
  i32 fdtype;                   /*0: socket fd, 1: epoll fd */
  i32 rlfd;                     /* copy of fdInf->rlfd */
  i32 rmidx;                    /* copy of fdInf->rmidx */
  i32 protoFD[NSEP_SMOD_MAX];   /* copy of fdInf->protoFD */// Here we need to set router infomations dependency
  struct eventpoll *ep;
  sys_sem_st epiLock;
  sys_sem_st freeLock;
  struct ep_list epiList;       /* This restore the epitem of this file descriptor */
  u32 sleepTime;                //add for NSTACK_SEM_SLEEP
  nsep_pidinfo pidinfo;
  nsfw_res res_chk;
  void *private_data;           /*add for debug, just used to record extern infomation, for example sbr conn */
  i32 reserv[4];
} nsep_epollInfo_t;

typedef struct
{
  void *ring;
  nsep_epollInfo_t *pool;
  char last_reserve[8];         //reserve for update
} nsep_infoPool_t;

struct epitem
{
  struct ep_rb_node rbn;
  struct ep_hlist_node rdllink;
  struct ep_hlist_node lkFDllink;
  int nwait;
  struct eventpoll *ep;
  nsep_epollInfo_t *epInfo;
  struct epitem *next;
  struct epoll_event event;
  struct list_node fllink;
  unsigned int revents;
  unsigned int ovf_revents;
  int fd;
  u32 pid;
  void *private_data;
  nsfw_res res_chk;
};

struct epitem_pool
{
  void *ring;
  struct epitem *pool;
};

typedef struct
{
  struct eventpoll_pool epollPool;
  struct epitem_pool epitemPool;
  nsep_infoPool_t infoPool;
  nsep_epollInfo_t **infoSockMap;       // The map of epInfo and socket

} nsep_epollManager_t;
extern nsep_epollManager_t g_epollMng;
#define nsep_getManager() (&g_epollMng)

extern int nsep_alloc_eventpoll (struct eventpoll **data);
extern int nsep_free_eventpoll (struct eventpoll *ep);
extern int nsep_alloc_epitem (struct epitem **data);
extern int nsep_free_epitem (struct epitem *data);
extern int nsep_alloc_epinfo (nsep_epollInfo_t ** data);
extern int nsep_free_epinfo (nsep_epollInfo_t * info);

extern int nsep_epitem_remove (nsep_epollInfo_t * pinfo, u32 pid);

extern struct epitem *nsep_find_ep (struct eventpoll *ep, int fd);
extern int nsep_epctl_add (struct eventpoll *ep, int fd,
                           struct epoll_event *events);
extern int nsep_epctl_del (struct eventpoll *ep, struct epitem *epi);
extern int nsep_epctl_mod (struct eventpoll *ep,
                           nsep_epollInfo_t * epInfo,
                           struct epitem *epi, struct epoll_event *events);
extern int nsep_ep_poll (struct eventpoll *ep, struct epoll_event *events,
                         int maxevents, int *eventflag, int num);
extern void nsep_remove_epfd (nsep_epollInfo_t * info);
extern void nsep_close_epfd (struct eventpoll *ep);

/**
 * @Function        nsep_init_infoSockMap
 * @Description     initial map of epoll info and socket
 * @param           none
 * @return          0 on success, -1 on error
 */
extern int nsep_init_infoSockMap ();
extern void nsep_set_infoSockMap (int sock, nsep_epollInfo_t * info);
extern nsep_epollInfo_t *nsep_get_infoBySock (int sock);
extern int nsep_alloc_infoWithSock (int nfd);
extern void nsep_set_infoProtoFD (int fd, int modInx, int protoFD);
extern int nsep_get_infoProtoFD (int fd, int modInx);
extern void nsep_set_infomdix (int fd, int rmidx);
extern int nsep_get_infoMidx (int fd);
extern void nsep_set_infoRlfd (int fd, int rlfd);
extern int nsep_get_infoRlfd (int fd);
extern void nsep_set_infoSleepTime (int fd, u32 sleepTime);
extern int nsep_get_infoSleepTime (int fd);
extern void nsep_set_infoEp (int fd, struct eventpoll *ep);
extern struct eventpoll *nsep_get_infoEp (int fd);
extern int nsep_free_infoWithSock (int sock);
extern int nstack_ep_unlink (struct eventpoll *ep, struct epitem *epi);

/* Attach shared memory */
extern int nsep_create_memory ();
extern int nsep_attach_memory ();
extern int nsep_ep_fdinfo_release (int sock);
extern int nsep_epoll_close (int sock);
extern void nsep_fork_child_proc (u32_t ppid);
extern void nsep_fork_parent_proc (u32_t ppid, u32_t cpid);

static inline i32
nsep_for_pidinfo_init (nsep_pidinfo * pidinfo)
{
  int ret;
  ret = MEMSET_S (pidinfo, sizeof (*pidinfo), 0, sizeof (*pidinfo));
  if (EOK != ret)
    {
      NSSOC_LOGERR ("MEMSET_S failed]ret=%d", ret);
      return -1;
    }
  return 0;
}

static inline int
nsep_add_pid (nsep_pidinfo * pidinfo, pid_t pid)
{
  int i;

  for (i = 0; i < NSTACK_FORK_NUM; i++)
    {
      if ((0 == pidinfo->pid_array[i])
          && (__sync_bool_compare_and_swap (&pidinfo->pid_array[i], 0, pid)))
        {
          if (pidinfo->pid_used_size < i + 1)
            {
              pidinfo->pid_used_size = i + 1;
            }
          return 0;
        }
    }
  return -1;
}

static inline int
nsep_del_pid (nsep_pidinfo * pidinfo, pid_t pid)
{
  int i;

  for (i = 0; i < pidinfo->pid_used_size && i < NSTACK_FORK_NUM; i++)
    {
      if (pid == pidinfo->pid_array[i])
        {
          pidinfo->pid_array[i] = 0;
          return 0;
        }
    }
  return -1;
}

static inline int
nsep_del_last_pid (nsep_pidinfo * pidinfo, pid_t pid)
{
  int i;
  int count = 0;
  int deleted = 0;
  for (i = 0; i < pidinfo->pid_used_size && i < NSTACK_FORK_NUM; i++)
    {
      if (pid == pidinfo->pid_array[i])
        {
          pidinfo->pid_array[i] = 0;
          deleted = 1;
          continue;
        }

      if (pidinfo->pid_array[i] != 0)
        {
          ++count;
        }
    }

  if (!deleted)
    {
      return -1;
    }

  return count;
}

static inline int
nsep_is_pid_exist (nsep_pidinfo * epinf, pid_t pid)
{
  int i;

  for (i = 0; i < epinf->pid_used_size && i < NSTACK_FORK_NUM; i++)
    {
      if (pid == epinf->pid_array[i])
        {
          return 1;
        }
    }
  return 0;
}

static inline int
nsep_is_pid_array_empty (nsep_pidinfo * epinf)
{
  int i;
  for (i = 0; i < epinf->pid_used_size && i < NSTACK_FORK_NUM; i++)
    {
      if (epinf->pid_array[i] != 0)
        {
          return 0;
        }
    }
  return 1;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSTACK_EVENTPOLL_H */
