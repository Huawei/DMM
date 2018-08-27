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
#include <sys/types.h>
#include <unistd.h>

#include "dmm_config.h"
#include "dmm_memory.h"
#include "dmm_group.h"

#define DMM_MEGABYTE (1024 * 1024)

/* shared from main process */
static struct dmm_share *main_share = NULL;
struct dmm_segment *main_seg = NULL;

/* shared by process tree */
static struct dmm_share base_share = { 0 };

struct dmm_segment *base_seg = NULL;

int
dmm_mem_main_init ()
{
  int ret;

  ret = dmm_group_create_main ();
  if (ret)
    {
      return -1;
    }

  main_share = &main_group->share;
  main_share->type = DMM_MAIN_SHARE_TYPE;
  main_share->size = DMM_MAIN_SHARE_SIZE * DMM_MEGABYTE;
  main_share->base = NULL;
  main_share->pid = getpid ();
  ret = dmm_share_create (main_share);
  if (ret)
    {
      return -1;
    }

  main_seg = dmm_seg_create (main_share->base, main_share->size);
  if (!main_seg)
    {
      return -1;
    }

  dmm_group_active ();

  return 0;
}

int
dmm_mem_main_exit ()
{
  dmm_group_delete_main ();
  return 0;
}

int
dmm_mem_app_init ()
{
  int ret;

  ret = dmm_group_attach_main ();
  if (0 == ret)
    {
      main_share = &main_group->share;
      ret = dmm_share_attach (main_share);
      if (ret)
        {
          return -1;
        }

      main_seg = dmm_seg_attach (main_share->base, main_share->size);
      if (!main_seg)
        {
          return -1;
        }

      /* now share main process share-memory */
      base_seg = main_seg;
    }
  else
    {
      base_share.type = DMM_SHARE_TYPE;
      base_share.size = DMM_SHARE_SIZE * DMM_MEGABYTE;
      base_share.base = NULL;
      base_share.pid = getpid ();
      ret = dmm_share_create (&base_share);
      if (ret)
        {
          return -1;
        }

      base_seg = dmm_seg_create (base_share.base, base_share.size);
      if (!base_seg)
        {
          return -1;
        }
    }

  return 0;
}

int
dmm_mem_app_exit ()
{
  dmm_group_detach_main ();

  if (base_share.base)
    dmm_share_delete (&base_share);

  base_share.base = NULL;
  base_seg = NULL;
  main_seg = NULL;

  return 0;
}
