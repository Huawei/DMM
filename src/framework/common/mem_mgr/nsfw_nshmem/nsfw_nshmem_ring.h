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

#ifndef _NSFW_NSHMEM_RING_H_
#define _NSFW_NSHMEM_RING_H_

#include <stdint.h>

int nsfw_nshmem_ring_mp_enqueue (struct nsfw_mem_ring *ring, void *box);
int nsfw_nshmem_ring_sp_enqueue (struct nsfw_mem_ring *ring, void *box);
int nsfw_nshmem_ring_mc_dequeue (struct nsfw_mem_ring *ring, void **box);
int nsfw_nshmem_ring_mc_dequeuev (struct nsfw_mem_ring *ring, void **box,
                                  unsigned int n);
int nsfw_nshmem_ring_sc_dequeue (struct nsfw_mem_ring *ring, void **box);
int nsfw_nshmem_ring_sc_dequeuev (struct nsfw_mem_ring *ring, void **box,
                                  unsigned int n);
int nsfw_nshmem_ring_singlethread_enqueue (struct nsfw_mem_ring *ring,
                                           void *box);
int nsfw_nshmem_ring_singlethread_dequeue (struct nsfw_mem_ring *ring,
                                           void **box);
int nsfw_nshmem_ring_singlethread_dequeuev (struct nsfw_mem_ring *ring,
                                            void **box, unsigned int n);

#endif /*_NSFW_NSHMEM_RING_H_*/
