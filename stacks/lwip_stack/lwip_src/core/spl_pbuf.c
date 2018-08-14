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

#include "spl_opt.h"

#include "spl_def.h"
#include "spl_pbuf.h"
#include "stackx_pbuf.h"

#include "stackx_spl_share.h"
#include "spl_api.h"

#include "nsfw_maintain_api.h"
#include "netif/sc_dpdk.h"
#include <common_mem_mbuf.h>

#include <string.h>
#include "nstack_log.h"
#include "nstack_securec.h"
#include "spl_instance.h"

#include "sys.h"
#include "mem.h"
#include "memp.h"
//#include "sockets.h"
//#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAL_LIB
#else
#include "rte_memcpy.h"
#endif
#define SIZEOF_STRUCT_PBUF LWIP_MEM_ALIGN_SIZE(sizeof(struct spl_pbuf))
u16_t g_offSetArry[SPL_PBUF_MAX_LAYER];

u16_t g_offSetArry[SPL_PBUF_MAX_LAYER] =
  { PBUF_TRANSPORT_HLEN + PBUF_IP_HLEN + SPL_PBUF_LINK_HLEN,
  SPL_PBUF_UDP_HLEN + PBUF_IP_HLEN + SPL_PBUF_LINK_HLEN,
  PBUF_IP_HLEN + SPL_PBUF_LINK_HLEN,
  SPL_PBUF_LINK_HLEN,
  0
};

inline struct spl_pbuf *
spl_pbuf_alloc_hugepage (spl_pbuf_layer layer, u16_t length,
                         spl_pbuf_type Type, u16_t proc_id, void *net_conn)
{
  struct spl_pbuf *p;
  u16_t offset;
  u16_t count = 0;
  spl_netconn_t *conn = (spl_netconn_t *) net_conn;

  NSPOL_LOGDBG (PBUF_DEBUG | STACKX_DBG_TRACE,
                "spl_pbuf_alloc_hugepage]length=%" U16_F, length);

  /* determine header offset */
  if (likely (layer < SPL_PBUF_MAX_LAYER))
    {
      offset = g_offSetArry[layer];
    }
  else
    {
      NSPOL_LOGERR ("spl_pbuf_alloc_hugepage: bad pbuf layer");
      return NULL;
    }

  if (unlikely (length < offset || Type != SPL_PBUF_HUGE))
    {
      NSPOL_LOGERR ("input is invalid!]length=%u, Type = %d", length, Type);
      return NULL;
    }

  p = spl_mbuf_malloc (length, SPL_PBUF_HUGE, &count);

  if (p == NULL)
    {
      /* last_log_prt_time and unprint_log_count indeed have multi-thread issue,
       * but their values don't have precision requirement. No risk. */
      NS_LOG_CTRL (LOG_CTRL_HUGEPAGE_ALLOC_FAIL, STACKX, "NSPOL", NSLOG_ERR,
                   "pbuf_alloc_huge: Could not allocate PBUF for SPL_PBUF_HUGE");

      return NULL;
    }

  if (conn)
    {
      p->conn_a = ADDR_LTOSH (conn);
    }
  else
    {
      p->conn_a = 0;
    }

  p->tot_len -= offset;
  p->len -= offset;
  p->next_a = 0;

  p->payload_a = p->payload_a + offset;

  p->proto_type = SPL_PBUF_PROTO_NONE;

  NSPOL_LOGDBG (PBUF_DEBUG | STACKX_DBG_TRACE,
                "pbuf_alloc]length=%" U16_F ",p=%p", length, (void *) p);
  return p;
}

inline int
spl_pbuf_internal_copy (struct spl_pbuf *dst, struct spl_pbuf *src)
{
  u32_t dst_len = dst->len;
  u32_t src_len = src->len;
  u32_t dst_pos = 0;
  u32_t src_pos = 0;
  while (dst != NULL && src != NULL)
    {
      if (dst_len > src_len)
        {
          if (NULL ==
              common_memcpy (PTR_SHTOL (char *, dst->payload_a) + dst_pos,
                             PTR_SHTOL (char *, src->payload_a) + src_pos,
                             src_len))
            {
              NSPOL_LOGERR ("rte_memcpy error");
              return -1;
            }

          dst_len -= src_len;
          dst_pos += src_len;
          src = ADDR_SHTOL (src->next_a);
          src_len = src != NULL ? src->len : 0;
          src_pos = 0;
        }
      else if (dst_len < src_len)
        {
          if (NULL ==
              common_memcpy (PTR_SHTOL (char *, dst->payload_a) + dst_pos,
                             PTR_SHTOL (char *, src->payload_a) + src_pos,
                             dst_len))
            {
              NSPOL_LOGERR ("rte_memcpy error");
              return -1;
            }

          src_len -= dst_len;
          src_pos += dst_len;
          dst = ADDR_SHTOL (dst->next_a);
          dst_len = dst != NULL ? dst->len : 0;
          dst_pos = 0;
        }
      else
        {
          if (NULL ==
              common_memcpy (PTR_SHTOL (char *, dst->payload_a) + dst_pos,
                             PTR_SHTOL (char *, src->payload_a) + src_pos,
                             dst_len))
            {
              NSPOL_LOGERR ("rte_memcpy error");
              return -1;
            }

          src = ADDR_SHTOL (src->next_a);
          src_len = src != NULL ? src->len : 0;
          src_pos = 0;
          dst = ADDR_SHTOL (dst->next_a);
          dst_len = dst != NULL ? dst->len : 0;
          dst_pos = 0;
        }
    }
  return 0;
}

