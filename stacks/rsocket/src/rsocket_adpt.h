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

#ifndef _RSOCKET_ADPT_H_
#define _RSOCKET_ADPT_H_

#include "indexer.h"
#include "rsocket_rdma.h"

enum
{
  RR_STAT_EPW_ERR,
  RR_STAT_EPW_EINTR,
  RR_STAT_EPW_ETIMEOUT,

  RR_STAT_NUM
};

#define RR_STAT_ADD(id, num) __sync_add_and_fetch(&g_rr_var.stat[(id)], num)
#define RR_STAT_SUB(id, num) __sync_sub_and_fetch(&g_rr_var.stat[(id)], num)
#define RR_STAT_INC(id) RR_STAT_ADD((id), 1)
#define RR_STAT_DEC(id) RR_STAT_SUB((id), 1)

#define RSRDMA_EXIT 1

typedef struct rsocket_var
{
  pthread_t epoll_threadid;

  int epfd;
  int type;
  int (*event_cb) (void *pdata, int events);

  uint64_t stat[RR_STAT_NUM];
} rsocket_var_t;

extern rsocket_var_t g_rr_var;

int rr_rs_handle (int fd, uint32_t events);

#endif /* #ifndef _RSOCKET_ADPT_H_ */
