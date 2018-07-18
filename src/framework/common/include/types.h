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

#ifndef clib_types_h
#define clib_types_h
#include <stddef.h>

/* Standard CLIB types. */

/* Define signed and unsigned 8, 16, 32, and 64 bit types
   and machine signed/unsigned word for all architectures. */
typedef char i8;
typedef short i16;

typedef unsigned char u8;
typedef unsigned short u16;

typedef int i32;
typedef long long i64;

typedef unsigned int u32;
typedef unsigned long long u64;

#ifndef bool
#define bool int
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef true
#define true 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef false
#define false 0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define PRIMARY_ADDR

typedef struct _nsfw_res
{
  u8 alloc_flag;
  u8 u8Reserve;
  u16 chk_count;
  u32 data;
} nsfw_res;

static inline void
res_alloc (nsfw_res * res)
{
  res->alloc_flag = TRUE;
  res->chk_count = 0;
  res->u8Reserve = 0;
}

static inline int
res_free (nsfw_res * res)
{
  if (TRUE != res->alloc_flag)
    {
      return -1;
    }
  res->chk_count = 0;
  res->alloc_flag = FALSE;
  return 0;
}

#define NSFW_THREAD __thread

#endif /*clib_types_h */
