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
#include <common_sys_config.h>

#include "common_mem_pal.h"
#include "common_mem_pal_memconfig.h"
#include "nstack_securec.h"
#include "nsfw_shmem_ring.h"
#include "nsfw_ring_fun.h"
#include "common_mem_buf.h"
#include "common_func.h"

void
nsfw_shmem_ring_baseaddr_query (uint64_t * rte_lowest_addr,
                                uint64_t * rte_highest_addr)
{
  struct common_mem_mem_config *pMemCfg =
    common_mem_pal_get_configuration ()->mem_config;
  struct common_mem_memseg *PMemSegArry = pMemCfg->memseg;

  *rte_lowest_addr = PMemSegArry[0].addr_64;
  *rte_highest_addr = PMemSegArry[0].addr_64 + PMemSegArry[0].len;

  int s = 1;

  while (s < COMMON_MEM_MAX_MEMSEG && PMemSegArry[s].len > 0)
    {
      if (*rte_lowest_addr > PMemSegArry[s].addr_64)
        {
          *rte_lowest_addr = PMemSegArry[s].addr_64;
        }

      if (*rte_highest_addr < PMemSegArry[s].addr_64 + PMemSegArry[s].len)
        {
          *rte_highest_addr = PMemSegArry[s].addr_64 + PMemSegArry[s].len;
        }

      s++;
    }

}

static unsigned
nsfw_shmem_pool_node_alloc (struct nsfw_mem_ring *perfring_ptr,
                            unsigned alloc_index, unsigned max_index,
                            void *mz, size_t mz_len, unsigned elt_size)
{
  size_t alloc_size = 0;
  unsigned offset_idx = 0;
  NSTCP_LOGINF ("mz(%p), mz_len = 0x%x", mz, mz_len);

  while (alloc_size + elt_size <= mz_len)
    {
      perfring_ptr->ring[alloc_index + offset_idx].data_s.ver =
        alloc_index + offset_idx;
      perfring_ptr->ring[alloc_index + offset_idx].data_s.val =
        ADDR_LTOSH_EXT (mz) - ((uint64_t) perfring_ptr->Addrbase);
      mz = (char *) mz + elt_size;
      alloc_size += elt_size;
      offset_idx++;

      if (alloc_index + offset_idx == max_index)
        {
          break;
        }
    }

  return offset_idx;
}

void
nsfw_shmem_pool_free (struct nsfw_mem_ring *perfring_ptr)
{
  struct nsfw_shmem_ring_head *ptemp;

  if (NULL == perfring_ptr)
    {
      return;
    }

  struct nsfw_shmem_ring_head *pnode =
    (struct nsfw_shmem_ring_head *) ((char *) perfring_ptr -
                                     sizeof (struct nsfw_shmem_ring_head));

  while (pnode)
    {
      // phead is involved in phead->mem_zone
      ptemp = pnode->next;
      common_mem_memzone_free (pnode->mem_zone);
      pnode = ptemp;
    }
}

