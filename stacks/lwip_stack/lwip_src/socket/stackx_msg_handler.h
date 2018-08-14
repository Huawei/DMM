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

#ifndef STACKX_MSG_HANDLER_H
#define STACKX_MSG_HANDLER_H
#include "stackx_socket.h"
#include "stackx_ip_addr.h"
#include "stackx_netbuf.h"
#include "stackx_spl_share.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define SBR_GET_SOCK_NAME 1
#define SBR_GET_PEER_NAME 0

int sbr_handle_socket (sbr_socket_t * sk, spl_netconn_type_t type, u8 proto);

int sbr_handle_bind (sbr_socket_t * sk, spl_ip_addr_t * addr, u16 port);

int sbr_handle_listen (sbr_socket_t * sk, int backlog);

int sbr_handle_connect (sbr_socket_t * sk, spl_ip_addr_t * addr, u16 port,
                        spl_ip_addr_t * local_ip);

int sbr_handle_get_name (sbr_socket_t * sk, struct sockaddr *name,
                         socklen_t * namelen, u8 cmd);

int sbr_handle_setsockopt (sbr_socket_t * sk, int level, int optname,
                           const void *optval, socklen_t optlen);

int sbr_handle_getsockopt (sbr_socket_t * sk, int level, int optname,
                           void *optval, socklen_t * optlen);

int sbr_handle_close (sbr_socket_t * sk, u8 how);

int sbr_handle_udp_send (sbr_socket_t * sk, struct spl_netbuf *buf,
                         spl_ip_addr_t * local_ip);

void sbr_handle_free_recv_buf (sbr_socket_t * sk);

void sbr_handle_free_send_buf (sbr_socket_t * sk, struct spl_pbuf *buf);

int sbr_handle_shutdown (sbr_socket_t * sk, u8 how);

void sbr_handle_tcp_recv (sbr_socket_t * sk, u32 len, struct spl_pbuf *buf);

int sbr_handle_tcp_send (sbr_socket_t * sk, size_t size, struct spl_pbuf *buf,
                         u8 api_flag);

int sbr_handle_custom_send (sbr_socket_t * sk, struct spl_pbuf *buf,
                            spl_ip_addr_t * src, spl_ip_addr_t * dst, u8 tos);

int sbr_handle_custom_close (sbr_socket_t * sk, spl_ip_addr_t * addr);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
