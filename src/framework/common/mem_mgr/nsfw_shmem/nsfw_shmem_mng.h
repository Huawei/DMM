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

#ifndef _NSFW_SHMEM_MNG_H
#define _NSFW_SHMEM_MNG_H

/*
 * mem mgr module init
 * para:point to nstack_fwmem_para
 */
i32 nsfw_shmem_init (nsfw_mem_para * para);

/*
 * mem mgr module destory
 *
 */
void nsfw_shmem_destroy (void);

/*
 * create a block memory with name
 * fw_mem_zone::stname name of memory
 * fw_mem_zone::isize memory size
 */
mzone_handle nsfw_shmem_create (nsfw_mem_zone * pinfo);

/*
 *create some blocks memory
 */
i32 nsfw_shmem_createv (nsfw_mem_zone * pmeminfo, i32 inum,
                        mzone_handle * paddr_array, i32 iarray_num);

/*
 *lookup a memory
 */
mzone_handle nsfw_shmem_lookup (nsfw_mem_name * pname);

/*release the memory*/
i32 nsfw_shmem_release (nsfw_mem_name * pname);

/*
 *create mbuf pool
 */
mpool_handle nsfw_shmem_mbfmpcreate (nsfw_mem_mbfpool * pbufinfo);

/*
 *create some mbuf pool
 */
i32 nsfw_shmem_mbfmpcreatev (nsfw_mem_mbfpool * pmbfname, i32 inum,
                             mpool_handle * phandle_array, i32 iarray_num);

/*
 *alloc a mbuf from mbuf pool
 */
mbuf_handle nsfw_shmem_mbfalloc (mpool_handle mhandle);

/*
 *release a mbuf pool
 */
i32 nsfw_shmem_mbffree (mbuf_handle mhandle);

/*
 *put mbuf back to mbuf pool
 */
i32 nsfw_shmem_mbfmprelease (nsfw_mem_name * pname);

/*look up mbuf mpool*/
mpool_handle nsfw_shmem_mbfmplookup (nsfw_mem_name * pmbfname);

/*
 *create simple pool
 */
mring_handle nsfw_shmem_spcreate (nsfw_mem_sppool * pmpinfo);

/*
 *create some simple pools
 */
i32 nsfw_shmem_spcreatev (nsfw_mem_sppool * pmpinfo, i32 inum,
                          mring_handle * pringhandle_array, i32 iarray_num);

/*
 *create a simple pool that members are rings
 */
i32 nswf_shmem_sp_ringcreate (nsfw_mem_mring * prpoolinfo,
                              mring_handle * pringhandle_array, i32 iringnum);

/*
 *release a simple pool
 */
i32 nsfw_shmem_sprelease (nsfw_mem_name * pname);

/*
 *look up a simple pool
 */
mring_handle nsfw_shmem_sp_lookup (nsfw_mem_name * pname);

/*
 *create a ring with name
 */
mring_handle nsfw_shmem_ringcreate (nsfw_mem_mring * pringinfo);

/*
 *look up a ring with name
 */
mring_handle nsfw_shmem_ring_lookup (nsfw_mem_name * pname);

/*
 *release ring
 */
i32 nsfw_shmem_ringrelease (nsfw_mem_name * pname);

ssize_t nsfw_shmem_static (void *handle, nsfw_mem_struct_type type);

i32 nsfw_shmem_mbuf_recycle (mpool_handle handle);

i32 nsfw_shmem_sp_iterator (mpool_handle handle, u32 start, u32 end,
                            nsfw_mem_item_fun fun, void *argv);
i32 nsfw_shmem_mbuf_iterator (mpool_handle handle, u32 start, u32 end,
                              nsfw_mem_item_fun fun, void *argv);

#endif
