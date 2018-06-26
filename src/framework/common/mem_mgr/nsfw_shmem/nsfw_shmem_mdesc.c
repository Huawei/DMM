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
#include "nsfw_shmem_mng.h"
#include "nsfw_shmem_mdesc.h"

/*the inferaces accessing memory*/
nsfw_mem_ops g_shmem_ops = {
  nsfw_shmem_init,
  nsfw_shmem_destroy,
  nsfw_shmem_create,
  nsfw_shmem_createv,
  nsfw_shmem_lookup,
  nsfw_shmem_release,
  nsfw_shmem_mbfmpcreate,
  nsfw_shmem_mbfmpcreatev,
  nsfw_shmem_mbfalloc,
  nsfw_shmem_mbffree,
  nsfw_shmem_mbfmplookup,
  nsfw_shmem_mbfmprelease,
  nsfw_shmem_spcreate,
  nsfw_shmem_spcreatev,
  nswf_shmem_sp_ringcreate,
  nsfw_shmem_sprelease,
  nsfw_shmem_sp_lookup,
  nsfw_shmem_ringcreate,
  nsfw_shmem_ring_lookup,
  nsfw_shmem_ringrelease,
  nsfw_shmem_static,
  nsfw_shmem_mbuf_recycle,
  nsfw_shmem_sp_iterator,
  nsfw_shmem_mbuf_iterator
};
