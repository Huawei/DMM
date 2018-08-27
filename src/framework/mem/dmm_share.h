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
#ifndef _DMM_SHARE_H_
#define _DMM_SHARE_H_

#define DMM_SHARE_PATH_MAX 100

enum dmm_share_type
{
  DMM_SHARE_HEAP,
  DMM_SHARE_FSHM,
  DMM_SHARE_HUGE,

  DMM_SHARE_ANY = -1
};

struct dmm_share
{
  int type;                     /* share type enum dmm_share_type */
  pid_t pid;                    /* owner/creator pid */
  void *base;                   /* base logical address */
  size_t size;                  /* memory size */
  char path[DMM_SHARE_PATH_MAX];        /* share path */
};

int dmm_heap_create (struct dmm_share *share);
int dmm_heap_delete (struct dmm_share *share);
int dmm_heap_attach (struct dmm_share *share);
int dmm_heap_detach (struct dmm_share *share);

int dmm_fshm_create (struct dmm_share *share);
int dmm_fshm_delete (struct dmm_share *share);
int dmm_fshm_attach (struct dmm_share *share);
int dmm_fshm_detach (struct dmm_share *share);

int dmm_huge_create (struct dmm_share *share);
int dmm_huge_delete (struct dmm_share *share);
int dmm_huge_attach (struct dmm_share *share);
int dmm_huge_detach (struct dmm_share *share);

#define DMM_SHARE_DISPATCH(share, action)   \
({                                          \
  int _r;                                   \
  switch (share->type)                      \
    {                                       \
    case DMM_SHARE_HEAP:                    \
      _r = dmm_heap_##action(share);        \
      break;                                \
    case DMM_SHARE_FSHM:                    \
      _r = dmm_fshm_##action(share);        \
      break;                                \
    case DMM_SHARE_HUGE:                    \
      _r = dmm_huge_##action(share);        \
      break;                                \
    default:                                \
      _r = -1;                              \
  }                                         \
  _r;                                       \
})

/* create share memory
input: share->type, share->size, share->pid
output: share->base, share->path
*/
inline static int
dmm_share_create (struct dmm_share *share)
{
  return DMM_SHARE_DISPATCH (share, create);
}

/* delete share memory
input: share->type, share->base, share->size, share->path
*/
inline static int
dmm_share_delete (struct dmm_share *share)
{
  return DMM_SHARE_DISPATCH (share, delete);
}

/* attach share memory
input: share->type share->path [share->size] [share->base]
output: share->base, share->size
*/
inline static int
dmm_share_attach (struct dmm_share *share)
{
  return DMM_SHARE_DISPATCH (share, attach);
}

/* attach share memory
input: share->type share->size share->base
*/
inline static int
dmm_share_detach (struct dmm_share *share)
{
  return DMM_SHARE_DISPATCH (share, detach);
}

#undef DMM_SHARE_DISPATCH

#endif /* #ifndef _DMM_SHARE_H_ */
