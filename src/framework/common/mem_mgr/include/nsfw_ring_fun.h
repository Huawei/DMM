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

#ifndef _NSFW_RING_FUN_H_
#define _NSFW_RING_FUN_H_

#include <stdint.h>
#include "common_pal_bitwide_adjust.h"
#include "nsfw_mem_api.h"
#include "nsfw_ring_data.h"

/*
   for nstack I advise addrbase set to lowest of mmaped hugepage Addr.
   to simple:
   1. ring element is from mmaped mem, set Addrbase to 0x7fffffffffff - 0xffffffffff is OK;
   1. ring element is from heap, set Addrbase to NULL is ok;
*/
static inline void
nsfw_mem_ring_init (struct nsfw_mem_ring *ring, unsigned int size,
                    void *addrbase, unsigned char memtype, unsigned char flag)
{
  unsigned int loop = 0;

  if (!ring)
    {
      return;
    }

  ring->prod.head = 0;
  ring->prod.tail = 0;
  ring->cons.head = 0;
  ring->cons.tail = 0;
  ring->size = size;
  ring->eltsize = 0;
  ring->mask = size - 1;
  ring->memtype = memtype;
  ring->ringflag = flag;
  ring->prodtflag = ring->prodhflag = get_sys_pid ();
  ring->conshflag = ring->constflag = get_sys_pid ();
  /*if shmem, addrbase already changed to primary memory address */
  ring->Addrbase = addrbase;
  ring->uireserv[0] = 0;
  ring->uireserv[1] = 0;
  ring->uireserv[2] = 0;
  ring->uireserv[3] = 0;

  /*init Ring */
  for (loop = 0; loop < size; loop++)
    {
      /*
         for a empty ring, version is the mapping head val - size
         so the empty ring's ver is loop-size;
       */
      ring->ring[loop].data_s.ver = (loop - size);
      ring->ring[loop].data_s.val = 0;
    }
}

/*
another way to init Pool while no continuous space
1. init a empty rte_perf_ring
2. add element to PerRing.
*/
static inline void
nsfw_mem_pool_head_init (struct nsfw_mem_ring *ring, unsigned int size,
                         unsigned int eltsize, void *addrbase,
                         nsfw_mem_type memtype, nsfw_mpool_type flag)
{
  ring->prod.head = size;
  ring->prod.tail = size;
  ring->cons.head = 0;
  ring->cons.tail = 0;
  ring->size = size;
  ring->eltsize = eltsize;
  ring->mask = size - 1;
  ring->memtype = memtype;
  ring->ringflag = flag;
  ring->prodtflag = ring->prodhflag = get_sys_pid ();
  ring->conshflag = ring->constflag = get_sys_pid ();
  /*if shmem, addrbase already changed to primary memory address */
  ring->Addrbase = addrbase;
  ring->uireserv[0] = 0;
  ring->uireserv[1] = 0;
  ring->uireserv[2] = 0;
  ring->uireserv[3] = 0;
  return;
}

#define NSFW_RING_FLAG_CHECK_RET(handle, desc) {\
        if (((struct  nsfw_mem_ring*)mhandle)->ringflag >= NSFW_MPOOL_TYPEMAX) \
        { \
            NSCOMM_LOGERR("invalid ring] desc=%s, ringflag=%d", desc, ((struct  nsfw_mem_ring*)mhandle)->ringflag); \
            return 0; \
        } \
    }

#endif /*_NSFW_RING_FUN_H_*/
