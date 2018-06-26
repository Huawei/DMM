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

#ifndef _NSFW_RING_DATA_H_
#define _NSFW_RING_DATA_H_

#include <stdint.h>
#include "types.h"
#include "common_mem_api.h"

#define VALUE_LEN 40

/*
Ring Data has two part; Ver&Data
val is a pointer offset base on rte_perf_ring::Addrbase, this struct support 1TB, it's enough now;
future __int128 maybe used, this type can perfectly solve the version & address rang problem.
*/
union RingData_U
{
  struct RingData_S
  {
    /*
       value of data, indeed it's a pointer offset base on rte_perf_ring::Addrbase;40bit is enough for user space addr
       ver must using 24bit, so val using 40bit; a CAS  now just support 64bit; in future, we may using __int128,now __int128 not support well.
     */
    volatile unsigned long long val:VALUE_LEN;
    /*
       version of data,  using 16b store version flg is more suitable for Address save, but using 16b version is too short, it's value range is [0-65535];
       between two cpu schedule time (TM-SPACE) of one process/thread, other processes/threads do N times queue oper. if N > 65535, still have a chance of ABA.
       if using a 24bit  save version flg, if ABA happened, 16777216 times queue oper need done in one TM-SPACE, it's impossible for today cpu.
     */
    volatile unsigned long long ver:(64 - VALUE_LEN);
  } data_s;
  u64 data_l;
};

/*
   this high perf Ring rely on the init value of Ring Slot;
   Ring Must init using PerfRingInit, Pool Must init using PerfPoolInit

   the addrbase is base addr for all element; now we support 1024G offset;
   for nstack the Ring element is from hugepage, and the addr is in stack space.

   1. not support a ring who's element space range bigger than 1024GB
      [if one element from heep, one from stack, range will bigger than 1024GB, we not support]
   2. one more thing addr from mmap is in stack
   3. rte_perf_ring must create by rte_perf_ring_create/rte_perf_pool_create
*/
struct nsfw_mem_ring
{
  u8 memtype;                   //shared, no shared
  u8 ringflag;                  //scmp, scsp, mcsp,mcmp
  u16 reserv;                   //reserv data
  u32 size;                     //size of the Ring, must 2^n
  u32 eltsize;                  //for s-pool, it is the size of per buf, if is ring, eltsize is zero.
  u32 mask;                     //mask of the Ring,  used mask mod Head/Tail to get real pos, must 2^n-1
  void *Addrbase;               /*Cause the Addr we support just 40b(1024G),  we using a basAddr+offset to get the real addr; ring[x].data_s.val just store offset;
                                 * not used when no shared mode
                                 */
  volatile u32_t prodhflag;     //for nshmem fork recover
  volatile u32_t prodtflag;     //for nshmem fork recover
  volatile u32_t conshflag;     //for nshmem fork recover
  volatile u32_t constflag;     //for nshmem fork recover
  nsfw_res res_chk;

  struct
  {
    volatile u32 head;          //Head of the Ring, used to indicate pos where to pull a val
    volatile u32 tail;          //for nshmem, shmem not used.
  } prod;
  struct
  {
    volatile u32 head;          //for nshmem, shmem not used.
    volatile u32 tail;          //Tail of the Ring, used to indicate pos where to push a val
  } cons;
  u32 uireserv[4];              //reserved for update
  union RingData_U ring[0];     //Value of Ring
};

#define PERFRING_ADDR_RANGE  (0xFFFFFFFFFFL)

#endif /*_NSFW_RING_DATA_H_*/
