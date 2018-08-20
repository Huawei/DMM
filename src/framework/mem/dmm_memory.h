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
#ifndef _DMM_MEMORY_H_
#define _DMM_MEMORY_H_
#include <stdio.h>
#include <stdarg.h>
#include "dmm_share.h"
#include "dmm_segment.h"

int dmm_mem_main_init ();
int dmm_mem_main_exit ();

int dmm_mem_app_init ();
int dmm_mem_app_exit ();

extern struct dmm_segment *main_seg;
extern struct dmm_segment *base_seg;

inline static void *
dmm_map (size_t size, const char name[DMM_MEM_NAME_SIZE])
{
  return dmm_mem_map (base_seg, size, name);
}

inline static void *
dmm_mapv (size_t size, const char *name_fmt, ...)
{
  int len;
  char name[DMM_MEM_NAME_SIZE];
  va_list ap;

  va_start (ap, name_fmt);
  len = vsnprintf (name, DMM_MEM_NAME_SIZE, name_fmt, ap);
  va_end (ap);

  if (len >= DMM_MEM_NAME_SIZE)
    return NULL;

  return dmm_map (size, name);
}

inline static void *
dmm_lookup (const char name[DMM_MEM_NAME_SIZE])
{
  return dmm_mem_lookup (base_seg, name);
}

inline static void *
dmm_lookupv (const char *name_fmt, ...)
{
  int len;
  char name[DMM_MEM_NAME_SIZE];
  va_list ap;

  va_start (ap, name_fmt);
  len = vsnprintf (name, DMM_MEM_NAME_SIZE, name_fmt, ap);
  va_end (ap);

  if (len >= DMM_MEM_NAME_SIZE)
    return NULL;

  return dmm_mem_lookup (base_seg, name);
}

#endif /* _DMM_MEMORY_H_ */
