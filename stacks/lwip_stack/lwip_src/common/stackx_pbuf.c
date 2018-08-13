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

#include "stackx_spl_share.h"
#include "stackx_pbuf.h"
#include "common_mem_mbuf.h"
#include "nstack_securec.h"
#include "nsfw_maintain_api.h"
#include "stackx_tcp_opt.h"
#ifdef HAL_LIB
#else
#include "rte_memcpy.h"
#endif

/*****************************************************************************
*   Prototype    : sbr_malloc_pbuf
*   Description  : malloc spl_pbuf,use it in app
*   Input        : mpool_handle mp
*                  u16 len
*                  u32 mbuf_data_size
*                  u16 offset
*   Output       : None
*   Return Value : struct pbuf*
*   Calls        :
*   Called By    :
*
*****************************************************************************/
struct spl_pbuf *
sbr_malloc_pbuf (mpool_handle mp, u16 len, u32 mbuf_data_size, u16 offset)
{
  if ((len < offset) || (len > mbuf_data_size))
    {
      NSSBR_LOGERR ("len is invalid]len=%u", len);
      return NULL;
    }

  struct common_mem_mbuf *mbuf = NULL;
  struct spl_pbuf *buf = NULL;

  mbuf = nsfw_mem_mbf_alloc (mp, NSFW_SHMEM);
  if (unlikely (mbuf == NULL))
    {
      NS_LOG_CTRL (LOG_CTRL_HUGEPAGE_ALLOC_FAIL, LOGSBR, "NSSBR", NSLOG_WAR,
                   "alloc mbuf failed");
      return NULL;
    }

  mbuf->data_len = len;
  mbuf->next = NULL;
  mbuf->nb_segs = 1;
  mbuf->pkt_len = len;
  buf = (struct spl_pbuf *) ((char *) mbuf + sizeof (struct common_mem_mbuf));
  res_alloc (&buf->res_chk);
  buf->next = 0;
  void *tmp = common_pktmbuf_mtod (mbuf, void *);
  buf->payload = (void *) (ADDR_LTOSH (tmp) + offset);
  buf->tot_len = len - offset;
  buf->len = len - offset;
  buf->type = SPL_PBUF_HUGE;
  buf->flags = 0;
  buf->freeNext = NULL;
  buf->conn_a = 0;
  buf->proto_type = SPL_PBUF_PROTO_NONE;
  NSSBR_LOGDBG ("malloc pbuf ok]buf=%p,PRIMARY_ADDR=%p", buf,
                ADDR_LTOSH (buf));
  return buf;
}

u32_t
spl_pbuf_copy_partial (struct spl_pbuf * buf, void *dataptr, u32_t len,
                       u32_t offset)
{
  struct spl_pbuf *p = buf;
  u32_t buf_copy_len;
  u32_t copied_total = 0;
  char *databuf = (char *) dataptr;

  /* Note some systems use byte copy if dataptr or one of the pbuf payload pointers are unaligned. */
  for (p = buf; len != 0 && p != NULL;
       p = PTR_SHTOL (struct spl_pbuf *, p->next_a))
    {
      if (offset != 0 && offset >= p->len)
        {
          /* don't copy from this buffer -> on to the next */
          offset -= p->len;
        }
      else if (p->len - offset > len)
        {
          /* copy from this buffer. maybe only partially. */
          (void) common_memcpy (databuf, ADDR_SHTOL (p->payload_a + offset),
                                len);
          copied_total += len;
          break;
        }
      else
        {
          buf_copy_len = p->len - offset;
          /* copy the necessary parts of the buffer */
          (void) common_memcpy (databuf, ADDR_SHTOL (p->payload_a + offset),
                                buf_copy_len);

          copied_total += buf_copy_len;
          databuf += buf_copy_len;
          len -= buf_copy_len;
          offset = 0;
        }
    }

  return copied_total;
}

int
spl_tx_force_buf_free (void *data)
{
  struct common_mem_mbuf *mbuf = data;
  if (NULL == mbuf)
    {
      return FALSE;
    }

  struct spl_pbuf *p_buf =
    (struct spl_pbuf *) (((char *) mbuf) + sizeof (struct common_mem_mbuf));
  u8 tempflag = p_buf->res_chk.u8Reserve;
  if ((mbuf->refcnt == 0) || (tempflag & DPDK_SEND_FLAG))
    {
      return FALSE;
    }

  NSFW_LOGINF ("free mbuf]%p", data);
  (void) nsfw_mem_mbf_free ((mbuf_handle) mbuf, NSFW_SHMEM);
  return TRUE;
}

int
spl_force_buf_free (void *data)
{
  struct common_mem_mbuf *mbuf = data;

  if (NULL == mbuf || (mbuf->refcnt == 0))
    {
      return FALSE;
    }

  NSFW_LOGINF ("free mbuf]%p", data);
  (void) nsfw_mem_mbf_free ((mbuf_handle) mbuf, NSFW_SHMEM);
  return TRUE;
}

int
spl_reg_res_tx_mgr (mpool_handle * pool)
{
  struct common_mem_mempool *mp = (struct common_mem_mempool *) pool;
  if (NULL == mp)
    {
      return 0;
    }

  nsfw_res_scn_cfg scn_cfg = { NSFW_RES_SCAN_MBUF, 60, 3, 16,
    mp->size / 128, mp->size,
    mp->elt_size,
    sizeof (struct common_mem_mbuf) + offsetof (struct spl_pbuf, res_chk),
    mp,
    mp->pool_data,
    spl_tx_force_buf_free
  };
  (void) nsfw_res_mgr_reg (&scn_cfg);
  return 0;
}

int
spl_reg_res_rxmt_mgr (mpool_handle * pool)
{
  struct common_mem_mempool *mp = (struct common_mem_mempool *) pool;
  if (NULL == mp)
    {
      return 0;
    }

  nsfw_res_scn_cfg scn_cfg = { NSFW_RES_SCAN_MBUF, 60, 3, 16,
    mp->size / 128, mp->size,
    mp->elt_size,
    sizeof (struct common_mem_mbuf) + offsetof (struct spl_pbuf, res_chk),
    mp,
    mp->pool_data,
    spl_tx_force_buf_free
  };                            /*Can use same function for time */
  (void) nsfw_res_mgr_reg (&scn_cfg);
  return 0;
}

int
spl_reg_res_txrx_mgr (mpool_handle * pool)
{
  struct common_mem_mempool *mp = (struct common_mem_mempool *) pool;
  if (NULL == mp)
    {
      return 0;
    }

  nsfw_res_scn_cfg scn_cfg = { NSFW_RES_SCAN_MBUF, 60, 3, 16,
    mp->size / 128, mp->size,
    mp->elt_size,
    sizeof (struct common_mem_mbuf) + offsetof (struct spl_pbuf, res_chk),
    mp,
    mp->pool_data,
    spl_force_buf_free
  };
  (void) nsfw_res_mgr_reg (&scn_cfg);
  return 0;
}
