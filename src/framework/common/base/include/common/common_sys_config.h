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

#ifndef __COMMON_SYS_CONFIG_H_
#define __COMMON_SYS_CONFIG_H_

/* Below compile macro is used only in UT makefile */
#if (HAL_LIB)
#else
#undef RTE_CACHE_LINE_SIZE
#define RTE_CACHE_LINE_SIZE 64  /* RTE_CACHE_LINE_SIZE */
#undef RTE_MAX_LCORE
#define RTE_MAX_LCORE 128       /* RTE_MAX_LCORE */
#undef RTE_MAX_NUMA_NODES
#define RTE_MAX_NUMA_NODES 8    /* RTE_MAX_NUMA_NODES */
#undef RTE_MAX_MEMSEG
#define RTE_MAX_MEMSEG 256      /* RTE_MAX_MEMSEG */
#undef RTE_MAX_MEMZONE
#define RTE_MAX_MEMZONE 2560    /* RTE_MAX_MEMZONE */
#undef RTE_MAX_TAILQ
#define RTE_MAX_TAILQ 32        /* RTE_MAX_TAILQ */
#undef RTE_ARCH_X86
#define RTE_ARCH_X86 1          /* RTE_ARCH_64 */
#undef RTE_ARCH_64
#define RTE_ARCH_64 1           /* RTE_ARCH_64 */
#undef RTE_PKTMBUF_HEADROOM
#define RTE_PKTMBUF_HEADROOM 128        /* RTE_PKTMBUF_HEADROOM */
#undef RTE_MEMPOOL_CACHE_MAX_SIZE
#define RTE_MEMPOOL_CACHE_MAX_SIZE 512  /* RTE_MEMPOOL_CACHE_MAX_SIZE */

#endif

#endif // __COMMON_SYS_CONFIG_H_