struct nsfw_mem_ring *
nsfw_shmem_pool_create (const char *name, unsigned int n,
                        unsigned int elt_size, int socket_id,
                        unsigned char flag)
{
  struct nsfw_mem_ring *perfring_ptr = NULL;
  struct common_mem_memzone *mz_mem;
  void *mz = NULL;

  /*get pool size, pool size must pow of 2 */
  unsigned int num = common_mem_align32pow2 (n + 1);

  struct nsfw_shmem_ring_head *pcur = NULL;
  /*calculate the empty rte_perf_ring Size */
  size_t len =
    sizeof (struct nsfw_shmem_ring_head) + sizeof (struct nsfw_mem_ring) +
    (size_t) num * sizeof (union RingData_U) + (size_t) num * elt_size;
  size_t alloc_len = len;
  unsigned int alloc_num = 0;
  unsigned int alloc_index = 0;

  size_t mz_len = 0;

  unsigned int mz_index = 1;
  char mz_name[128] = { 0 };
  int retVal;
  /*we'd better use `strlen(src)` or `sizeof(dst)` to explain copying length of src string.
     it's meaningless using `sizeof(dst) - 1` to reserve 1 byte for '\0'.
     if copying length equals to or bigger than dst length, just let STRNCPY_S() returns failure. */
  retVal = STRNCPY_S (mz_name, sizeof (mz_name), name, sizeof (mz_name));

  if (EOK != retVal)
    {
      NSTCP_LOGERR ("STRNCPY_S failed]ret=%d", retVal);
      return NULL;
    }

  mz_mem = common_memzone_data_lookup_name (name);
  NSTCP_LOGINF ("memzone data look up] n=%u,num=%u,len=%zu", n, num, len);

  if (mz_mem)
    {
      perfring_ptr =
        (struct nsfw_mem_ring *) ((char *) mz_mem +
                                  sizeof (struct nsfw_shmem_ring_head));
      return perfring_ptr;
    }

  while (alloc_len > 0)
    {
      if (NULL != perfring_ptr)
        {
          retVal =
            SPRINTF_S (mz_name, sizeof (mz_name), "%s_%03d", name, mz_index);

          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
              nsfw_shmem_pool_free (perfring_ptr);
              return NULL;
            }
        }

      mz_mem =
        (struct common_mem_memzone *) common_mem_memzone_reserve (mz_name,
                                                                  alloc_len,
                                                                  socket_id,
                                                                  0);

      if (mz_mem == NULL)
        {
          mz_mem =
            (struct common_mem_memzone *) common_mem_memzone_reserve (mz_name,
                                                                      0,
                                                                      socket_id,
                                                                      0);
        }

      if (mz_mem == NULL)
        {
          nsfw_shmem_pool_free (perfring_ptr);
          return NULL;
        }

      if (NULL == perfring_ptr
          && (mz_mem->len <
              sizeof (struct nsfw_shmem_ring_head) +
              sizeof (struct nsfw_mem_ring) +
              num * sizeof (union RingData_U)))
        {
          common_mem_memzone_free (mz_mem);
          return NULL;
        }

      if (NULL == perfring_ptr)
        {
          pcur = (struct nsfw_shmem_ring_head *) ADDR_SHTOL (mz_mem->addr_64);
          pcur->mem_zone = mz_mem;
          pcur->next = NULL;

          perfring_ptr =
            (struct nsfw_mem_ring *) ((char *) pcur +
                                      sizeof (struct nsfw_shmem_ring_head));

          mz =
            (void *) ((char *) perfring_ptr + sizeof (struct nsfw_mem_ring) +
                      num * sizeof (union RingData_U));
          mz_len =
            mz_mem->len - sizeof (struct nsfw_shmem_ring_head) -
            sizeof (struct nsfw_mem_ring) - num * sizeof (union RingData_U);

          uint64_t rte_base_addr;
          uint64_t rte_highest_addr;
          nsfw_shmem_ring_baseaddr_query (&rte_base_addr, &rte_highest_addr);
          nsfw_mem_pool_head_init (perfring_ptr, num, elt_size,
                                   (void *) rte_base_addr, NSFW_SHMEM,
                                   (nsfw_mpool_type) flag);
        }
      else
        {
          if (pcur)
            {
              pcur->next =
                (struct nsfw_shmem_ring_head *) ADDR_SHTOL (mz_mem->addr_64);
              pcur = pcur->next;
              pcur->mem_zone = mz_mem;
              pcur->next = NULL;

              if (mz_mem->len < sizeof (struct nsfw_shmem_ring_head))
                {
                  NSCOMM_LOGERR ("mz_len error %d", mz_mem->len);
                  nsfw_shmem_pool_free (perfring_ptr);
                  return NULL;
                }

              mz =
                (void *) ((char *) pcur +
                          sizeof (struct nsfw_shmem_ring_head));
              mz_len = mz_mem->len - sizeof (struct nsfw_shmem_ring_head);
            }
        }

      alloc_num =
        nsfw_shmem_pool_node_alloc (perfring_ptr, alloc_index, num, mz,
                                    mz_len, elt_size);
      alloc_index += alloc_num;

      if (alloc_index >= num)
        {
          break;
        }

      // second time allocate should not contained all ring head
      alloc_len =
        (size_t) (num - alloc_index) * elt_size +
        sizeof (struct nsfw_shmem_ring_head);
      mz_index++;
    }

  return perfring_ptr;
}

/*ring create*/
struct nsfw_mem_ring *
nsfw_shmem_ring_create (const char *name, unsigned int n, int socket_id,
                        unsigned char flag)
{
  struct nsfw_mem_ring *perfring_ptr;
  struct common_mem_memzone *mz;
  uint64_t rte_base_addr;
  uint64_t rte_highest_addr;

  unsigned int num = common_mem_align32pow2 (n);

  mz =
    (struct common_mem_memzone *) common_mem_memzone_reserve (name,
                                                              sizeof (struct
                                                                      nsfw_mem_ring)
                                                              +
                                                              num *
                                                              sizeof (union
                                                                      RingData_U),
                                                              socket_id, 0);

  if (mz == NULL)
    {
      return NULL;
    }

  perfring_ptr = mz->addr;

  nsfw_shmem_ring_baseaddr_query (&rte_base_addr, &rte_highest_addr);
  nsfw_mem_ring_init (perfring_ptr, num, (void *) rte_base_addr, NSFW_SHMEM,
                      flag);

  return perfring_ptr;
}

