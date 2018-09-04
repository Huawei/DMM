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

#pragma GCC diagnostic ignored "-Wcpp"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <errno.h>
#include<sys/types.h>
#include<sys/time.h>
#include<unistd.h>
#include <string.h>
#include <pthread.h>
#include "nstack_types.h"
#include "nstack_sockops.h"
#include "nstack_log.h"
#include "linux_kernel_module.h"
#include "linux_kernel_socket.h"
#include "nsfw_base_linux_api.h"
#include "nstack_fd_mng.h"
#include "nstack_dmm_api.h"

#define SK_MAX_EP_EVENT 1024

kernel_stack_info_t g_ksInfo = {.thread_inited = ks_false,.epfd =
    -1,.checkEpollFD = -1
};

/*design ensures that g_ksInfo is not write accessed at the same time.
only read is done simultaneously with no chance of other thread writing it.
so no protection needed.*/
/* Custodial pointer not freed for events at end of this function */
/* This can be ignored as this is a thread and runs in infinite loop. Hence will never return */
void *
ks_ep_thread (void *arg)
{
  int eventNum = 0;
  int loop = 0;
  struct epoll_event *events =
    (struct epoll_event *) malloc (SK_MAX_EP_EVENT *
                                   sizeof (struct epoll_event));
  struct epoll_event *innerEvt =
    (struct epoll_event *) malloc (SK_MAX_EP_EVENT *
                                   sizeof (struct epoll_event));

  if (NULL == events || NULL == innerEvt)
    {
      NSSOC_LOGERR ("malloc events failed");

      if (events)
        {
          free (events);
          events = NULL;        /* Set NULL to pointer after free */
        }

      if (innerEvt)
        {
          free (innerEvt);
          innerEvt = NULL;      /* Set NULL to pointer after free */
        }
      /* When ks_ep_thread failed, it should set g_ksInfo.thread_inited ks_true, otherwise,it will result kernel_stack_register in dead loop */
      g_ksInfo.thread_inited = ks_true;
      return NULL;
    }

  if (-1 == g_ksInfo.epfd)
    {
      NSTACK_CAL_FUN (&g_ksInfo.libcOps, epoll_create, (1), g_ksInfo.epfd);
    }

  if (-1 == g_ksInfo.epfd)
    {
      g_ksInfo.thread_inited = ks_true;

      if (events)
        {
          free (events);
          events = NULL;        /* Set NULL to pointer after free */
        }

      if (innerEvt)
        {
          free (innerEvt);
          innerEvt = NULL;      /* Set NULL to pointer after free */
        }

      return NULL;
    }

  g_ksInfo.thread_inited = ks_true;

  do
    {

      NSTACK_CAL_FUN (&g_ksInfo.libcOps, epoll_wait,
                      (g_ksInfo.epfd, events, SK_MAX_EP_EVENT, -1), eventNum);

      if (0 == eventNum)
        {
          sys_sleep_ns (0, 100000);

        }

      for (loop = 0; loop < eventNum; loop++)
        {

          NSSOC_LOGDBG ("Epoll]events=%u,epfd=%d", events[loop].events,
                        events[loop].data.fd);

          if (events[loop].events & EPOLLIN)
            {
              int i = 0, num = 0, ret = 0, epfd = events[loop].data.fd;
              NSTACK_CAL_FUN (&g_ksInfo.libcOps, epoll_wait,
                              (epfd, innerEvt, SK_MAX_EP_EVENT, 0), num);

              if (0 == num)
                {
                  NSSOC_LOGWAR ("Num is zero");
                  continue;
                }

              NSTACK_CAL_FUN (&g_ksInfo.libcOps, epoll_ctl,
                              (g_ksInfo.epfd, EPOLL_CTL_DEL, epfd, NULL),
                              ret);

              ret = -1;
              for (i = 0; i < num; i++)
                {
                  ret &=
                    g_ksInfo.regVal.event_cb (innerEvt[i].data.ptr,
                                              innerEvt[i].events);
                  NSSOC_LOGDBG ("Kernel got one event]i=%d,ptr=%d,events=%u",
                                i, innerEvt[i].data.ptr, innerEvt[i].events);
                }

              if (ret)
                {
                  struct epoll_event ev;
                  ev.data.fd = epfd;
                  ev.events = EPOLLIN;
                  NSTACK_CAL_FUN (&g_ksInfo.libcOps, epoll_ctl,
                                  (g_ksInfo.epfd, EPOLL_CTL_ADD, epfd, &ev),
                                  ret);
                }
            }
        }
    }
  while (1);

}

int
kernel_fd_check (int fd, int flag)
{
  struct epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLERR;
  if (fd == g_ksInfo.checkEpollFD)
    {
      return 0;
    }

  /*in order to reduce the cost of epoll ctl */
  if (STACK_FD_FUNCALL_CHECK == flag)
    {
      return 1;
    }

  if (-1 ==
      nsfw_base_epoll_ctl (g_ksInfo.checkEpollFD, EPOLL_CTL_ADD, fd, &event))
    {
      return 0;
    }

  nsfw_base_epoll_ctl (g_ksInfo.checkEpollFD, EPOLL_CTL_DEL, fd, NULL);
  return 1;
}

