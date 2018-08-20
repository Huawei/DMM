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
#ifndef _DMM_ATOMIC_H_
#define _DMM_ATOMIC_H_

/* atomic 32 bit operation */

typedef struct
{
  volatile int cnt;
} dmm_atomic_t;

inline static int
dmm_atomic_get (dmm_atomic_t * a)
{
  return a->cnt;
}

inline static int
dmm_atomic_add (dmm_atomic_t * a, int n)
{
  return __sync_fetch_and_add (&a->cnt, n);
}

inline static int
dmm_atomic_sub (dmm_atomic_t * a, int n)
{
  return __sync_fetch_and_sub (&a->cnt, n);
}

inline static int
dmm_atomic_and (dmm_atomic_t * a, int n)
{
  return __sync_fetch_and_and (&a->cnt, n);
}

inline static int
dmm_atomic_or (dmm_atomic_t * a, int n)
{
  return __sync_fetch_and_or (&a->cnt, n);
}

inline static int
dmm_atomic_xor (dmm_atomic_t * a, int n)
{
  return __sync_fetch_and_xor (&a->cnt, n);
}

inline static int
dmm_atomic_swap (dmm_atomic_t * a, int o, int n)
{
  return __sync_val_compare_and_swap (&a->cnt, o, n);
}

inline static int
dmm_atomic_add_return (dmm_atomic_t * a, int n)
{
  return __sync_add_and_fetch (&a->cnt, n);
}

inline static int
dmm_atomic_sub_return (dmm_atomic_t * a, int n)
{
  return __sync_sub_and_fetch (&a->cnt, n);
}

inline static int
dmm_atomic_and_return (dmm_atomic_t * a, int n)
{
  return __sync_and_and_fetch (&a->cnt, n);
}

inline static int
dmm_atomic_or_return (dmm_atomic_t * a, int n)
{
  return __sync_or_and_fetch (&a->cnt, n);
}

inline static int
dmm_atomic_xor_return (dmm_atomic_t * a, int n)
{
  return __sync_xor_and_fetch (&a->cnt, n);
}

/* atomit 64bit operation */

typedef struct
{
  volatile long long int cnt;
} dmm_atomic64_t;

inline static long long int
dmm_atomic64_get (dmm_atomic64_t * a)
{
  return a->cnt;
}

inline static long long int
dmm_atomic64_add (dmm_atomic64_t * a, int n)
{
  return __sync_fetch_and_add (&a->cnt, n);
}

inline static long long int
dmm_atomic64_sub (dmm_atomic64_t * a, int n)
{
  return __sync_fetch_and_sub (&a->cnt, n);
}

inline static long long int
dmm_atomic64_and (dmm_atomic64_t * a, int n)
{
  return __sync_fetch_and_and (&a->cnt, n);
}

inline static long long int
dmm_atomic64_or (dmm_atomic64_t * a, int n)
{
  return __sync_fetch_and_or (&a->cnt, n);
}

inline static long long int
dmm_atomic64_xor (dmm_atomic64_t * a, int n)
{
  return __sync_fetch_and_xor (&a->cnt, n);
}

inline static long long int
dmm_atomic64_swap (dmm_atomic_t * a, int o, int n)
{
  return __sync_val_compare_and_swap (&a->cnt, o, n);
}

inline static long long int
dmm_atomic64_add_return (dmm_atomic64_t * a, int n)
{
  return __sync_add_and_fetch (&a->cnt, n);
}

inline static long long int
dmm_atomic64_sub_return (dmm_atomic64_t * a, int n)
{
  return __sync_sub_and_fetch (&a->cnt, n);
}

inline static long long int
dmm_atomic64_and_return (dmm_atomic64_t * a, int n)
{
  return __sync_and_and_fetch (&a->cnt, n);
}

inline static long long int
dmm_atomic64_or_return (dmm_atomic64_t * a, int n)
{
  return __sync_or_and_fetch (&a->cnt, n);
}

inline static long long int
dmm_atomic64_xor_return (dmm_atomic64_t * a, int n)
{
  return __sync_xor_and_fetch (&a->cnt, n);
}

#endif /* #ifndef _DMM_ATOMIC_H_ */
