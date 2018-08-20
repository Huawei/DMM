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
#include <stdio.h>
#include <sys/types.h>

#include "dmm_config.h"
#include "dmm_share.h"

#define DMM_HUGE_FMT "%s/dmm-%d-%d"     /* HUGE_DIR pid index */

inline static void
huge_set_path (struct dmm_share *share, int index)
{
  (void) snprintf (share->path, sizeof (share->path), DMM_HUGE_FMT,
                   DMM_HUGE_DIR, share->pid, index);
}

int
dmm_huge_create (struct dmm_share *share)
{
  return -1;
}

int
dmm_huge_delete (struct dmm_share *share)
{
  return -1;
}

int
dmm_huge_attach (struct dmm_share *share)
{
  return -1;
}

int
dmm_huge_detach (struct dmm_share *share)
{
  return -1;
}
