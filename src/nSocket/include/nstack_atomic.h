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

#ifndef _NSTACK_ATOMIC_H_
#define _NSTACK_ATOMIC_H_
#include <stdio.h>
#include <stdlib.h>

#include "nstack_types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif

typedef struct
{
  volatile ns_int32 counter;
} atomic_t;

#define atomic_set(v, i) ((v)->counter = (i))

#define atomic_fetch_and_add(v, i) __sync_fetch_and_add(&(v)->counter, i)
#define atomic_add_and_fetch(v, i) __sync_add_and_fetch(&(v)->counter, i)

#define atomic_fetch_and_sub(v, i) __sync_fetch_and_sub(&(v)->counter, i)
#define atomic_sub_and_fetch(v, i) __sync_sub_and_fetch(&(v)->counter, i)

#define atomic_read(v) atomic_fetch_and_add(v, 0)
#define atomic_inc(v) atomic_add_and_fetch(v, 1)
#define atomic_dec(v)atomic_sub_and_fetch(v, 1)
#define atomic_add(v, i) atomic_add_and_fetch(v, i)
#define atomic_sub(v, i) atomic_sub_and_fetch(v,i)

#define cas(ptr, oldValue, exchange) __sync_bool_compare_and_swap(ptr, oldValue, exchange)

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
