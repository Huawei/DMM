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

#ifndef included_dmm_vcl_h
#define included_dmm_vcl_h

#include "nstack_dmm_api.h"

#define DMM_VCL_ENV_DEBUG     "DMM_VCL_DEBUG"

typedef struct dmm_vcl
{
  int epfd;
  long unsigned int epoll_threadid;
  nstack_event_cb regVal;
  int (*p_epoll_create) (int size);
  unsigned int (*p_epoll_ctl) (int epFD, int proFD, int ctl_ops,
                               struct epoll_event * events);
  unsigned int (*p_epoll_wait) (int epfd, struct epoll_event * events,
                                int maxevents, int timeout);
  int (*p_close) (int fd);
} dmm_vcl_t;

#endif /* included_dmm_vcl_h */
