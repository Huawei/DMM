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

#ifndef __STACKX_PBUF_H__
#define __STACKX_PBUF_H__

#include "cc.h"

#include "common_mem_base_type.h"
#include "stackx_pbuf_comm.h"
#include "common_mem_mbuf.h"

#ifdef HAL_LIB
#else
#include "common_pal_bitwide_adjust.h"
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

extern u16_t g_offSetArry[SPL_PBUF_MAX_LAYER];

/** indicates this pbuf is loop buf .*/
#define PBUF_FLAG_LOOPBUF 0x08U

#define PBUF_SET_LOOP_FLAG(buf) ((buf)->flags |= PBUF_FLAG_LOOPBUF)
#define PBUF_IS_LOOP_BUF(buf) ((buf)->flags & PBUF_FLAG_LOOPBUF)

/*Add 1 parameter, the last one, indicating which dpdk_malloc
 *should be allocated from the pool; non-PBUF_ALLOC type regardless of this parameter*/
struct spl_pbuf *spl_pbuf_alloc_hugepage (spl_pbuf_layer l, u16_t length,
                                          spl_pbuf_type type,
                                          u16_t thread_index, void *net_conn);
struct pbuf *spl_convert_spl_pbuf_to_pbuf (struct spl_pbuf *p_from);
err_t
splpbuf_to_pbuf_transport_copy (struct pbuf *p_to, struct spl_pbuf *p_from);

err_t pbuf_to_splpbuf_copy (struct spl_pbuf *p_to, struct pbuf *p_from);
err_t splpbuf_to_pbuf_copy (struct pbuf *p_to, struct spl_pbuf *p_from);
spl_pbuf_layer get_pbuf_layer_from_pbuf_payload (struct pbuf *buf);
void print_pbuf_payload_info (struct pbuf *buf, bool send);

static inline u8_t
spl_pbuf_header (struct spl_pbuf *p, s16_t header_size_increment)
{
  u8_t pbuf_ret;
  /* header max len is tcp len+ ip len:  0xf*4+PBUF_IP_HLEN */
  if (unlikely
      (((header_size_increment) < 0 && (-(header_size_increment)) > p->len)
       || (header_size_increment > 0
           && (header_size_increment) >= (0xf * 4 + SPL_PBUF_IP_HLEN))))
    {
      pbuf_ret = 1;
    }
  else
    {
      p->payload_a = p->payload_a - (header_size_increment);
      p->len += (header_size_increment);
      p->tot_len += (header_size_increment);
      pbuf_ret = 0;
    }
  return pbuf_ret;
}

void spl_pbuf_realloc (struct spl_pbuf *p, u32_t size);

void spl_pbuf_cat (struct spl_pbuf *head, struct spl_pbuf *tail);
void spl_pbuf_free (struct spl_pbuf *p);

#define pbuf_free_safe(x)\
{\
    spl_pbuf_free((x));\
    (x) = NULL;\
}

/**
 * Count number of pbufs in a chain
 *
 * @param p first pbuf of chain
 * @return the number of pbufs in a chain
 */
static inline u16_t
spl_pbuf_clen (struct spl_pbuf *p)
{
  u16_t len = 0;
  while (p != NULL)
    {
      ++len;
      p = (struct spl_pbuf *) ADDR_SHTOL (p->next_a);
    }
  return len;
}

static inline u16_t
mbuf_count (struct spl_pbuf *p)
{
  u16_t count = 0;
  struct spl_pbuf *buf = p;
  struct common_mem_mbuf *mbuf;
  while (buf)
    {
      mbuf =
        (struct common_mem_mbuf *) ((char *) buf -
                                    sizeof (struct common_mem_mbuf));
      while (mbuf)
        {
          count++;
#ifdef HAL_LIB
#else
          mbuf = mbuf->next;
#endif
        }
      buf = (struct spl_pbuf *) ADDR_SHTOL (buf->next_a);       //buf->next;
    }
  return count;
}

static inline u16_t
mbuf_count_in_one_pbuf (struct spl_pbuf *p)
{

  u16_t cnt = 0;
  struct common_mem_mbuf *mbuf;
  if (NULL == p)
    {
      NSPOL_LOGERR ("Invalid param : p(null) !");
      return 0;
    }

  mbuf =
    (struct common_mem_mbuf *) ((char *) p - sizeof (struct common_mem_mbuf));

  /* no need to check mbuf itself */
#ifdef HAL_LIB
#else
  if (!mbuf->next)
#endif
    return 1;

  while (mbuf)
    {
      ++cnt;
#ifdef HAL_LIB
#else
      mbuf = mbuf->next;
#endif
    }

  return cnt;
}

inline int pbuf_internal_copy (struct spl_pbuf *dst, struct spl_pbuf *src);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __STACKX_PBUF_H__ */
