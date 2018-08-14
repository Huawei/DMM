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

#ifndef _RSOCKET_RS_C_
#define _RSOCKET_RS_C_

inline static void
rr_rs_init (struct rsocket *rs)
{
  RR_DBG ("(rs:%p{index:%d})\n", rs, rs->index);
  rs->rr_epoll_ref = 0;
  rs->rr_epoll_fd = -1;
  rs->rr_epoll_pdata = NULL;
}

inline static void
rr_rs_dest (struct rsocket *rs)
{
  RR_DBG ("(rs:%p{index:%d})\n", rs, rs->index);

  if (rs->rr_epoll_ref)
    {
      (void) rr_ep_del (rs->rr_epoll_fd);
      rs->rr_epoll_ref = 0;
      rs->rr_epoll_fd = -1;
      rs->rr_epoll_pdata = NULL;
    }
}

#ifndef POLL__RSOCKET_RS_H_
#define POLL__RSOCKET_RS_H_

static inline uint32_t
rr_rs_poll_tcp (struct rsocket *rs)
{
  uint32_t events = 0;
  if (rs->state & rs_connected)
    {
      if (rs_have_rdata (rs))
        events |= EPOLLIN;
      if (rs_can_send (rs))
        events |= EPOLLOUT;
    }
  if (rs->state & (rs_error | rs_connect_error))
    events |= EPOLLERR;
  if (rs->state & rs_disconnected)
    events |= EPOLLHUP;
  return events;
}

static inline uint32_t
rr_rs_poll_udp (struct rsocket *rs)
{
  uint32_t events = 0;
  if (rs_have_rdata (rs))
    events |= EPOLLIN;
  if (ds_can_send (rs))
    events |= EPOLLOUT;
  if (rs->state & rs_error)
    events |= EPOLLERR;
  return events;
}

static inline uint32_t
rr_rs_poll_both (struct rsocket *rs)
{
  if (rs->type == SOCK_STREAM)
    return rr_rs_poll_tcp (rs);

  if (rs->type == SOCK_DGRAM)
    return rr_rs_poll_udp (rs);

  return 0;
}

uint32_t
rr_rs_poll (int fd, uint32_t revents)
{
  struct rsocket *rs = (struct rsocket *) idm_lookup (&idm, fd);

  if (!rs)
    return 0;

  if (rs->state == rs_listening)
    return revents;

  return rr_rs_poll_both (rs);
}

#endif /* #ifndef POLL__RSOCKET_RS_H_ */

static inline void
rr_rs_notify_tcp (struct rsocket *rs)
{
  if (rs->rr_epoll_ref)
    {
      uint32_t events = rr_rs_poll_tcp (rs);
      if (events)
        (void) rr_notify_event (rs->rr_epoll_pdata, events);
    }
}

static inline void
rr_rs_notify_udp (struct rsocket *rs)
{
  if (rs->rr_epoll_ref)
    {
      uint32_t events = rr_rs_poll_udp (rs);
      if (events)
        (void) rr_notify_event (rs->rr_epoll_pdata, events);
    }
}

#ifndef HANDLE__RSOCKET_RS_H_
#define HANDLE__RSOCKET_RS_H_

inline static void
rr_rs_handle_tcp (struct rsocket *rs)
{
  int ret;

  RR_DBG ("(%d)@ state:0x%x\n", rs->index, rs->state);

  if (!(rs->state & (rs_connected | rs_opening)))
    return;

  fastlock_acquire (&rs->cq_wait_lock);
  ret = rs_get_cq_event (rs);
  RR_DBG ("rs_get_cq_event({%d})=%d,%d\n", rs->index, ret, errno);
  fastlock_release (&rs->cq_wait_lock);

  fastlock_acquire (&rs->cq_lock);

  if (rs->state & rs_connected)
    {
      rs_update_credits (rs);
      ret = rs_poll_cq (rs);
      RR_DBG ("rs_poll_cq({%d})=%d,%d {ref:%d, armed:%d}\n",
              rs->index, ret, errno, rs->rr_epoll_ref, rs->cq_armed);
    }

  if (rs->rr_epoll_ref && rs->cq_armed < 1)
    {
      ret = ibv_req_notify_cq (rs->cm_id->recv_cq, 0);
      RR_DBG ("ibv_req_notify_cq({%d})=%d,%d\n", rs->index, ret, errno);
      if (0 == ret)
        __sync_fetch_and_add (&rs->cq_armed, 1);
    }

  if (rs->state & rs_connected)
    {
      ret = rs_poll_cq (rs);
      RR_DBG ("rs_poll_cq({%d})=%d,%d\n", rs->index, ret, errno);
      rs_update_credits (rs);
    }

  fastlock_release (&rs->cq_lock);

  RR_DBG ("(%d)=\n", rs->index);
}

inline static void
rr_rs_handle_udp (struct rsocket *rs)
{
  fastlock_acquire (&rs->cq_wait_lock);
  ds_get_cq_event (rs);
  fastlock_release (&rs->cq_wait_lock);

  fastlock_acquire (&rs->cq_lock);
  ds_poll_cqs (rs);
  if (rs->rr_epoll_ref && !rs->cq_armed)
    {
      ds_req_notify_cqs (rs);
      rs->cq_armed = 1;
    }
  fastlock_release (&rs->cq_lock);
}

