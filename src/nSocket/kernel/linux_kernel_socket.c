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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "linux_kernel_socket.h"
#include "linux_kernel_module.h"
#include "nstack_sockops.h"
#include "nstack_securec.h"

int
lk_listen (int sockfd, int backlog)
{
  int ret = -1;

  NSTACK_CAL_FUN (&g_ksInfo.libcOps, listen, (sockfd, backlog), ret);
  return ret;
}

int
lk_epollctl (int epfd, int op, int protoFd, struct epoll_event *event)
{
  int retVal;
  struct epoll_event ev;

  /* Input parameter validation */
  if (NULL == event)
    {
      NSSOC_LOGERR ("input param event is NULL");
      return -1;
    }

  retVal =
    MEMCPY_S (&ev, sizeof (struct epoll_event), event,
              sizeof (struct epoll_event));
  if (EOK != retVal)
    {
      NSSOC_LOGERR ("MEMCPY_S failed]ret=%d", retVal);
      return -1;
    }
  ev.data.fd = protoFd;
  return g_ksInfo.libcOps.pfepoll_ctl (g_ksInfo.epfd, op, protoFd, &ev);
}
