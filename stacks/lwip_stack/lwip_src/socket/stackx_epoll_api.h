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

#ifndef STACKX_EPOLL_API_H
#define STACKX_EPOLL_API_H
#include "stackx_socket.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef enum
{
  EPOLL_API_OP_RECV,
  EPOLL_API_OP_SEND,
  EPOLL_API_OP_STACK_RECV
} EPOLL_TRIGGLE_EVENT_API_OPS_T;

extern void epoll_triggle_event_from_api (sbr_socket_t * sock, int op);
extern unsigned int stackx_eventpoll_getEvt (sbr_socket_t * sock,
                                             unsigned int events);
extern unsigned int stackx_eventpoll_triggle (sbr_socket_t * sock,
                                              int triggle_ops,
                                              struct epoll_event *pevent,
                                              void *pdata);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