/*
this is a  multi thread/process enqueue function, please pay attention to the bellow point
1. while Enqueue corrupt,  we may lose one element; because no one to add the Head
*/
int
nsfw_shmem_ring_mp_enqueue (struct nsfw_mem_ring *ring, void *box)
{
  union RingData_U expectPostVal;
  union RingData_U curVal;
  unsigned int tmpHead;
  unsigned int tmpTail;
  unsigned int CurHead = ring->prod.head;
  unsigned int mask = ring->mask;
  unsigned int size = ring->size;
  void *prmBox = NULL;

  prmBox = (void *) ADDR_LTOSH_EXT (box);

  /*do box range check */
  if ((char *) prmBox <= (char *) ring->Addrbase
      || (char *) prmBox > (char *) ring->Addrbase + PERFRING_ADDR_RANGE)
    {
      /*invalid addr of box, can't put in rte_perf_ring, maybe should set a errno here */
      return -1;
    }

  do
    {
      /*
         if the ring is Full return directly; this not a exact check, cause we made tail++ after dequeue success.
         the thing we could make sure is ring[ring->Tail&mask] already dequeue
       */
      tmpTail = ring->cons.tail;

      if (tmpTail + size - CurHead == 0)
        {
          /*
             here we give enqueue a chance to recorrect the Tail,   if tail not has Data let tail++
           */
          if (ring->ring[tmpTail & mask].data_s.val == 0)
            {
              (void) __sync_bool_compare_and_swap (&ring->cons.tail, tmpTail,
                                                   tmpTail + 1);
            }
          else
            {
              return 0;
            }
        }

      /*
         the old version of ring->ring[CurHead&mask] must CurHead - size, which enqueue set to this pos lasttime
         & the val must already dequeue. otherwise this pos can't enqueue;
       */
      expectPostVal.data_l =
        (((unsigned long long) (CurHead - size)) << VALUE_LEN);

      /*
         the new version of ring->ring[CurHead&mask] must CurHead, which enqueue set to this pos this time.
       */
      curVal.data_l =
        ((((unsigned long long) CurHead) << VALUE_LEN) |
         ((char *) prmBox - (char *) ring->Addrbase));
      if (ring->ring[CurHead & mask].data_s.ver == expectPostVal.data_s.ver
          && __sync_bool_compare_and_swap (&ring->ring[CurHead & mask].data_l,
                                           expectPostVal.data_l,
                                           curVal.data_l))
        {
          /*
             enqueue success, add Head Value now
             here we using  CAS set instead __sync_fetch_and_add(&ring->Head, 1) to assume that, if one process enqueue success && been killed before
             add Head, other process can recorrect the Head Value;
             one more thing the direction of Tail must been add-direction, so we using the condition (ring->Head - CurHead >0x80000000);
             while many thread do enqueue at same time, Head may not correct,exp:
             thread A get old head 10, thread A want set head to 11
             thread B get old head 10, thread B want set head to 12
             thread A do CAS && thread B do CAS at same time, thread A do CAS success;
             the result of head is 11, but the correct Value should be 12;

             then thread C get old head 11, thread C will set head to 13[cause pos 12 already has value, thread C will skill 12],
             the head will be recorrect by thread C.
             if no thread C, thread A& B are the last enqueue thread; the head must recorrect by the deque function.
           */
          tmpHead = ring->prod.head;

          if (0 == (CurHead & 0x03) && tmpHead - CurHead > 0x80000000)
            {
              (void) __sync_bool_compare_and_swap (&ring->prod.head, tmpHead,
                                                   CurHead + 1);
            }

          break;
        }

      /*
         CurHead++ here;
         may be cpu slice is end here; while re-sched CurHead < ring->Tail < ring->Head; to avoid multi try
         we using a cmp & CurHead++  instead CurHead++  directly.
         if using CurHead++ will amplify the probability of ABA problem
       */
      /*CurHead++; */
      tmpHead = ring->prod.head;
      CurHead = CurHead - tmpHead < mask - 1 ? CurHead + 1 : tmpHead;
    }
  while (1);

  return 1;
}