void
spl_pbuf_realloc (struct spl_pbuf *p, u32_t new_len)
{
  if (NULL != p && p->type == SPL_PBUF_HUGE)
    {
      p->tot_len = new_len;
      p->len = new_len;
      return;
    }
}

void
spl_pbuf_free (struct spl_pbuf *p)
{
  struct spl_pbuf *q;
  u8_t count = 0;

  NSPOL_LOGINF (PBUF_DEBUG, "Freeing PBF %p", p);

  if (unlikely (p == NULL || p->type != SPL_PBUF_HUGE))
    {
      NSPOL_LOGDBG (PBUF_DEBUG | STACKX_DBG_LEVEL_SERIOUS,
                    "input param illegal]p=%p, type=%d", p, p ? p->type : -1);
      return;
    }

  (void) count;

  /* de-allocate all consecutive pbufs from the head of the chain that
   * obtain a zero reference count after decrementing*/
  do
    {
      /* remember next pbuf in chain for next iteration */
      NSPOL_LOGDBG (PBUF_DEBUG | STACKX_DBG_TRACE,
                    "spl_pbuf_free: deallocating]buf=%p", (void *) p);
      q = (struct spl_pbuf *) ADDR_SHTOL (p->next_a);
      if (res_free (&p->res_chk))
        {
          //NSPOL_LOGERR("res_free failed]p=%p", p);
        }

      count++;
      {
        pbuf_set_recycle_flg (p, MBUF_UNUSED);  /* release buf hold by app on abnormal exit */
        spl_mbuf_free ((char *) p - sizeof (struct common_mem_mbuf));
        p->res_chk.u8Reserve |= PBUF_FREE_FLAG;
      }

      /* proceed to next pbuf */
      p = q;
    }
  while (p != NULL);

  /* return number of de-allocated pbufs */
  return;
}

void
spl_pbuf_cat (struct spl_pbuf *h, struct spl_pbuf *t)
{
  struct spl_pbuf *p;

  if (h == NULL || t == NULL)
    {
      NSPOL_LOGERR ("pbuf_cat: h=NULL||t=NULL!!");
      return;
    }

  for (p = h; p->next_a != 0; p = PTR_SHTOL (struct spl_pbuf *, p->next_a))
    {
      /* add total length of second chain to all totals of first chain */
      p->tot_len += t->tot_len;
    }

  if (0 != p->next_a)
    {
      NSPOL_LOGERR
        ("pbuf_cat: p is not the last pbuf of the first chain, p->next_a != 0");
      return;
    }

  /* add total length of second chain to last pbuf total of first chain */
  p->tot_len += t->tot_len;

  /* chain last pbuf of head (p) with first of tail (t) */
  p->next_a = (uint64_t) ADDR_LTOSH_EXT (t);

  /* p->next now references t, but the caller will drop its reference to t,
   * so netto there is no change to the reference count of t.
   */
}

