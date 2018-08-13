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

#ifndef STACKX_OPTS_H
#define STACKX_OPTS_H

struct memory_statics
{
  char name[120];
  long size;
};

extern u32 g_type;
extern struct memory_statics memory_used_size[80];

#define DPDK_MEMORY_COUNT(memory, len)\
{\
    if (g_type < MAX_MEMORY_USED_SIZE)\
    {\
	    const char * ptr_memory = memory;\
        if (ptr_memory && EOK != STRCPY_S(memory_used_size[g_type].name, sizeof(memory_used_size[g_type].name), ptr_memory))\
        {\
           NSPOL_LOGERR("STRCPY_S failed.");\
        }\
        memory_used_size[g_type].size =(long) len;\
        g_type++;\
    }\
}

#define SPL_MAX_UDP_MSG_LEN           (0xFFFF -28)
#define RECV_MAX_POOL                 4
#define MAX_TRY_GET_MEMORY_TIMES      4
#define MAX_MEMORY_USED_SIZE          80
#define SPL_IP_HLEN	                  20
#define SPL_TCP_HLEN	              20
#define SPL_TCP_MAX_OPTION_LEN        40
#define SPL_FRAME_MTU                 1500
#define SPL_TCP_SEND_MAX_SEG_PER_MSG  5

#endif
