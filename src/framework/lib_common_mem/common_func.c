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

#include "common_mem_pal_memconfig.h"
#include "common_mem_mbuf.h"
#include "common_mem_common.h"
#include "nstack_log.h"
#include "common_pal_bitwide_adjust.h"

#include "common_func.h"

#include "nstack_securec.h"

#define COMMON_PROCESS_MAPS   "/proc/self/maps"

int g_PrimSameFlg = 1;

NSTACK_STATIC void **g_PrimAddr2LocalMap = NULL;
NSTACK_STATIC void *g_LocalBaseAddr = NULL;
NSTACK_STATIC void *g_LocalMaxAddr = NULL;
NSTACK_STATIC void *g_LocalCfgAddrBase = NULL;

NSTACK_STATIC uint64_t *g_LocalAddr2PrimMap = NULL;
NSTACK_STATIC uint64_t g_PrimBaseAddr = 0;
NSTACK_STATIC uint64_t g_PrimMaxAddr = 0;
NSTACK_STATIC uint64_t g_PrimCfgAddrBase = 0;

NSTACK_STATIC uint64_t g_LBitMask = 0;
NSTACK_STATIC int g_LBitMaskLen = 0;

uint64_t
pal_laddr_to_shddr (void *LAddr)
{
  size_t l2pIdx;

  if (g_PrimSameFlg || LAddr == NULL)
    {
      return (uint64_t) LAddr;
    }

  /*calculate the IDX */
  l2pIdx = (ALIGN_PTR (LAddr) - ALIGN_PTR (g_LocalBaseAddr)) >> g_LBitMaskLen;

  /*check the Hugepage Addr Rang */
  if (LAddr <= g_LocalMaxAddr && LAddr >= g_LocalBaseAddr
      && g_LocalAddr2PrimMap[l2pIdx])
    {
      return g_LocalAddr2PrimMap[l2pIdx] + (ALIGN_PTR (LAddr) & g_LBitMask);
    }

  /*check the Cfg Mapping Addr Rang */
  if (LAddr >= g_LocalCfgAddrBase
      && LAddr <=
      (void *) ((char *) g_LocalCfgAddrBase +
                sizeof (struct common_mem_mem_config)))
    {
      return g_PrimCfgAddrBase + ((char *) LAddr -
                                  (char *) g_LocalCfgAddrBase);
    }

  NSCOMM_LOGWAR
    ("WARNING!!! Input invalid LAddr]LAddr=%p, g_LocalBaseAddr=%p, g_LocalMaxAddr=%p, g_LocalCfgAddrBase=%p, g_LocalCfgAddrMax=%p.",
     LAddr, g_LocalBaseAddr, g_LocalMaxAddr, g_LocalCfgAddrBase,
     (char *) g_LocalCfgAddrBase + sizeof (struct common_mem_mem_config));

  return (uint64_t) LAddr;
}

void *
pal_shddr_to_laddr (uint64_t PAddr)
{
  size_t p2lIdx;

  if (g_PrimSameFlg || PAddr == ALIGN_PTR (NULL))
    {
      return (void *) PAddr;
    }

  p2lIdx = (PAddr - g_PrimBaseAddr) >> g_LBitMaskLen;
  /*check the Hugepage Addr Rang */
  if (PAddr <= g_PrimMaxAddr && PAddr >= g_PrimBaseAddr
      && g_PrimAddr2LocalMap[p2lIdx])
    {
      return (void *) ((uint64_t) g_PrimAddr2LocalMap[p2lIdx] +
                       (PAddr & g_LBitMask));
    }

  /*check the Cfg Mapping Addr Rang */
  if (PAddr >= g_PrimCfgAddrBase
      && PAddr <= g_PrimCfgAddrBase + sizeof (struct common_mem_mem_config))
    {
      return (void *) ((uint64_t) g_LocalCfgAddrBase + PAddr -
                       g_PrimCfgAddrBase);
    }

  NSCOMM_LOGWAR
    ("WARNING!!! Input invalid PAddr]PAddr=%lx, g_PrimBaseAddr=%lx, g_PrimMaxAddr=%lx, g_PrimCfgAddrBase=%lx, g_PrimCfgAddrMax=%lx.",
     PAddr, g_PrimBaseAddr, g_PrimMaxAddr, g_PrimCfgAddrBase,
     g_PrimCfgAddrBase + sizeof (struct common_mem_mem_config));

  return (void *) PAddr;
}

/*lint +e539 */

int
dmm_pal_addr_align ()
{
  return g_PrimSameFlg;
}

int32_t
dmm_pktmbuf_pool_iterator (struct common_mem_mempool * mp, uint32_t start,
                           uint32_t end, dmm_mbuf_item_fun fun, void *argv)
{
  if (NULL == mp || fun == NULL)
    {
      return 0;
    }

  if (start >= mp->size || end <= start)
    {
      return 0;
    }

  int32_t elm_size = mp->elt_size + mp->header_size + mp->trailer_size;
  struct common_mem_mbuf *elm_mbuf = (struct common_mem_mbuf *) (STAILQ_FIRST (&mp->mem_list)->addr + start * elm_size + mp->header_size);      /*lint !e647 */

  uint32_t i;
  uint32_t mbuf_end = COMMON_MEM_MIN (end, mp->size) - start;
  for (i = 0; i < mbuf_end; i++)
    {
      (void) fun (elm_mbuf, argv);
      elm_mbuf = (struct common_mem_mbuf *) ((char *) elm_mbuf + elm_size);
    }

  return mbuf_end;
}

void
dmm_addr_print (void)
{
  const struct common_mem_mem_config *mcfg =
    common_mem_pal_get_configuration ()->mem_config;
  int s;
  FILE *fd;
  char *ptembuf = NULL;
  if (!mcfg)
    {
      NSCOMM_LOGERR ("mcfg is null");
      return;
    }
  /*printf base address */
  NSCOMM_LOGINF ("********master baseaddr begin***************");
  for (s = 0; s < COMMON_MEM_MAX_MEMSEG; ++s)
    {
      if ((mcfg->memseg[s].len > 0) && (mcfg->memseg[s].addr != 0))
        {
          NSCOMM_LOGINF ("addr:%p, len:%u", mcfg->memseg[s].addr,
                         mcfg->memseg[s].len);
        }
    }
  NSCOMM_LOGINF ("********master baseaddr end***************");

  fd = fopen (COMMON_PROCESS_MAPS, "r");
  if (!fd)
    {
      NSCOMM_LOGERR ("/proc/self/maps open fail, erro:%d", errno);
      return;
    }

  ptembuf = (char *) malloc (BUFSIZ);
  if (!ptembuf)
    {
      NSCOMM_LOGERR ("malloc buff failed]buff_len=%d", BUFSIZ);
      fclose (fd);
      return;
    }
  if (EOK != MEMSET_S (ptembuf, BUFSIZ, 0, BUFSIZ))
    {
      NSCOMM_LOGERR ("MEMSET_S failed] buff=%p", ptembuf);
    }
  NSCOMM_LOGINF ("********self process addr space begin***************");
  while (fgets (ptembuf, BUFSIZ, fd) != NULL)
    {
      NSCOMM_LOGERR ("%s", ptembuf);
    }
  NSCOMM_LOGINF ("********self process addr space end*****************");
  fclose (fd);
  free (ptembuf);
  return;
}
