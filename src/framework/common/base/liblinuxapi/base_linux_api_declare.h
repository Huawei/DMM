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

BASE_MK_DECL (int, socket, (int, int, int));
BASE_MK_DECL (int, bind, (int, const struct sockaddr *, socklen_t));
BASE_MK_DECL (int, listen, (int, int));
BASE_MK_DECL (int, shutdown, (int, int));
BASE_MK_DECL (int, getsockname, (int, struct sockaddr *, socklen_t *));
BASE_MK_DECL (int, getpeername, (int, struct sockaddr *, socklen_t *));
BASE_MK_DECL (int, getsockopt, (int, int, int, void *, socklen_t *));
BASE_MK_DECL (int, setsockopt, (int, int, int, const void *, socklen_t));
BASE_MK_DECL (int, accept, (int, struct sockaddr *, socklen_t *));
BASE_MK_DECL (int, accept4, (int, struct sockaddr *, socklen_t *, int flags));
BASE_MK_DECL (int, connect, (int, const struct sockaddr *, socklen_t));
BASE_MK_DECL (ssize_t, recv, (int, void *, size_t, int));
BASE_MK_DECL (ssize_t, send, (int, const void *, size_t, int));
BASE_MK_DECL (ssize_t, read, (int, void *, size_t));
BASE_MK_DECL (ssize_t, write, (int, const void *, size_t));
BASE_MK_DECL (ssize_t, writev, (int, const struct iovec *, int));
BASE_MK_DECL (ssize_t, readv, (int, const struct iovec *, int));
BASE_MK_DECL (ssize_t, sendto,
              (int, const void *, size_t, int, const struct sockaddr *,
               socklen_t));
BASE_MK_DECL (ssize_t, recvfrom,
              (int, void *, size_t, int, struct sockaddr *, socklen_t *));
BASE_MK_DECL (ssize_t, sendmsg, (int, const struct msghdr *, int flags));
BASE_MK_DECL (ssize_t, recvmsg, (int, struct msghdr *, int flags));
BASE_MK_DECL (int, close, (int));
BASE_MK_DECL (int, select,
              (int, fd_set *, fd_set *, fd_set *, struct timeval *));
BASE_MK_DECL (int, ioctl, (int, unsigned long, unsigned long));
BASE_MK_DECL (int, fcntl, (int, int, unsigned long));
BASE_MK_DECL (int, epoll_create, (int));
BASE_MK_DECL (int, epoll_create1, (int));
BASE_MK_DECL (int, epoll_ctl, (int, int, int, struct epoll_event *));
BASE_MK_DECL (int, epoll_wait, (int, struct epoll_event *, int, int));
BASE_MK_DECL (pid_t, fork, (void));
#undef BASE_MK_DECL
