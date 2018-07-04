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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>

#include "nstack_dmm_api.h"

#include "rsocket_adpt.h"
#include "rdma/rsocket.h"


#define RR_EVFD(u64) ((int)((u64) >> 32))
#define RR_RSFD(u64) ((int)((u64) & 0xFFFFFFFF))
#define RR_DATA(evfd, rsfd) ((((uint64_t)(evfd)) << 32) | (uint64_t)(uint32_t)(rsfd))

#define RR_EV_NUM 64

rsocket_var_t g_rr_var = {0};

rr_sapi_t g_sapi = {0};

int g_rr_log_level = -1;


int rr_notify_event(void *pdata, int events)
{
    int ret;

    ret = g_rr_var.event_cb(pdata, events);

    RR_DBG("event_cb(%p, 0x%x)=%d,%d\n", pdata, events, ret, errno);

    return ret;
}

int rr_epoll_ctl(int op, int evfd, uint32_t events, int rsfd)
{
    int ret;
    struct epoll_event event;
    event.events = events;
    event.data.u64 = RR_DATA(evfd, rsfd);
    ret = GSAPI(epoll_ctl)(g_rr_var.epfd, op, evfd, &event);
    return ret;
}

static void *rr_epoll_thread(void *arg)
{
    int i, ret, e;
    struct epoll_event events[RR_EV_NUM];

    while (1) {
        ret = GSAPI(epoll_wait)(g_rr_var.epfd, events, RR_EV_NUM, 100);
        e = errno;

        for (i = 0; i < ret; ++i) {
            if (rr_rs_handle(RR_RSFD(events[i].data.u64), events[i].events)) {
                (void)rr_ep_del(RR_EVFD(events[i].data.u64));
            }
        }

        if (ret < 0) {
            RR_STAT_INC(RR_STAT_EPW_ERR);
            if (e == EINTR) {
                RR_STAT_INC(RR_STAT_EPW_EINTR);
            } else if ( e == ETIMEDOUT) {
                RR_STAT_INC(RR_STAT_EPW_ETIMEOUT);
            } else {
                RR_ERR("epoll_wait()=%d:%d\n", ret, errno);
            }
        }
    }

    return NULL;
}


static int rr_init_sapi()
{
    void *handle = dlopen("libc.so.6", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        RR_ERR("dlopen(libc.so.6):NULL\n");
        return -1;
    }

#define RR_SAPI(name) \
    GSAPI(name) = dlsym(handle, #name); \
    if (!GSAPI(name)) \
        RR_ERR("dlsym(" #name "):NULL\n");
#include "rsocket_sapi.h"
#undef RR_SAPI

    return 0;
}

static void rr_init_log()
{
    int level;
    char *log;

    if (g_rr_log_level >= 0)
        return;

    log = getenv("RSOCKET_LOG");
    if (!log || !log[0]) {
        g_rr_log_level = RR_LOG_OFF;
        return;
    }

    level = atoi(log);
    if (level < 0 || level > 99999) {
        g_rr_log_level = RR_LOG_OFF;
        return;
    }

    g_rr_log_level = level;
}

static int rsocket_init()
{
    int ret;

    rr_init_log();

    if (rr_init_sapi()) {
        return -1;
    }

    g_rr_var.epfd = GSAPI(epoll_create)(1);
    if (g_rr_var.epfd < 0)
        return g_rr_var.epfd;

    ret = pthread_create(&g_rr_var.epoll_threadid, NULL, rr_epoll_thread, NULL);
    if (ret) {
        GSAPI(close)(g_rr_var.epfd);
        g_rr_var.epfd = -1;
        return ret;
    }
    (void)pthread_setname_np(g_rr_var.epoll_threadid, "rsocket_epoll");

    return 0;
}

int rsocket_exit()
{
    if (g_rr_var.epfd >= 0) {
        (void)GSAPI(close)(g_rr_var.epfd);
        g_rr_var.epfd = -1;
    }

    return 0;
}

unsigned int rsocket_ep_ctl(int epFD, int proFD, int ctl_ops, struct epoll_event * event, void *pdata)
{
    int ret;
    struct eventpoll *ep;
    unsigned int revents = 0;

    RR_DBG("(%d, %d, %d, 0x%x, %p)\n", epFD, proFD, ctl_ops, event->events, pdata);

    switch (ctl_ops) {
    case nstack_ep_triggle_add:
        ret = rr_rs_ep_add(proFD, pdata, &revents);
        if (ret)
            return -1;
        return revents;

    case nstack_ep_triggle_mod:
        ret = rr_rs_ep_mod(proFD, pdata, &revents);
        if (ret)
            return -1;
        return revents;
    case nstack_ep_triggle_del:
        return rr_rs_ep_del(proFD);
    }

    return _err(EPERM);
}

int rsocket_stack_register(nstack_proc_cb *proc_fun, nstack_event_cb *event_ops)
{
    rr_init_log();

#define NSTACK_MK_DECL(ret, fn, args) \
    do { \
        proc_fun->socket_ops.pf##fn = dlsym(event_ops->handle, "r"#fn); \
        if (!proc_fun->socket_ops.pf##fn) \
            RR_LOG("socket API '" #fn "' not found\n"); \
    } while (0)
#include "declare_syscalls.h"
#undef NSTACK_MK_DECL

    proc_fun->extern_ops.module_init = rsocket_init;
    proc_fun->extern_ops.ep_ctl = rsocket_ep_ctl;
    proc_fun->extern_ops.ep_getevt = NULL;

    g_rr_var.type = event_ops->type;
    g_rr_var.event_cb = event_ops->event_cb;

    return 0;
}

