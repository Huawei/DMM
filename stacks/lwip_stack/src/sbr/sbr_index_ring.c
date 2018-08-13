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

#include <string.h>
#include "sbr_index_ring.h"
#include "nstack_securec.h"
#include "common_mem_common.h"
#include "common_func.h"

/*****************************************************************************
*   Prototype    : sbr_init_index_ring
*   Description  : init index ring
*   Input        : sbr_index_ring* ring
*                  u32 num
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
sbr_init_index_ring (sbr_index_ring * ring, u32 num)
{
  u32 loop;

  ring->head = 0;
  ring->tail = 0;
  ring->num = num;
  ring->mask = num - 1;

  for (loop = 0; loop < num; loop++)
    {
      ring->nodes[loop].ver = (loop - num);
      ring->nodes[loop].val = 0;
    }
}

/*****************************************************************************
*   Prototype    : sbr_create_index_ring
*   Description  : create index ring
*   Input        : u32 num
*   Output       : None
*   Return Value : sbr_index_ring*
*   Calls        :
*   Called By    :
*
*****************************************************************************/
sbr_index_ring *
sbr_create_index_ring (u32 num)
{
  num = common_mem_align32pow2 (num + 1);
  sbr_index_ring *ring =
    (sbr_index_ring *) malloc (sizeof (sbr_index_ring) +
                               num * sizeof (sbr_index_node));
  if (!ring)
    {
      return NULL;
    }

  sbr_init_index_ring (ring, num);
  return ring;
}

/*****************************************************************************
*   Prototype    : sbr_index_ring_enqueue
*   Description  : enqueue data,val != 0
*   Input        : sbr_index_ring* ring
*                  i32 val
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_index_ring_enqueue (sbr_index_ring * ring, i32 val)
{
  if (0 == val)
    {
      return -1;
    }

  sbr_index_node expect_node;
  sbr_index_node cur_node;
  u32 tmp_head;
  u32 tmp_tail;
  u32 cur_head = ring->head;
  u32 mask = ring->mask;
  u32 size = ring->num;

  do
    {
      tmp_tail = ring->tail;
      if (tmp_tail + size - cur_head == 0)
        {
          if (ring->nodes[tmp_tail & mask].val == 0)
            {
              (void) __sync_bool_compare_and_swap (&ring->tail, tmp_tail,
                                                   tmp_tail + 1);
            }
          else
            {
              return 0;
            }
        }

      expect_node.ver = cur_head - size;
      expect_node.val = 0;

      cur_node.ver = cur_head;
      cur_node.val = val;

      if ((ring->nodes[cur_head & mask].ver == expect_node.ver)
          && __sync_bool_compare_and_swap (&ring->nodes[cur_head & mask].data,
                                           expect_node.data, cur_node.data))
        {
          tmp_head = ring->head;
          if ((tmp_head - cur_head > 0x80000000) && (0 == (cur_head & 0x11)))
            {
              (void) __sync_bool_compare_and_swap (&ring->head, tmp_head,
                                                   cur_head);
            }

          break;
        }

      tmp_head = ring->head;
      cur_head = cur_head - tmp_head < mask - 1 ? cur_head + 1 : tmp_head;
    }
  while (1);

  return 1;
}

/*****************************************************************************
*   Prototype    : sbr_index_ring_dequeue
*   Description  : dequeue
*   Input        : sbr_index_ring* ring
*                  i32* val
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_index_ring_dequeue (sbr_index_ring * ring, i32 * val)
{
  u32 cur_tail;
  u32 tmp_tail;
  u32 tmp_head;
  u32 mask = ring->mask;
  sbr_index_node null_node;
  sbr_index_node expect_node;

  cur_tail = ring->tail;
  do
    {
      tmp_head = ring->head;
      if (cur_tail == tmp_head)
        {
          if (0 != (ring->nodes[tmp_head & mask].val))
            {
              (void) __sync_bool_compare_and_swap (&ring->head, tmp_head,
                                                   tmp_head + 1);
            }
          else
            {
              return 0;
            }
        }

      null_node.ver = cur_tail;
      null_node.val = 0;
      expect_node = ring->nodes[cur_tail & mask];

      if ((null_node.ver == expect_node.ver) && (expect_node.val)
          && __sync_bool_compare_and_swap (&ring->nodes[cur_tail & mask].data,
                                           expect_node.data, null_node.data))

        {
          *val = expect_node.val;
          tmp_tail = ring->tail;
          if ((tmp_tail - cur_tail > 0x80000000) && (0 == (cur_tail & 0x11)))
            {
              (void) __sync_bool_compare_and_swap (&ring->tail, tmp_tail,
                                                   cur_tail);
            }

          break;
        }

      tmp_tail = ring->tail;
      cur_tail = cur_tail - tmp_tail < mask - 1 ? cur_tail + 1 : tmp_tail;
    }
  while (1);

  return 1;
}