inline static void
rr_rs_handle_rs (struct rsocket *rs)
{
  if (rs->state & rs_opening)
    {
      int ret = rs_do_connect (rs);
      RR_DBG ("rs_do_connect(%p{%d}):%d:%d\n", rs, rs->index, ret, errno);
      return;
    }

  if (rs->type == SOCK_STREAM)
    {
      rr_rs_handle_tcp (rs);
    }

  if (rs->type == SOCK_DGRAM)
    {
      rr_rs_handle_udp (rs);
    }
}

int
rr_rs_handle (int fd, uint32_t events)
{
  struct rsocket *rs = (struct rsocket *) idm_lookup (&idm, fd);

  RR_DBG ("(fd:%d, events:0x%x):rs:%p\n", fd, events, rs);

  if (!rs)
    return _err (EBADF);

  if (rs->state == rs_listening)
    {
      if (events & EPOLLIN)
        {
          (void) rr_notify_event (rs->rr_epoll_pdata, events);
        }
      return 0;
    }

  rr_rs_handle_rs (rs);

  return 0;
}

#endif /* #ifndef HANDLE__RSOCKET_RS_H_ */

#ifndef ADPT__RSOCKET_RS_H_
#define ADPT__RSOCKET_RS_H_

inline static int
rr_rs_evfd (struct rsocket *rs)
{
  if (rs->type == SOCK_STREAM)
    {
      if (rs->state >= rs_connected)
        return rs->cm_id->recv_cq_channel->fd;
      else
        return rs->cm_id->channel->fd;
    }
  else
    {
      return rs->epfd;
    }

  return -1;
}

int
rr_rs_ep_add (int fd, void *pdata, uint32_t * revent)
{
  int ref;
  struct rsocket *rs = (struct rsocket *) idm_lookup (&idm, fd);
  RR_DBG ("(%d(%p),)\n", fd, rs);
  if (!rs)
    return _err (EBADF);

  ref = __sync_add_and_fetch (&rs->rr_epoll_ref, 1);
  if (1 == ref)
    {
      rs->rr_epoll_fd = rr_rs_evfd (rs);
      (void) rr_ep_add (rs->rr_epoll_fd, rs->index);
    }

  (void) rr_rs_handle_rs (rs);
  *revent = rs->state == rs_listening ? 0 : rr_rs_poll_both (rs);

  rs->rr_epoll_pdata = pdata;

  RR_DBG ("*revent=0x%x\n", *revent);
  return 0;
}

int
rr_rs_ep_mod (int fd, void *pdata, uint32_t * revent)
{
  struct rsocket *rs = (struct rsocket *) idm_lookup (&idm, fd);
  RR_DBG ("(%d(%p),)\n", fd, rs);
  if (!rs)
    return _err (EBADF);

  if (rs->rr_epoll_ref <= 0)
    return _err (ENOENT);

  (void) rr_rs_handle_rs (rs);
  *revent = rs->state == rs_listening ? 0 : rr_rs_poll_both (rs);

  rs->rr_epoll_pdata = pdata;

  RR_DBG ("*revent=0x%x\n", *revent);
  return 0;
}

int
rr_rs_ep_del (int fd)
{
  int ref;
  struct rsocket *rs = (struct rsocket *) idm_lookup (&idm, fd);
  RR_DBG ("(%d(%p))\n", fd, rs);

  if (!rs)
    return _err (EBADF);

  ref = __sync_sub_and_fetch (&rs->rr_epoll_ref, 1);
  if (0 == ref)
    {
      (void) rr_ep_del (rs->rr_epoll_fd);
      rs->rr_epoll_fd = -1;
    }

  return 0;
}

#endif /* #ifndef ADPT__RSOCKET_RS_H_ */

inline static void
rr_rs_connected (struct rsocket *rs)
{
  RR_DBG ("rsfd:%d ref:%d evfd:%d->%d state:0x%x\n", rs->index,
          rs->rr_epoll_ref, rs->rr_epoll_fd, rr_rs_evfd (rs), rs->state);

  if (!(rs->state & rs_connected))
    {
      rr_rs_notify_tcp (rs);
      return;
    }

  if (rs->rr_epoll_ref)
    {
      int evfd = rr_rs_evfd (rs);

      if (evfd != rs->rr_epoll_fd)
        {
          (void) rr_ep_del (rs->rr_epoll_fd);
          rs->rr_epoll_fd = evfd;
          (void) rr_ep_add (evfd, rs->index);
        }

      rr_rs_handle_tcp (rs);
    }
}

int
raccept4 (int socket, struct sockaddr *addr, socklen_t * addrlen, int flags)
{
  int ret, fd;
  struct rsocket *rs;

  RR_DBG ("(%d, %p, %p, %d)@\n", socket, addr, addrlen, flags);
  fd = raccept (socket, addr, addrlen);
  RR_DBG ("(%d, , , %d):%d:%d\n", socket, flags, fd, errno);
  if (fd < 0)
    return fd;

  rs = (struct rsocket *) idm_lookup (&idm, fd);
  if (!rs)
    {
      RR_ERR ("panic\n");
      return -1;
    }

  if (flags & SOCK_NONBLOCK)
    {
      if (0 == (rs->fd_flags & O_NONBLOCK))
        {
          RR_DBG ("orig flag:%x\n",
                  GSAPI (fcntl) (rs->cm_id->channel->fd, F_GETFL));
          ret = GSAPI (fcntl) (rs->cm_id->channel->fd, F_SETFL, O_NONBLOCK);
          if (0 == ret)
            rs->fd_flags |= O_NONBLOCK;
        }
    }

  if (flags & SOCK_CLOEXEC)
    {
      RR_LOG ("ignore flag:SOCK_CLOEXEC\n");
    }

  return fd;
}

#endif /* #ifndef _RSOCKET_RS_C_ */
