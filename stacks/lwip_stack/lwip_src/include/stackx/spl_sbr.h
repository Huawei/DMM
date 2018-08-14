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

#ifndef _SPL_SBR_H_
#define _SPL_SBR_H_
#include "nsfw_msg_api.h"
#include "tcp.h"
#include "udp.h"
//#include "stackx/raw.h"
#define COMM_PRIVATE_PTR(m) \
    ((m->param.receiver) ? ((struct common_pcb *)m->param.comm_receiver) : 0)
#define TCP_PRIVATE_PTR(m) \
    ((m->param.receiver) ? (*(struct tcp_pcb **)m->param.receiver) : 0)
#define UDP_PRIVATE_PTR(m) \
    ((m->param.receiver) ? (*(struct udp_pcb **)m->param.receiver) : 0)
/*#define RAW_PRIVATE_PTR(m) \
    ((m->param.receiver) ? (*(void *)m->param.receiver) : 0) */

int spl_sbr_process (data_com_msg * m);
#endif /* _SPL_SBR_H_ */
