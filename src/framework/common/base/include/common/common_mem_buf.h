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

#ifndef _COMMON_MEM_BUF_H_
#define _COMMON_MEM_BUF_H_

#ifdef HAL_LIB
#else

#include "common_mem_base_type.h"
#include "types.h"

typedef enum __DMM_PROC_TYPE
{
  DMM_PROC_T_AUTO = 0,          /*auto detect */
  DMM_PROC_T_PRIMARY = 1,       /* set to primary */
  DMM_PROC_T_SECONDARY = 2,     /* set to secondary */
  DMM_PROC_T_INVALID
} DMM_PROC_TYPE;

#define  DMM_MBUF_RET_OK    0
#define  DMM_MBUF_RET_ERR   1

#define LCORE_MAX        128
#define LCORE_MASK_PER   (sizeof(int) * 8)
#define LCORE_MASK_MAX   (LCORE_MAX/LCORE_MASK_PER)

#define LCORE_MASK_SET(ilcoremask, value) \
    if (value < LCORE_MAX) \
    { \
       ilcoremask[(value/LCORE_MASK_PER)] = (int) ( (ilcoremask[(value/LCORE_MASK_PER)]) | (1< (value%LCORE_MASK_PER))); \
    } \

#define  DMM_HUGTBL_ENABLE   0
#define  DMM_HUGTBL_DISABLE  1

typedef struct __common_pal_module_info
{
  int ishare_mem_size;          /*shared memory size */
  int ilcoremask[LCORE_MASK_MAX];
    /**/ unsigned char uchugeflag;
  unsigned char ucproctype;
  unsigned char ucinstance;
  unsigned char ucresrv2;
} common_mem_pal_module_info;

/**
 * rte pal module init.
 *
 *
 * @param name
 *   The name of the buf pool.
 */
int nscomm_pal_module_init (common_mem_pal_module_info * pinfo, u8 app_mode);

void *nscomm_memzone_data_reserve_name (const char *name, size_t len,
                                        int socket_id);

void *nscomm_memzone_data_lookup_name (const char *name);

#endif

#endif /* _COMMON_MEM_BUF_H_ */
