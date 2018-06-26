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
#include <sched.h>
#include "nstack_securec.h"

#include "nsfw_mem_desc.h"
#include "nsfw_nshmem_ring.h"
#include "nsfw_ring_fun.h"
#include "common_func.h"

/*copy the data to obj*/
NSTACK_STATIC inline void
nsfw_nshmem_ring_obj_copy (struct nsfw_mem_ring *r, uint32_t cons_head,
                           void **obj_table, unsigned n)
{
  uint32_t idx = cons_head & r->mask;
  unsigned i = 0;
  const uint32_t size = r->size;

  if (likely (idx + n < size))
    {
      for (i = 0; i < (n & (~(unsigned) 0x3)); i += 4, idx += 4)
        {
          obj_table[i] = (void *) r->ring[idx].data_l;
          obj_table[i + 1] = (void *) r->ring[idx + 1].data_l;
          obj_table[i + 2] = (void *) r->ring[idx + 2].data_l;
          obj_table[i + 3] = (void *) r->ring[idx + 3].data_l;
        }
      switch (n & 0x3)
        {
        case 3:
          obj_table[i++] = (void *) r->ring[idx++].data_l;

        case 2:
          obj_table[i++] = (void *) r->ring[idx++].data_l;

        case 1:
          obj_table[i++] = (void *) r->ring[idx++].data_l;
        }
    }
  else
    {
      for (i = 0; idx < size; i++, idx++)
        {
          obj_table[i] = (void *) r->ring[idx].data_l;
        }

      for (idx = 0; i < n; i++, idx++)
        {
          obj_table[i] = (void *) r->ring[idx].data_l;
        }
    }
}

/*fork recover*/
NSTACK_STATIC inline void
nsfw_nshmem_enqueue_fork_recov (struct nsfw_mem_ring *r)
{
  u32_t pidflag = 0;
  u32_t curpid = get_sys_pid ();
  int success = 0;
  /*if pid is not the same, maybe mult thread fork happen */
  pidflag = r->prodhflag;

  if (unlikely (pidflag != curpid))
    {
      success = common_mem_atomic32_cmpset (&r->prodhflag, pidflag, curpid);

      if (unlikely (success != 0))
        {
          /*recover it */
          if (r->prod.tail != r->prod.head)
            {
              r->prod.head = r->prod.tail;
            }

          r->prodtflag = curpid;
        }
    }

  return;
}

NSTACK_STATIC inline void
nsfw_nshmem_dequeue_fork_recov (struct nsfw_mem_ring *r)
{
  u32_t pidflag = 0;
  u32_t curpid = get_sys_pid ();
  int success = 0;
  /*if pid is not the same, maybe mult thread fork happen */
  pidflag = r->conshflag;

  if (unlikely (pidflag != curpid))
    {
      success = common_mem_atomic32_cmpset (&r->conshflag, pidflag, curpid);

      if (unlikely (success != 0))
        {
          /*recover it */
          if (r->cons.tail != r->cons.head)
            {
              r->cons.head = r->cons.tail;
            }

          r->constflag = curpid;
        }
    }

  return;
}

