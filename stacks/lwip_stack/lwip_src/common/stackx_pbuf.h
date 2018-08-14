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

#ifndef STACKX_PBUF_H
#define STACKX_PBUF_H

#include "common_mem_base_type.h"
#include "common_mem_mbuf.h"
#include "nsfw_mem_api.h"
#include "stackx_pbuf_comm.h"
#ifdef HAL_LIB
#else
#include "common_pal_bitwide_adjust.h"
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define NEED_ACK_FLAG   0x01    /* This spl_pbuf may have multiple references. */
#define PBUF_FREE_FLAG  0x04    /* This spl_pbuf has been free. */
#define DPDK_SEND_FLAG  0x10    /* This spl_pbuf has been sent to DPDK. */
#define LOOP_SEND_FLAG  0x40    /* This spl_pbuf has been looped to IP layer and not received by app layer yet. */

struct spl_pbuf *sbr_malloc_pbuf (mpool_handle mp, u16 len,
                                  u32 mbuf_data_size, u16 offset);
void sbr_free_pbuf (struct spl_pbuf *p);
u32 spl_pbuf_copy_partial (struct spl_pbuf *p, void *dataptr, u32_t len,
                           u32_t offset);
int spl_reg_res_txrx_mgr (mpool_handle * pool);
int spl_reg_res_tx_mgr (mpool_handle * pool);

/* release buf hold by app on abnormal exit */
/*
 *For TX mbuf: recycle_flg can be: MBUF_UNUSED, MBUF_HLD_BY_APP, MBUF_HLD_BY_SPL.
 *For TX mbuf: recycle_flg can be: MBUF_UNSUED, app pid.
 */
static inline void
pbuf_set_recycle_flg (struct spl_pbuf *p, uint32_t flg)
{
  uint32_t *recycle_flg;
  struct spl_pbuf *q = p;
  struct common_mem_mbuf *m;

  while (q != NULL)
    {
      m =
        (struct common_mem_mbuf *) ((char *) q -
                                    sizeof (struct common_mem_mbuf));
#ifdef HAL_LIB
#else
      recycle_flg =
        (uint32_t *) ((char *) (m->buf_addr) + RTE_PKTMBUF_HEADROOM -
                      sizeof (uint32_t));
#endif
      *recycle_flg = flg;
      q = (struct spl_pbuf *) ADDR_SHTOL (q->next_a);
    }
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
