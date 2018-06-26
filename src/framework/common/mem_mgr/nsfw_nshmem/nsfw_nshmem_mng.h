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

#ifndef _NSFW_NSHMEM_MNG_H_
#define _NSFW_NSHMEM_MNG_H_

#include "generic/common_mem_rwlock.h"

#include "common_func.h"

#define NSFW_NSHMEM_ZONE_MAX    2560

typedef struct
{
  i8 aname[NSFW_MEM_NAME_LENGTH];
  void *addr;
  int length;
} nsfw_nshmem_mzone;

typedef struct
{
  nsfw_nshmem_mzone amemzone[NSFW_NSHMEM_ZONE_MAX];
  common_mem_rwlock_t mlock;
} nsfw_nshmem_cfg;

/*
 * no share memory module init
 */
i32 nsfw_nshmem_init (nsfw_mem_para * para);

/*
 * no share memory module destory
 */
void nsfw_nshmem_destory (void);

/*
 * create a no shared memory
 */
mzone_handle nsfw_nshmem_create (nsfw_mem_zone * pinfo);

mzone_handle nsfw_nshmem_lookup (nsfw_mem_name * pname);

i32 nsfw_nshmem_release (nsfw_mem_name * pname);

mring_handle nsfw_nshmem_spcreate (nsfw_mem_sppool * pmpinfo);

i32 nsfw_nshmem_sprelease (nsfw_mem_name * pname);

mring_handle nsfw_nshmem_sp_lookup (nsfw_mem_name * pname);

mring_handle nsfw_nshmem_ringcreate (nsfw_mem_mring * pringinfo);

i32 nsfw_nshmem_ringrelease (nsfw_mem_name * pname);

ssize_t nsfw_nshmem_static (void *handle, nsfw_mem_struct_type type);

#endif
