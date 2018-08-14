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

#ifndef STACKX_COMMON_H
#define STACKX_COMMON_H
#include "nsfw_mem_api.h"
//#include "lwip/cc.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

int spl_snprintf (char *buffer, int buflen, const char *format, ...);
mzone_handle sbr_create_mzone (const char *name, size_t size);
mzone_handle sbr_lookup_mzone (const char *name);
mring_handle sbr_create_pool (const char *name, i32 num, u16 size);
mring_handle sbr_create_ring (const char *name, i32 num);
int sbr_create_multi_ring (const char *name, u32 ring_size, i32 ring_num,
                           mring_handle * array, nsfw_mpool_type type);
mring_handle sbr_lookup_ring (const char *name);
int sbr_timeval2msec (struct timeval *pTime, u64 * msec);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