/*
   this is a  single thread/process enqueue function
 */
int
nsfw_shmem_ring_sp_enqueue (struct nsfw_mem_ring *ring, void *box)
{
  union RingData_U texpectPostVal;
  union RingData_U curVal;
  unsigned int tmpTail;
  unsigned int CurHead = ring->prod.head;
  unsigned int mask = ring->mask;
  unsigned int uisize = ring->size;
  void *prmBox = NULL;

  prmBox = (void *) ADDR_LTOSH_EXT (box);

  if ((char *) prmBox <= (char *) ring->Addrbase
      || (char *) prmBox > (char *) ring->Addrbase + PERFRING_ADDR_RANGE)
    {
      return -1;
    }

  do
    {
      tmpTail = ring->cons.tail;
      if (tmpTail + uisize - CurHead == 0)
        {
          /*
           *here we give enqueue a chance to recorrect the Tail,   if tail not has Data let tail++
           */
          if (ring->ring[tmpTail & mask].data_s.val == 0)
            {
              (void) __sync_bool_compare_and_swap (&ring->cons.tail, tmpTail,
                                                   tmpTail + 1);
            }
          else
            {
              return 0;
            }
        }
      texpectPostVal.data_l =
        (((unsigned long long) (CurHead - uisize)) << VALUE_LEN);

      curVal.data_l =
        ((((unsigned long long) CurHead) << VALUE_LEN) |
         ((char *) prmBox - (char *) ring->Addrbase));

      if (ring->ring[CurHead & mask].data_l == texpectPostVal.data_l)
        {
          ring->ring[CurHead & mask].data_l = curVal.data_l;
          ring->prod.head = CurHead + 1;
          break;
        }

      CurHead++;
    }
  while (1);
  return 1;
}

/*this is a  multi thread/process dequeue function, please pay attention to the bellow point
*/
int
nsfw_shmem_ring_mc_dequeue (struct nsfw_mem_ring *ring, void **box)
{
  unsigned int CurTail;
  unsigned int tmpTail;
  unsigned int tmpHead;
  unsigned int mask = ring->mask;
  union RingData_U curNullVal;
  union RingData_U ExcpRingVal;

  CurTail = ring->cons.tail;
  do
    {
      /*if ring is empty return directly */
      tmpHead = ring->prod.head;

      if (CurTail == tmpHead)
        {
          /*
             here we give deque a chance to recorrect the Head,   if head has Data let Head++
           */
          if (ring->ring[tmpHead & mask].data_s.val != 0)
            {
              (void) __sync_bool_compare_and_swap (&ring->prod.head, tmpHead,
                                                   tmpHead + 1);
            }
          else
            {
              return 0;
            }
        }
      curNullVal.data_l = (((unsigned long long) CurTail) << VALUE_LEN);
      ExcpRingVal = ring->ring[CurTail & mask];
      /*
       *the version of ring->ring[CurTail&mask] must CurTail&0xFFFFFF
       */
      if ((curNullVal.data_s.ver == ExcpRingVal.data_s.ver)
          && (ExcpRingVal.data_s.val)
          && __sync_bool_compare_and_swap (&ring->ring[CurTail & mask].data_l,
                                           ExcpRingVal.data_l,
                                           curNullVal.data_l))
        {

          *box =
            ADDR_SHTOL (((char *) ring->Addrbase + ExcpRingVal.data_s.val));

          /*
             enqueue success, add Tail Value now
             here we using  CAS set instead __sync_fetch_and_add(&ring->Tail, 1) to assume that, if one process dequeue success && been killed before
             add Tail, other process can recorrect the Tail Value;
             one more thing the direction of Tail must been add-direction, so we using the condition (rlTail - CurTail >0x80000000);
             while multi CAS done the result value of CurTail may not correct, but we don't care, it will be recorrect while next deque done.

             avg CAS cost 200-300 Cycles, so we using cache loop to instead some CAS;[head&tail not exact guide, so no need Do CAS evertime]
             here we using 0 == (CurTail&0x11) means we only do CAS while head/tail low bit is 0x11; four times do one CAS.
           */
          tmpTail = ring->cons.tail;

          if (0 == (CurTail & 0x03) && tmpTail - CurTail > 0x80000000)
            {
              (void) __sync_bool_compare_and_swap (&ring->cons.tail, tmpTail,
                                                   CurTail + 1);
            }
          break;
        }

      /*
         CurTail++ here;
         may be  cpu slice is end here; while re-sched CurTail < ring->Tail < ring->Head; to avoid multi try
         we using a cmp & CurTail++  instead CurTail++  directly.
         if using CurTail++ will amplify the probability of ABA problem
       */
      /*CurTail++; */
      tmpTail = ring->cons.tail;
      CurTail = CurTail - tmpTail < mask - 1 ? CurTail + 1 : tmpTail;
    }
  while (1);

  return 1;
}

