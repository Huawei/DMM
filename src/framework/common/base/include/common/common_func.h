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

#ifndef _RTE_COMM_FUNC_H_
#define _RTE_COMM_FUNC_H_

#ifdef HAL_LIB

#else

#define common_mem_rwlock_t rte_rwlock_t
#define common_mem_spinlock_t rte_spinlock_t
    //typedef rte_rwlock_t common_mem_rwlock_t;

#define nsfw_write_lock(plock)   rte_rwlock_write_lock(plock)
#define nsfw_write_unlock(plock) rte_rwlock_write_unlock(plock)
#define nsfw_read_lock(plock)    rte_rwlock_read_lock(plock)
#define nsfw_read_unlock(plock)  rte_rwlock_read_unlock(plock)

#define common_mem_align32pow2 rte_align32pow2

#define common_mem_atomic32_cmpset rte_atomic32_cmpset
#define common_mem_pause rte_pause

#define COMMON_MEM_MAX_MEMZONE RTE_MAX_MEMZONE

#define common_mem_atomic32_t rte_atomic32_t

#define common_mem_memseg rte_memseg
#define common_mem_mem_config rte_mem_config

#define common_mem_pal_get_configuration rte_eal_get_configuration

    //#define commem_mem_pal_module_info rte_eal_module_info
    //
#define common_mem_pal_init rte_eal_init

#define COMMON_MEM_MEMPOOL_NAMESIZE RTE_MEMPOOL_NAMESIZE

#define common_mem_memzone_lookup rte_memzone_lookup
#define common_mem_memzone rte_memzone
#define common_mem_atomic32_add_return rte_atomic32_add_return

#define common_mem_spinlock_init rte_spinlock_init
#define common_mem_spinlock_lock rte_spinlock_lock
#define common_mem_spinlock_unlock rte_spinlock_unlock

#define common_mem_memzone_free rte_memzone_free
#define common_mem_pktmbuf_pool_create rte_pktmbuf_pool_create

#define common_mem_pktmbuf_alloc rte_pktmbuf_alloc

#define common_mem_mempool rte_mempool

#define common_mem_pktmbuf_free rte_pktmbuf_free
#define common_mem_mbuf rte_mbuf

#define common_mem_mempool_lookup rte_mempool_lookup

#define common_mem_ring_get_memsize rte_ring_get_memsize
#define common_mem_ring rte_ring

#define COMMON_MEM_MAX_MEMSEG RTE_MAX_MEMSEG

#define common_mem_memzone_reserve rte_memzone_reserve
#define common_mem_rwlock_read_lock rte_rwlock_read_lock
#define common_mem_rwlock_read_unlock rte_rwlock_read_unlock

#define common_mem_rwlock_write_lock rte_rwlock_write_lock
#define common_mem_rwlock_write_unlock rte_rwlock_write_unlock
#define common_mem_spinlock_trylock rte_spinlock_trylock

#define common_mem_socket_id rte_socket_id
#define common_mem_malloc_socket_stats rte_malloc_socket_stats

#define COMMON_MEM_MIN RTE_MIN

#define  common_pal_module_init nscomm_pal_module_init
#define  common_memzone_data_reserve_name nscomm_memzone_data_reserve_name
#define  common_memzone_data_lookup_name nscomm_memzone_data_lookup_name

#define common_dump_stack  rte_dump_stack
#define COMMON_PKTMBUF_HEADROOM  RTE_PKTMBUF_HEADROOM

#define common_pktmbuf_mtod rte_pktmbuf_mtod
#define common_memcpy rte_memcpy
#define common_spinlock_try_lock_with_pid dmm_spinlock_try_lock_with_pid
#define common_spinlock_unlock rte_spinlock_unlock
#define common_atomic64_t rte_atomic64_t
#define common_atomic64_inc rte_atomic64_inc
#define common_atomic64_read rte_atomic64_read
#define common_atomic64_dec rte_atomic64_dec
#define common_mbuf_refcnt_set rte_mbuf_refcnt_set
#define common_mbuf_refcnt_read rte_mbuf_refcnt_read
#define common_exit rte_exit
#define COMMON_CACHE_LINE_SIZE RTE_CACHE_LINE_SIZE
#define common_eal_process_type rte_eal_process_type
#define COMMON_PROC_PRIMARY RTE_PROC_PRIMARY

#endif

#endif // _RTE_COMM_FUNC_H_
