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

#ifndef STACKX_CONTAINER_CFG_H
#define STACKX_CONTAINER_CFG_H
#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define SBR_MAX_CFG_PATH_LEN 256
#define SBR_MAX_CONTAINER_IP_NUM 1024
#define SBR_MAX_CFG_FILE_SIZE (1 * 1024 * 1024)

typedef struct
{
  u32 ip;
  u32 mask_len;
} sbr_container_ip;

typedef struct
{
  sbr_container_ip ip_array[SBR_MAX_CONTAINER_IP_NUM];
  u32 ip_num;
} sbr_container_ip_group;

extern sbr_container_ip_group g_container_ip_group;

int sbr_init_cfg ();
int sbr_get_src_ip (u32 dst_ip, u32 * src_ip);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
