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
#ifndef _DMM_RWLOCK_H_
#define _DMM_RWLOCK_H_

#include "dmm_pause.h"

typedef struct
{
  volatile int lock;
} dmm_rwlock_t;

#define DMM_RWLOCK_INIT { 0 }

inline static void
dmm_rwlock_init (dmm_rwlock_t * rwlock)
{
  rwlock->lock = 0;
}

inline static void
dmm_read_lock (dmm_rwlock_t * rwlock)
{
  int val;

  do
    {
      if ((val = rwlock->lock) < 0)
        {
          dmm_pause ();
          continue;
        }
    }
  while (!__sync_bool_compare_and_swap (&rwlock->lock, val, val + 1));
}

inline static void
dmm_read_unlock (dmm_rwlock_t * rwlock)
{
  __sync_sub_and_fetch (&rwlock->lock, 1);
}

inline static void
dmm_write_lock (dmm_rwlock_t * rwlock)
{
  do
    {
      if (rwlock->lock != 0)
        {
          dmm_pause ();
          continue;
        }
    }
  while (!__sync_bool_compare_and_swap (&rwlock->lock, 0, -1));
}

inline static void
dmm_write_unlock (dmm_rwlock_t * rwlock)
{
  rwlock->lock = 0;
}

#endif /* #ifndef _DMM_RWLOCK_H_ */
