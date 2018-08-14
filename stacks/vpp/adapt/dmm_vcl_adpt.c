/*
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

#define _GNU_SOURCE
#include <pthread.h>
#include <dlfcn.h>
#include <sys/epoll.h>
#include "dmm_vcl.h"
#include "nstack_dmm_api.h"     // nstack_socket_ops*
#include <vppinfra/error.h>     // clib_warning()

#define DMM_VCL_ADPT_DEBUG dmm_vcl_debug
static unsigned int dmm_vcl_debug;
dmm_vcl_t g_dmm_vcl;

unsigned int
vpphs_ep_ctl_ops (int epFD, int proFD, int ctl_ops,
                  struct epoll_event *events, void *pdata)
{
  struct epoll_event tmpEvt;
  int ret = 0;
  int dmm_epfd;

  tmpEvt.data.ptr = pdata;
  tmpEvt.events = events->events;

  if (DMM_VCL_ADPT_DEBUG > 0)
    clib_warning ("DMM VCL ADPT<%d>: epfd=%d,fd=%d,ops=%d, events=%u",
                  getpid (), epFD, proFD, ctl_ops, events->events);

  dmm_epfd = g_dmm_vcl.epfd;
  switch (ctl_ops)
    {
    case nstack_ep_triggle_add:
      ret = g_dmm_vcl.p_epoll_ctl (dmm_epfd, EPOLL_CTL_ADD, proFD, &tmpEvt);
      break;
    case nstack_ep_triggle_mod:
      ret = g_dmm_vcl.p_epoll_ctl (dmm_epfd, EPOLL_CTL_MOD, proFD, &tmpEvt);
      break;
    case nstack_ep_triggle_del:
      ret = g_dmm_vcl.p_epoll_ctl (dmm_epfd, EPOLL_CTL_DEL, proFD, &tmpEvt);
      break;
    default:
      ret = -1;
      break;
    }
  return ret;
}

#define DMM_VCL_MAX_EP_EVENT 1024

static void *
dmm_vcl_epoll_thread (void *arg)
{
  int num, i;

  struct epoll_event events[DMM_VCL_MAX_EP_EVENT];

  while (1)
    {
      num =
        g_dmm_vcl.p_epoll_wait (g_dmm_vcl.epfd, events, DMM_VCL_MAX_EP_EVENT,
                                100);

      for (i = 0; i < num; ++i)
        {
          if (DMM_VCL_ADPT_DEBUG > 0)
            clib_warning
              ("DMM_VCL_ADPT<%d>: dmm_vcl_epoll i[%d] events=%u, epfd=%d, ptr=%d",
               getpid (), i, events[i].events, events[i].data.fd,
               events[i].data.ptr);

          g_dmm_vcl.regVal.event_cb (events[i].data.ptr, events[i].events);

        }
    }

  return NULL;
}

int
dmm_vpphs_init ()
{
  char *env_var_str;
  int rv = 0;

  env_var_str = getenv (DMM_VCL_ENV_DEBUG);
  if (env_var_str)
    {
      u32 tmp;
      if (sscanf (env_var_str, "%u", &tmp) != 1)
        clib_warning
          ("DMM_VCL_ADPT<%d>: WARNING: Invalid debug level specified "
           "in the environment variable " DMM_VCL_ENV_DEBUG " (%s)!\n",
           getpid (), env_var_str);
      else
        {
          dmm_vcl_debug = tmp;
          if (DMM_VCL_ADPT_DEBUG > 0)
            clib_warning
              ("DMM_VCL_ADPT<%d>: configured DMM VCL ADPT debug (%u) from "
               "DMM_VCL_ENV_DEBUG ", getpid (), dmm_vcl_debug);
        }
    }

  g_dmm_vcl.epfd = g_dmm_vcl.p_epoll_create (1000);
  if (g_dmm_vcl.epfd < 0)
    return g_dmm_vcl.epfd;

  rv =
    pthread_create (&g_dmm_vcl.epoll_threadid, NULL, dmm_vcl_epoll_thread,
                    NULL);
  if (rv != 0)
    {
      clib_warning ("dmm vcl epoll thread create fail, errno:%d!", errno);
      g_dmm_vcl.p_close (g_dmm_vcl.epfd);
      g_dmm_vcl.epfd = -1;
      return rv;
    }

  rv = pthread_setname_np (g_dmm_vcl.epoll_threadid, "dmm_vcl_epoll");
  if (rv != 0)
    {
      clib_warning
        ("pthread_setname_np failed for dmm_vcl_epoll, rv=%d, errno:%d",
         rv, errno);
    }

  return rv;
}

int
vpphs_stack_register (nstack_proc_cb * ops, nstack_event_cb * val)
{

#undef NSTACK_MK_DECL
#define NSTACK_MK_DECL(ret, fn, args) \
    (ops->socket_ops).pf ## fn = (typeof(((nstack_socket_ops*)0)->pf ## fn))dlsym(val->handle,  # fn);
#include "declare_syscalls.h"
  (ops->socket_ops).pfepoll_create = NULL;

  g_dmm_vcl.p_epoll_ctl = dlsym (val->handle, "epoll_ctl");
  g_dmm_vcl.p_epoll_create = dlsym (val->handle, "epoll_create1");
  g_dmm_vcl.p_epoll_wait = dlsym (val->handle, "epoll_wait");
  g_dmm_vcl.p_close = dlsym (val->handle, "close");
  g_dmm_vcl.regVal = *val;

  ops->extern_ops.module_init = dmm_vpphs_init;
  ops->extern_ops.fork_init_child = NULL;
  ops->extern_ops.fork_parent_fd = NULL;
  ops->extern_ops.fork_child_fd = NULL;
  ops->extern_ops.fork_free_fd = NULL;
  ops->extern_ops.ep_ctl = vpphs_ep_ctl_ops;
  ops->extern_ops.ep_prewait_proc = NULL;
  ops->extern_ops.stack_fd_check = NULL;
  ops->extern_ops.stack_alloc_fd = NULL;
  ops->extern_ops.peak = NULL;

  return 0;
}
