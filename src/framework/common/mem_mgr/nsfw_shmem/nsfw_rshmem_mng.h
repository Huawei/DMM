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

#ifndef _NSFW_RSHMEM_MNG_H
#define _NSFW_RSHMEM_MNG_H

mzone_handle nsfw_memzone_remote_reserv (const i8 * name, size_t mlen,
                                         i32 socket_id);
i32 nsfw_memzone_remote_reserv_v (nsfw_mem_zone * pmeminfo,
                                  mzone_handle * paddr_array, i32 inum,
                                  pid_t pid);
i32 nsfw_remote_free (const i8 * name, nsfw_mem_struct_type entype);
mpool_handle nsfw_remote_shmem_mbf_create (const i8 * name, unsigned int n,
                                           unsigned cache_size,
                                           unsigned priv_size,
                                           unsigned data_room_size,
                                           i32 socket_id,
                                           nsfw_mpool_type entype);
i32 nsfw_remote_shmem_mbf_createv (nsfw_mem_mbfpool * pmbfname,
                                   mpool_handle * phandle_array, i32 inum,
                                   pid_t pid);
mring_handle nsfw_remote_shmem_mpcreate (const char *name, unsigned int n,
                                         unsigned int elt_size, i32 socket_id,
                                         nsfw_mpool_type entype);
i32 nsfw_remote_shmem_mpcreatev (nsfw_mem_sppool * pmpinfo,
                                 mring_handle * pringhandle_array, i32 inum,
                                 pid_t pid);
mring_handle nsfw_remote_shmem_ringcreate (const char *name, unsigned int n,
                                           i32 socket_id,
                                           nsfw_mpool_type entype);
i32 nsfw_remote_shmem_ringcreatev (const char *name, i32 ieltnum,
                                   mring_handle * pringhandle_array,
                                   i32 iringnum, i32 socket_id,
                                   nsfw_mpool_type entype);

void *nsfw_remote_shmem_lookup (const i8 * name, nsfw_mem_struct_type entype);

#endif