err_t
spl_pbuf_copy (struct spl_pbuf * p_to, struct spl_pbuf * p_from)
{
  u32_t offset_to = 0;
  u32_t offset_from = 0;
  u32_t len = 0;
  NSPOL_LOGDBG (PBUF_DEBUG | STACKX_DBG_TRACE, "pbuf_copy]p_to=%p,p_from=%p",
                (void *) p_to, (void *) p_from);

  if (!
      ((p_to != NULL) && (p_from != NULL)
       && (p_to->tot_len >= p_from->tot_len)))
    {
      NSPOL_LOGERR ("pbuf_copy: target not big enough to hold source");
      return ERR_ARG;
    }

  do
    {
      if (p_to == NULL)
        {
          NSPOL_LOGERR ("pbuf_copy: target pbuf not exist: p_to == NULL!!");
          return ERR_ARG;
        }

      if ((p_to->len - offset_to) >= (p_from->len - offset_from))
        {

          len = p_from->len - offset_from;
        }
      else
        {

          len = p_to->len - offset_to;
        }

      if (EOK !=
          MEMMOVE_S ((u8_t *) ADDR_SHTOL (p_to->payload_a) + offset_to, len,
                     (u8_t *) ADDR_SHTOL (p_from->payload_a) + offset_from,
                     len))
        {
          NSPOL_LOGERR ("MEMMOVE_S failed");
          return ERR_MEM;
        }

      offset_to += len;
      offset_from += len;
      if (offset_to > p_to->len)
        {
          NSPOL_LOGERR
            ("pbuf_copy: target offset not match: offset_to > p_to->len.");
          return ERR_VAL;
        }
      if (offset_to == p_to->len)
        {
          offset_to = 0;
          p_to = (struct spl_pbuf *) ADDR_SHTOL (p_to->next_a);
        }

      if (offset_from >= p_from->len)
        {
          /* on to next p_from (if any) */
          NSPOL_LOGDBG (PBUF_DEBUG | STACKX_DBG_TRACE,
                        "pbuf_copy: source offset not match: offset_from >= p_from->len");
          offset_from = 0;
          p_from = (struct spl_pbuf *) ADDR_SHTOL (p_from->next_a);
        }

      if ((p_from != NULL) && (p_from->len == p_from->tot_len))
        {
          if ((p_from->next_a != 0))
            {
              NSPOL_LOGERR ("pbuf_copy() does not allow packet queues!");
              return ERR_VAL;
            }
        }

      if ((p_to != NULL) && (p_to->len == p_to->tot_len))
        {
          /* don't copy more than one packet! */
          if ((p_to->next_a != 0))
            {
              NSPOL_LOGERR ("pbuf_copy() does not allow packet queues!");
              return ERR_VAL;
            }
        }
    }
  while (p_from);

  NSPOL_LOGDBG (PBUF_DEBUG | STACKX_DBG_TRACE,
                "pbuf_copy: end of chain reached.");
  return ERR_OK;
}

err_t
splpbuf_to_pbuf_copy (struct pbuf * p_to, struct spl_pbuf * p_from)
{
  u32_t offset_to = 0;
  u32_t offset_from = 0;
  u32_t len = 0;

  NSPOL_LOGINF (NETIF_DEBUG, "splpbuf_to_pbuf_copy");

  if (!
      ((p_to != NULL) && (p_from != NULL)
       && (p_to->tot_len >= p_from->tot_len)))
    {
      NSPOL_LOGERR
        ("splpbuf_to_pbuf_copy: target not big enough to hold source");
      return ERR_ARG;
    }
  NSPOL_LOGINF (PBUF_DEBUG,
                "splpbuf_to_pbuf_copy]p_to=%p [type %d tot_len %d], p_from=%p [type %d tot_len %d]",
                (void *) p_to, p_to->type, p_to->tot_len, (void *) p_from,
                p_from->type, p_from->tot_len);
  do
    {
      NSPOL_LOGINF (NETIF_DEBUG, "copying....");
      if (p_to == NULL)
        {
          NSPOL_LOGERR
            ("splpbuf_to_pbuf_copy: target not big enough to hold source p_to len [%d] p_from len [%d]",
             p_to->tot_len, p_from->tot_len);
          return ERR_ARG;
        }

      if ((p_to->len - offset_to) >= (p_from->len - offset_from))
        {

          len = p_from->len - offset_from;
        }
      else
        {

          len = p_to->len - offset_to;
        }

      if (EOK != MEMMOVE_S ((u8_t *) p_to->payload + offset_to,
                            len,
                            (u8_t *) ADDR_SHTOL (p_from->payload_a) +
                            offset_from, len))
        {
          NSPOL_LOGERR ("MEMMOVE_S failed");
          return ERR_MEM;
        }

      offset_to += len;
      offset_from += len;
      if (offset_to > p_to->len)
        {
          NSPOL_LOGERR
            ("splpbuf_to_pbuf_copy: target offset not match: offset_to > p_to->len.");
          return ERR_VAL;
        }
      if (offset_to == p_to->len)
        {
          offset_to = 0;
          p_to = p_to->next;

          NSPOL_LOGINF (NETIF_DEBUG,
                        "splpbuf_to_pbuf_copy: p_to next %p", p_to);
        }

      if (offset_from >= p_from->len)
        {
          /* on to next p_from (if any) */
          NSPOL_LOGINF (NETIF_DEBUG,
                        "splpbuf_to_pbuf_copy: source offset not match: offset_from >= p_from->len");
          offset_from = 0;
          p_from = (struct spl_pbuf *) ADDR_SHTOL (p_from->next_a);
          NSPOL_LOGINF (NETIF_DEBUG,
                        "splpbuf_to_pbuf_copy: pfrom next %p", p_from);
        }

      if ((p_from != NULL) && (p_from->len == p_from->tot_len))
        {
          if ((p_from->next_a != 0))
            {
              NSPOL_LOGERR
                ("splpbuf_to_pbuf_copy() does not allow packet queues!");
              return ERR_VAL;
            }
        }

      if ((p_to != NULL) && (p_to->len == p_to->tot_len))
        {
          /* don't copy more than one packet! */
          if ((p_to->next != NULL))
            {
              NSPOL_LOGERR
                ("splpbuf_to_pbuf_copy() does not allow packet queues!");
              return ERR_VAL;
            }
        }
    }
  while (p_from);

  NSPOL_LOGDBG (NETIF_DEBUG, "splpbuf_to_pbuf_copy: end of chain reached.");
  return ERR_OK;
}

