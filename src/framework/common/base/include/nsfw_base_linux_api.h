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

#ifndef _NSFW_BASE_LINUX_API_H_
#define _NSFW_BASE_LINUX_API_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>

int nsfw_base_socket (int, int, int);
int nsfw_base_bind (int, const struct sockaddr *, socklen_t);
int nsfw_base_listen (int, int);
int nsfw_base_shutdown (int, int);
int nsfw_base_getsockname (int, struct sockaddr *, socklen_t *);
int nsfw_base_getpeername (int, struct sockaddr *, socklen_t *);
int nsfw_base_getsockopt (int, int, int, void *, socklen_t *);
int nsfw_base_setsockopt (int, int, int, const void *, socklen_t);
int nsfw_base_accept (int, struct sockaddr *, socklen_t *);
int nsfw_base_accept4 (int, struct sockaddr *, socklen_t *, int flags);
int nsfw_base_connect (int, const struct sockaddr *, socklen_t);
ssize_t nsfw_base_recv (int, void *, size_t, int);
ssize_t nsfw_base_send (int, const void *, size_t, int);
ssize_t nsfw_base_read (int, void *, size_t);
ssize_t nsfw_base_write (int, const void *, size_t);
ssize_t nsfw_base_writev (int, const struct iovec *, int);
ssize_t nsfw_base_readv (int, const struct iovec *, int);
ssize_t nsfw_base_sendto (int, const void *, size_t, int,
                          const struct sockaddr *, socklen_t);
ssize_t nsfw_base_recvfrom (int, void *, size_t, int, struct sockaddr *,
                            socklen_t *);
ssize_t nsfw_base_sendmsg (int, const struct msghdr *, int flags);
ssize_t nsfw_base_recvmsg (int, struct msghdr *, int flags);
int nsfw_base_close (int);
int nsfw_base_select (int, fd_set *, fd_set *, fd_set *, struct timeval *);
int nsfw_base_ioctl (int, unsigned long, unsigned long);
int nsfw_base_fcntl (int, int, unsigned long);
int nsfw_base_epoll_create (int);
int nsfw_base_epoll_create1 (int);
int nsfw_base_epoll_ctl (int, int, int, struct epoll_event *);
int nsfw_base_epoll_wait (int, struct epoll_event *, int, int);
pid_t nsfw_base_fork (void);

#endif
