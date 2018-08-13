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

#ifndef _SBR_INDEX_RING_H_
#define _SBR_INDEX_RING_H_

#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef struct
{
  union
  {
    struct
    {
      volatile u32 ver;
      volatile u32 val;
    };
    u64 data;
  };
} sbr_index_node;

typedef struct
{
  volatile u32 head;
  i8 cache_space[124];
  volatile u32 tail;
  u32 num;
  u32 mask;
  sbr_index_node nodes[0];
} sbr_index_ring;

sbr_index_ring *sbr_create_index_ring (u32 num);
int sbr_index_ring_enqueue (sbr_index_ring * ring, i32 val);
int sbr_index_ring_dequeue (sbr_index_ring * ring, i32 * val);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