err_t
pbuf_to_splpbuf_copy (struct spl_pbuf * p_to, struct pbuf * p_from)
{
  int offset = 0;
  void *data = NULL;

  do
    {
      data = (u8_t *) (p_to->payload_a) + offset;
      memcpy (data, (u8_t *) p_from->payload, p_from->len);
      offset = offset + p_from->len;
      if (offset >= 2048)
        {
          NSPOL_LOGERR ("More thank 2K size");
          return ERR_MEM;
        }
      p_from = p_from->next;
    }
  while (p_from != NULL);

  return ERR_OK;
}

struct pbuf *
spl_convert_spl_pbuf_to_pbuf (struct spl_pbuf *p_from)
{
  struct pbuf *p_to = NULL;

  p_to = pbuf_alloc (PBUF_RAW, p_from->tot_len, PBUF_POOL);
  if (p_to)
    {
      splpbuf_to_pbuf_copy (p_to, p_from);
    }
  return p_to;
}

spl_pbuf_layer
get_pbuf_layer_from_pbuf_payload (struct pbuf * buf)
{

  struct eth_hdr *ethhdr;
  spl_pbuf_layer layer = SPL_PBUF_TRANSPORT;
  u16_t type;

  if (buf->len <= SIZEOF_ETH_HDR)
    {
      NSPOL_LOGINF (PBUF_DEBUG,
                    "get_pbuf_layer_from_payload failed. length is wrong");
      return layer;
    }
  ethhdr = (struct eth_hdr *) buf->payload;
  type = spl_htons (ethhdr->type);

  NSPOL_LOGINF (PBUF_DEBUG, "packet type %x", type);

  switch (type)
    {
    case ETHTYPE_IP:
      layer = SPL_PBUF_IP;
      break;
    case ETHTYPE_ARP:
      layer = SPL_PBUF_LINK;
      break;
    default:
      layer = SPL_PBUF_TRANSPORT;
      break;
    }

  return layer;
}

void
print_pbuf_payload_info (struct pbuf *buf, bool send)
{

  struct eth_hdr *ethhdr;
  u16_t type;

  if (buf->len <= SIZEOF_ETH_HDR)
    {
      NSPOL_LOGINF (PBUF_DEBUG,
                    "get_pbuf_layer_from_payload failed. length is wrong");
      return;
    }
  ethhdr = (struct eth_hdr *) buf->payload;
  type = spl_htons (ethhdr->type);

  if (send)
    {
      NSPOL_LOGINF (PBUF_DEBUG, "send packet start type %x len %d *****",
                    type, buf->len);
    }
  else
    {
      NSPOL_LOGINF (PBUF_DEBUG,
                    "receive packet start type %x len %d ########", type,
                    buf->len);
    }

  switch (type)
    {
    case ETHTYPE_IP:
      NSPOL_LOGINF (TCPIP_DEBUG, "ip packet len %d tot len %d type %x",
                    buf->len, buf->tot_len, type);
      struct ip_hdr *ip =
        (struct ip_hdr *) ((char *) buf->payload + SIZEOF_ETH_HDR);
      NSPOL_LOGINF (PBUF_DEBUG, "ip packet src %x dest %x proto %d", ip->src,
                    ip->dest, ip->_proto);
      break;
    case ETHTYPE_ARP:
      NSPOL_LOGINF (TCPIP_DEBUG, "arp packet len %d tot len %d type %x",
                    buf->len, buf->tot_len, type);
      break;
    default:
      NSPOL_LOGINF (TCPIP_DEBUG, "other packet len %d tot len %d type %x",
                    buf->len, buf->tot_len, type);
      break;
    }
  return;
}