/*
this is a  multi thread/process enqueue function, please pay attention to the bellow point
1. while Enqueue corrupt,  we may lose one element; because no one to add the Head
*/
int
nsfw_nshmem_ring_mp_enqueue (struct nsfw_mem_ring *mem_ring, void *obj_table)
{
  uint32_t producer_head, producer_next;
  uint32_t consumer_tail, free_entries;
  int success;
  unsigned rep = 0;
  uint32_t mask = mem_ring->mask;
  uint32_t size = mem_ring->size;
  uint32_t n = 1;

  /* move prod.head atomically */
  do
    {

      producer_head = mem_ring->prod.head;
      consumer_tail = mem_ring->cons.tail;
      /* The subtraction is done between two unsigned 32bits value
       * (the result is always modulo 32 bits even if we have
       * producer_head > consumer_tail). So 'free_entries' is always between 0
       * and size(ring)-1. */
      free_entries = (size + consumer_tail - producer_head);

      /* check that we have enough room in ring */
      if (unlikely (n > free_entries))
        {
          return 0;
          /* Below code is commented currenlty as its a dead code. */
        }

      /*if pid is not the same, maybe mult thread fork happen */
      nsfw_nshmem_enqueue_fork_recov (mem_ring);

      while (unlikely
             ((mem_ring->prod.tail != mem_ring->prod.head)
              || (mem_ring->prodtflag != mem_ring->prodhflag)))
        {
          common_mem_pause ();
        }

      producer_next = producer_head + n;
      success =
        common_mem_atomic32_cmpset (&mem_ring->prod.head, producer_head,
                                    producer_next);
    }
  while (unlikely (success == 0));

  mem_ring->ring[producer_head & mask].data_l = (u64) obj_table;

  /*
   * If there are other enqueues in progress that preceded us,
   * we need to wait for them to complete
   */
  while (unlikely (mem_ring->prod.tail != producer_head))
    {
      common_mem_pause ();

      /* Set COMMON_RING_PAUSE_REP_COUNT to avoid spin too long waiting
       * for other thread finish. It gives pre-emptied thread a chance
       * to proceed and finish with ring dequeue operation. */
      /* check the queue can be operate */
      if (++rep == 5)
        {
          rep = 0;
          (void) sched_yield ();
        }
    }

  mem_ring->prod.tail = producer_next;
  return (int) n;
}

/*
   this is a  single thread/process enqueue function
 */
int
nsfw_nshmem_ring_sp_enqueue (struct nsfw_mem_ring *r, void *obj_table)
{
  uint32_t producer_head, consumer_tail;
  uint32_t producer_next, free_entries;
  uint32_t mask = r->mask;
  uint32_t n = 1;
  uint32_t size = r->size;

  producer_head = r->prod.head;
  consumer_tail = r->cons.tail;
  /* The subtraction is done between two unsigned 32bits value
   * (the result is always modulo 32 bits even if we have
   * producer_head > consumer_tail). So 'free_entries' is always between 0
   * and size(ring)-1. */
  free_entries = size + consumer_tail - producer_head;

  /* check that we have enough room in ring */
  if (unlikely (n > free_entries))
    {
      return 0;
    }

  nsfw_nshmem_enqueue_fork_recov (r);

  producer_next = producer_head + n;
  r->prod.head = producer_next;

  r->ring[producer_head & mask].data_l = (u64) obj_table;

  r->prod.tail = producer_next;
  return (int) n;
}

/*
   this is enhanced mc_ring_dequeue, support dequeue multi element one time.
*/
int
nsfw_nshmem_ring_mc_dequeuev (struct nsfw_mem_ring *r, void **obj_table,
                              unsigned int n)
{
  uint32_t consumer_head, producer_tail;
  uint32_t consumer_next, entries;
  int success;
  unsigned rep = 0;
  uint32_t num = n;

  /* Avoid the unnecessary cmpset operation below, which is also
   * potentially harmful when n equals 0. */
  if (unlikely (num == 0))
    {
      return 0;
    }

  nsfw_nshmem_dequeue_fork_recov (r);

  /* move cons.head atomically */
  do
    {
      num = n;
      consumer_head = r->cons.head;
      producer_tail = r->prod.tail;
      /* The subtraction is done between two unsigned 32bits value
       * (the result is always modulo 32 bits even if we have
       * cons_head > prod_tail). So 'entries' is always between 0
       * and size(ring)-1. */
      entries = (producer_tail - consumer_head);

      /* Set the actual entries for dequeue */
      if (unlikely (num > entries))
        {
          if (likely (entries > 0))
            {
              num = entries;
            }
          else
            {
              return 0;
            }
        }

      /* check the queue can be operate */
      while (unlikely
             ((r->cons.tail != r->cons.head)
              || (r->conshflag != r->constflag)))
        {
          common_mem_pause ();
        }

      consumer_next = consumer_head + num;

      success =
        common_mem_atomic32_cmpset (&r->cons.head, consumer_head,
                                    consumer_next);
    }
  while (unlikely (success == 0));

  nsfw_nshmem_ring_obj_copy (r, consumer_head, obj_table, num);

  /*
   * If there are other dequeues in progress that preceded us,
   * we need to wait for them to complete
   */
  while (unlikely (r->cons.tail != consumer_head))
    {
      common_mem_pause ();

      /* Set COMMON_RING_PAUSE_REP_COUNT to avoid spin too long waiting
       * for other thread finish. It gives pre-emptied thread a chance
       * to proceed and finish with ring dequeue operation. */
      /* check the queue can be operate */
      if (++rep == 5)
        {
          rep = 0;
          (void) sched_yield ();
        }
    }

  r->cons.tail = consumer_next;

  return (int) num;
}