/*
   this is enhanced mc_ring_dequeue, support dequeue multi element one time.
*/
int
nsfw_shmem_ring_mc_dequeuev (struct nsfw_mem_ring *ring, void **box,
                             unsigned int n)
{
  unsigned int uiCurTail;
  unsigned int tmpTail;
  unsigned int tmpHead;
  unsigned int uimask = ring->mask;
  union RingData_U curNullVal;
  union RingData_U ExcpRingVal;
  unsigned int deqNum = 0;
  uiCurTail = ring->cons.tail;
  do
    {
      /*if ring is empty return directly */
      tmpHead = ring->prod.head;
      if (uiCurTail == tmpHead)
        {
          /*
             here we give deque a chance to recorrect the Head,   if head has Data let Head++;
             here must done to avoid some msg can't deque.
           */
          if (deqNum == 0 && ring->ring[tmpHead & uimask].data_s.val != 0)
            {
              (void) __sync_bool_compare_and_swap (&ring->prod.head, tmpHead,
                                                   tmpHead + 1);
            }
          else
            {
              return deqNum;
            }
        }

      curNullVal.data_l = (((unsigned long long) uiCurTail) << VALUE_LEN);
      ExcpRingVal = ring->ring[uiCurTail & uimask];

      /*
       *the version of ring->ring[CurTail&mask] must CurTail&0xFFFFFF
       */
      if ((curNullVal.data_s.ver == ExcpRingVal.data_s.ver)
          && (ExcpRingVal.data_s.val)
          && __sync_bool_compare_and_swap (&ring->
                                           ring[uiCurTail & uimask].data_l,
                                           ExcpRingVal.data_l,
                                           curNullVal.data_l))
        {

          box[deqNum] =
            ADDR_SHTOL (((char *) ring->Addrbase + ExcpRingVal.data_s.val));

          /*
             enqueue success, add Tail Value now
             here we using  CAS set instead __sync_fetch_and_add(&ring->Tail, 1) to assume that, if one process dequeue success && been killed before
             add Tail, other process can recorrect the Tail Value;
             one more thing the direction of Tail must been add-direction, so we using the condition (rlTail - CurTail >0x80000000);

             avg CAS cost 200-300 Cycles, so we using cache loop to instead some CAS;[head&tail not exact guide, so no need Do CAS evertime]
             here we using 0 == (CurTail&0x11) means we only do CAS while head/tail low bit is 0x11; four times do one CAS.
           */
          tmpTail = ring->cons.tail;

          if (0 == (uiCurTail & 0x03) && tmpTail - uiCurTail > 0x80000000)
            {
              (void) __sync_bool_compare_and_swap (&ring->cons.tail, tmpTail,
                                                   uiCurTail + 1);
            }

          deqNum++;
        }

      /*
         CurTail++ here;
         may be  cpu slice is end here; while re-sched CurTail < ring->Tail < ring->Head; to avoid multi try
         we using a cmp & CurTail++  instead CurTail++  directly.
         if using CurTail++ will amplify the probability of ABA problem
       */
      /*CurTail++; */
      tmpTail = ring->cons.tail;
      uiCurTail = uiCurTail - tmpTail < uimask - 1 ? uiCurTail + 1 : tmpTail;

    }
  while (n > deqNum);

  return deqNum;
}

/*
   this is enhanced mc_ring_dequeue, support dequeue multi element one time.
*/
int
nsfw_shmem_ring_sc_dequeue (struct nsfw_mem_ring *ring, void **box)
{
  unsigned int CurTail;
  unsigned int mask = ring->mask;
  union RingData_U curNullVal;
  union RingData_U ExcpRingVal;
  unsigned int uitmpHead;

  CurTail = ring->cons.tail;

  do
    {
      /*if ring is empty return directly */
      uitmpHead = ring->prod.head;
      if (CurTail == uitmpHead)
        {
          /*
             here we give deque a chance to recorrect the Head,   if head has Data let Head++
           */
          if (ring->ring[uitmpHead & mask].data_s.val != 0)
            {
              (void) __sync_bool_compare_and_swap (&ring->prod.head,
                                                   uitmpHead, uitmpHead + 1);
            }
          else
            {
              return 0;
            }
        }
      curNullVal.data_l = (((unsigned long long) CurTail) << VALUE_LEN);
      ExcpRingVal = ring->ring[CurTail & mask];

      if ((curNullVal.data_s.ver == ExcpRingVal.data_s.ver)
          && (ExcpRingVal.data_s.val))
        {
          ring->ring[CurTail & mask].data_l = curNullVal.data_l;

          *box =
            ADDR_SHTOL (((char *) ring->Addrbase + ExcpRingVal.data_s.val));

          ring->cons.tail = CurTail + 1;
          break;
        }

      CurTail++;
    }
  while (1);

  return 1;
}

