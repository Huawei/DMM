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

#include "nsfw_mem_desc.h"
#include "nsfw_nshmem_mng.h"
#include "nsfw_nshmem_mdesc.h"

/*no share memory access inferface*/
nsfw_mem_ops g_nshmem_ops = {
  nsfw_nshmem_init,
  nsfw_nshmem_destory,
  nsfw_nshmem_create,
  NULL,
  nsfw_nshmem_lookup,
  nsfw_nshmem_release,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  nsfw_nshmem_spcreate,
  NULL,
  NULL,
  nsfw_nshmem_sprelease,
  nsfw_nshmem_sp_lookup,
  nsfw_nshmem_ringcreate,
  NULL,
  nsfw_nshmem_ringrelease,
  nsfw_nshmem_static,
  NULL,
  NULL,                         /*mem_ops_sp_iterator */
  NULL,                         /*mem_ops_mbuf_iterator */
};