/*this is a  multi thread/process dequeue function, please pay attention to the bellow point
1. while dequeue corrupt,  the tail no one added, may multi the try times.
*/
int
nsfw_nshmem_ring_mc_dequeue (struct nsfw_mem_ring *ring, void **box)
{
  return nsfw_nshmem_ring_mc_dequeuev (ring, box, 1);
}

/*
   this is a single thread/process dequeue function
*/
int
nsfw_nshmem_ring_sc_dequeuev (struct nsfw_mem_ring *r, void **obj_table,
                              unsigned int n)
{
  uint32_t consumer_head, producer_tail;
  uint32_t consumer_next, entries;
  uint32_t inum = n;
  consumer_head = r->cons.head;
  producer_tail = r->prod.tail;
  /* The subtraction is done between two unsigned 32bits value
   * (the result is always modulo 32 bits even if we have
   * cons_head > prod_tail). So 'entries' is always between 0
   * and size(ring)-1. */
  entries = producer_tail - consumer_head;

  if (unlikely (inum > entries))
    {
      if (likely (entries > 0))
        {
          inum = entries;
        }
      else
        {
          return 0;
        }
    }

  nsfw_nshmem_dequeue_fork_recov (r);

  consumer_next = consumer_head + inum;
  r->cons.head = consumer_next;

  nsfw_nshmem_ring_obj_copy (r, consumer_head, obj_table, inum);

  r->cons.tail = consumer_next;
  return (int) inum;
}

/*
   this is enhanced mc_ring_dequeue, support dequeue multi element one time.
*/
int
nsfw_nshmem_ring_sc_dequeue (struct nsfw_mem_ring *ring, void **box)
{
  return nsfw_nshmem_ring_sc_dequeuev (ring, box, 1);
}

/*stack just using one thread, for performance using que not support multi thread*/
int
nsfw_nshmem_ring_singlethread_enqueue (struct nsfw_mem_ring *ring, void *box)
{
  u32 head = 0;

  /*if queue is full, just return 0 */
  if (unlikely (ring->prod.head >= (ring->size + ring->cons.tail)))
    {
      return 0;
    }

  head = ring->prod.head;
  ring->ring[head & ring->mask].data_l = (u64) box;
  ring->prod.head++;
  return 1;
}

/*stack just using one thread, for performance using que not support multi thread*/
int
nsfw_nshmem_ring_singlethread_dequeue (struct nsfw_mem_ring *ring, void **box)
{
  return nsfw_nshmem_ring_singlethread_dequeuev (ring, box, 1);
}

/*stack just using one thread, for performance using que not support multi thread*/
int
nsfw_nshmem_ring_singlethread_dequeuev (struct nsfw_mem_ring *ring,
                                        void **box, unsigned int n)
{
  u32 tail = 0;
  u32 num = 0;

  while (num < n)
    {
      tail = ring->cons.tail;

      /* if all entries are dequeued return 0 */
      if (unlikely (ring->prod.head == ring->cons.tail))
        {
          return num;
        }

      box[num] = (void *) ring->ring[tail & ring->mask].data_l;
      ring->cons.tail++;
      num++;
    }

  return num;
}