/*
   this is a single thread/process dequeue function
*/
int
nsfw_shmem_ring_sc_dequeuev (struct nsfw_mem_ring *ring, void **box,
                             unsigned int n)
{
  unsigned int CurTail;
  unsigned int tmpHead;
  unsigned int mask = ring->mask;
  union RingData_U curNullVal;
  union RingData_U ExcpRingVal;
  unsigned int usdeqNum = 0;

  CurTail = ring->cons.tail;

  do
    {
      /*if ring is empty return directly */
      tmpHead = ring->prod.head;
      if (CurTail == tmpHead)
        {
          /*
             here we give deque a chance to recorrect the Head,   if head has Data let Head++;
             here must done to avoid some msg can't deque.
           */
          if (usdeqNum == 0 && ring->ring[tmpHead & mask].data_s.val != 0)
            {
              (void) __sync_bool_compare_and_swap (&ring->prod.head, tmpHead,
                                                   tmpHead + 1);
            }
          else
            {
              return usdeqNum;
            }
        }
      curNullVal.data_l = (((unsigned long long) CurTail) << VALUE_LEN);
      ExcpRingVal = ring->ring[CurTail & mask];

      /*
         the version of ring->ring[CurTail&mask] must CurTail&0xFFFFFF
       */
      if ((curNullVal.data_s.ver == ExcpRingVal.data_s.ver)
          && (ExcpRingVal.data_s.val))
        {
          ring->ring[CurTail & mask].data_l = curNullVal.data_l;

          box[usdeqNum] =
            ADDR_SHTOL (((char *) ring->Addrbase + ExcpRingVal.data_s.val));

          ring->cons.tail = CurTail + 1;
          usdeqNum++;
        }

      CurTail++;
    }
  while (n > usdeqNum);

  return usdeqNum;
}

/* nstack just using one thread, for performance using que not support multi thread*/
int
nsfw_shmem_ring_singlethread_enqueue (struct nsfw_mem_ring *ring, void *box)
{
  u32 head = 0;
  void *prmBox = NULL;

  /*if queue is full, just return 0 */
  if (ring->prod.head >= (ring->size + ring->cons.tail))
    {
      return 0;
    }

  prmBox = (void *) ADDR_LTOSH_EXT (box);

  head = ring->prod.head;
  ring->prod.head = head + 1;
  ring->ring[head & ring->mask].data_s.ver = head;
  ring->ring[head & ring->mask].data_s.val =
    (char *) prmBox - (char *) ring->Addrbase;
  return 1;
}

/* nstack just using one thread, for performance using que not support multi thread*/
int
nsfw_shmem_ring_singlethread_dequeue (struct nsfw_mem_ring *ring, void **box)
{
  u32 tail = 0;

  /* if all entries are dequeued return 0 */
  if (unlikely (ring->prod.head == ring->cons.tail))
    {
      return 0;
    }

  tail = ring->cons.tail;
  *box =
    ADDR_SHTOL ((char *) ring->Addrbase +
                ring->ring[tail & ring->mask].data_s.val);
  ring->ring[tail & ring->mask].data_s.val = 0;
  ring->ring[tail & ring->mask].data_s.ver = tail;
  ring->cons.tail++;
  return 1;
}

/*    nstack just using one thread, for performance using que not support multi thread*/
int
nsfw_shmem_ring_singlethread_dequeuev (struct nsfw_mem_ring *ring, void **box,
                                       unsigned int n)
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

      ring->cons.tail = tail + 1;

      box[num] =
        ADDR_SHTOL ((char *) ring->Addrbase +
                    ring->ring[tail & ring->mask].data_s.val);

      ring->ring[tail & ring->mask].data_s.val = 0;
      ring->ring[tail & ring->mask].data_s.ver = tail;
      num++;
    }

  return num;
}
