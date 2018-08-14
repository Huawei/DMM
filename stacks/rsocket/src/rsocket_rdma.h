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

#ifndef _RSOCKET_RDMA_H_
#define _RSOCKET_RDMA_H_

#include <stdint.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>
#include <time.h>

enum
{
  RR_LOG_OFF = 0x00,
  RR_LOG_ERR = 0x01,
  RR_LOG_WRN = 0x02,
  RR_LOG_LOG = 0x03,
  RR_LOG_DBG = 0x04,
};

#define RR_OUT(level, name, fmt, arg...) do { \
    extern int g_rr_log_level; \
    if (g_rr_log_level >= level) { \
        (void)printf("#RSOCKET:%s# %s:%d> " fmt "", \
            name, __func__, __LINE__, ##arg); \
    } \
} while (0)

#define RR_ERR(fmt, arg...) RR_OUT(RR_LOG_ERR, "ERR", fmt, ##arg)
#define RR_WRN(fmt, arg...) RR_OUT(RR_LOG_WRN, "WRN", fmt, ##arg)
#define RR_LOG(fmt, arg...) RR_OUT(RR_LOG_LOG, "LOG", fmt, ##arg)
#if defined(RSOCKET_DEBUG) && RSOCKET_DEBUG > 0
#define RR_DBG(fmt, arg...) RR_OUT(RR_LOG_DBG, "DBG", fmt, ##arg)
#else
#define RR_DBG(fmt, arg...) ((void)0)
#endif

#define _err(err_no) ((errno = (err_no)), -1)

int rr_rs_ep_add (int fd, void *pdata, uint32_t * revent);
int rr_rs_ep_mod (int fd, void *pdata, uint32_t * revent);
int rr_rs_ep_del (int fd);

uint32_t rr_rs_poll (int fd, uint32_t revents);

int rr_notify_event (void *pdata, int events);

typedef struct rr_socket_api
{
#define RR_SAPI(name) typeof(name) *n_##name;   /* native api */
#include "rsocket_sapi.h"
#undef RR_SAPI
} rr_sapi_t;

extern rr_sapi_t g_sapi;

#define GSAPI(name) g_sapi.n_##name

int rr_epoll_ctl (int op, int evfd, uint32_t events, int rsfd);

inline static int
rr_ep_add (int evfd, int rsfd)
{
  return rr_epoll_ctl (EPOLL_CTL_ADD, evfd, EPOLLET | EPOLLIN | EPOLLOUT,
                       rsfd);
}

inline static int
rr_ep_del (int evfd)
{
  if (evfd < 0)
    return 0;
  return rr_epoll_ctl (EPOLL_CTL_DEL, evfd, 0, 0);
}

#endif /* #ifndef _RSOCKET_RDMA_H_ */
