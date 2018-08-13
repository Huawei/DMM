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

#ifndef  __STACKX_EVENT_H__
#define __STACKX_EVENT_H__

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include "stackx_socket.h"
#include "nstack_securec.h"
#include "sbr_res_mgr.h"
#include "sbr_index_ring.h"
#define NSTACK_SETSIZE  SBR_MAX_FD_NUM

typedef struct
{
  unsigned char fds_bits[(NSTACK_SETSIZE + 7) / 8];
} __attribute__ ((packed)) nstack_fd_set;

#define NSTACK_FD_SET(n, p)		((p)->fds_bits[(n)/8]|=1U<<((n)&0x07))
#define NSTACK_FD_ISSET(n,p)	(((p)->fds_bits[(n)/8]&(1U<<((n)&0x07)))?1:0)
#define NSTACK_FD_CLR(n,p)		((p)->fds_bits[(n)/8]&=~(1U<<((n)&0x07)))
#define NSTACK_FD_ZERO(p)		(MEMSET_S((void *)(p), sizeof(*(p)),0,sizeof(*(p))))
#define NSTACK_FD_OR(p1 ,p2)     {\
    int i;\
    for(i = 0; i < (NSTACK_SETSIZE+7)/8; i++){\
        (p1)->fds_bits[i] |= (p2)->fds_bits[i];\
    }\
}

#endif /* __STACKX_EVENT_H__ */