int
kernel_prewait_proc (int epfd)
{
  int ret = 0;
  struct epoll_event ep_event;

  NSSOC_LOGINF ("epfd=%d was added", epfd);
  if (epfd < 0)
    {
      return -1;
    }
  ep_event.data.fd = epfd;
  ep_event.events = EPOLLIN | EPOLLET;
  ret = lk_epollctl (0, EPOLL_CTL_ADD, epfd, &ep_event);
  return ret;
}

unsigned int
kernel_ep_fd_add (int epFD, int proFD, int ctl_ops,
                  struct epoll_event *events, void *pdata)
{
  struct epoll_event tmpEvt;
  int ret = 0;
  tmpEvt.data.ptr = pdata;
  tmpEvt.events = events->events;
  NSSOC_LOGINF ("epfd=%d,fd=%d,ops=%d, events=%u", epFD, proFD, ctl_ops,
                events->events);
  switch (ctl_ops)
    {
    case nstack_ep_triggle_add:
      ret = nsfw_base_epoll_ctl (epFD, EPOLL_CTL_ADD, proFD, &tmpEvt);
      break;
    case nstack_ep_triggle_mod:
      ret = nsfw_base_epoll_ctl (epFD, EPOLL_CTL_MOD, proFD, &tmpEvt);
      break;
    case nstack_ep_triggle_del:
      ret = nsfw_base_epoll_ctl (epFD, EPOLL_CTL_DEL, proFD, &tmpEvt);
      break;
    default:
      ret = -1;
      break;
    }
  return ret;
}

int
kernel_socket (int a, int b, int c)
{

  return nsfw_base_socket (a, b, c);
}

int
kernel_module_init ()
{
  int retval = 0;

  g_ksInfo.thread_inited = ks_false;
  retval = pthread_create (&g_ksInfo.ep_thread, NULL, ks_ep_thread, NULL);
  if (retval != 0)
    {
      NSPOL_LOGERR ("kernel ep thread create fail, errno:%d!", errno);
      return ks_fail;
    }

  /* The earlier thread "ep_thread" created will exit automatically when
     return failure from below if any failure */
  retval = pthread_setname_np (g_ksInfo.ep_thread, K_EPOLL_THREAD_NAME);
  if (retval != 0)
    {
      NSMON_LOGERR
        ("pthread_setname_np failed for ep_thread]retval=%d, errno:%d",
         retval, errno);
      /*set thread name failed no need to return */
    }

  NSSOC_LOGDBG ("New thread started");

  do
    {
      sys_sleep_ns (0, 0);
    }
  while (!g_ksInfo.thread_inited);

  if (-1 == g_ksInfo.epfd)
    {
      NSPOL_LOGERR ("thread epoll create Err!");
      retval |= -1;
    }
  return retval;
}

int
kernel_fd_alloc ()
{
  return nsfw_base_socket (AF_UNIX, SOCK_DGRAM, 0);
}

int
kernel_stack_register (nstack_proc_cb * ops, nstack_event_cb * val)
{
  int retval = 0;
  /*  Input parameter validation */
  if ((NULL == ops) || (NULL == val))
    {
      NSPOL_LOGERR ("input param is NULL");
      return ks_fail;
    }

#undef NSTACK_MK_DECL
#define NSTACK_MK_DECL(ret, fn, args) \
    g_ksInfo.libcOps.pf##fn = nsfw_base_##fn;

#include "declare_syscalls.h"

  g_ksInfo.regVal = *val;

  /*create a efpd to check fd is ok */
  if (g_ksInfo.checkEpollFD == -1)
    {
      g_ksInfo.checkEpollFD = nsfw_base_epoll_create (1);
    }

  NSSOC_LOGDBG ("start to regist stack");

  ops->socket_ops = g_ksInfo.libcOps;
  MEMSET_S (&(ops->extern_ops), sizeof (ops->extern_ops), 0,
            sizeof (ops->extern_ops));
  NSTACK_SET_OPS_FUN (&(ops->socket_ops), listen, lk_listen);
  NSTACK_SET_OPS_FUN (&(ops->socket_ops), epoll_ctl, lk_epollctl);
  NSTACK_SET_OPS_FUN (&(ops->socket_ops), socket, kernel_socket);
  ops->extern_ops.stack_fd_check = kernel_fd_check;
  ops->extern_ops.ep_ctl = kernel_ep_fd_add;
  ops->extern_ops.ep_prewait_proc = kernel_prewait_proc;
  ops->extern_ops.module_init = kernel_module_init;
  ops->extern_ops.module_init_child = kernel_module_init;
  ops->extern_ops.stack_alloc_fd = kernel_fd_alloc;

  /* don't close file descriptor */

  return retval ? ks_fail : ks_success;
}
