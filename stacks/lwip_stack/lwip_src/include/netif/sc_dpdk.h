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

#ifndef SC_DPDK_H_
#define SC_DPDK_H_

#include <pbuf.h>
#include <sharedmemory.h>

#include "lwip/etharp.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define NSTACK_CONFIG_SHM "nstack_config"

#define MBUF_DATA_SIZE  2048
#define MBUF_SIZE (MBUF_DATA_SIZE + sizeof(struct common_mem_mbuf) + COMMON_PKTMBUF_HEADROOM)

extern int g_nstack_bind_cpu;

inline uint16_t spl_mbuf_refcnt_update (void *mbuf, int16_t value);
struct stack_proc_content *get_stack_proc_content ();

inline int spl_msg_malloc (data_com_msg ** p_msg_entry);
#define spl_msg_free(p_msg_entry) msg_free(p_msg_entry)
struct spl_pbuf *spl_mbuf_malloc (uint16_t len, spl_pbuf_type type,
                                  u16_t * count);

inline void spl_mbuf_free (void *mbuf);

inline int spl_set_lcore_id (unsigned dest_lcore_id);

static inline unsigned
spl_get_lcore_id ()
{
  unsigned core_id = 0;

#if (DPDK_MODULE != 1)
#ifdef HAL_LIB
#else
  core_id = rte_lcore_id ();
#endif
  if (core_id >= MAX_THREAD_NUM)
    {
      core_id = 0;
    }
#endif

  return core_id;
}

int set_share_config ();

int init_instance ();

void printmeminfo ();

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#endif /* SERVER_DPDK_H_ */
