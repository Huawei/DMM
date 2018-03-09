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

#include "pidinfo.h"
#include "nstack_securec.h"

inline i32
nsfw_pidinfo_init (nsfw_pidinfo * pidinfo)
{
  int retVal =
    MEMSET_S (pidinfo, sizeof (nsfw_pidinfo), 0, sizeof (nsfw_pidinfo));
  if (EOK != retVal)
    {
      return -1;
    }

  return 0;
}

inline int
nsfw_add_pid (nsfw_pidinfo * pidinfo, u32 pid)
{
  u32 i;

  for (i = 0; i < NSFW_MAX_FORK_NUM; i++)
    {
      if ((0 == pidinfo->apid[i])
          && (__sync_bool_compare_and_swap (&pidinfo->apid[i], 0, pid)))
        {
          if (pidinfo->used_size < i + 1)
            {
              pidinfo->used_size = i + 1;
            }
          return 0;
        }
    }
  return -1;
}

inline int
nsfw_del_pid (nsfw_pidinfo * pidinfo, u32 pid)
{
  u32 i;

  for (i = 0; i < pidinfo->used_size && i < NSFW_MAX_FORK_NUM; i++)
    {
      if (pid == pidinfo->apid[i])
        {
          pidinfo->apid[i] = 0;
          return 0;
        }
    }
  return -1;
}

inline int
nsfw_del_last_pid (nsfw_pidinfo * pidinfo, u32 pid)
{
  u32 i;
  int count = 0;
  int deleted = 0;
  for (i = 0; i < pidinfo->used_size && i < NSFW_MAX_FORK_NUM; i++)
    {
      if (pid == pidinfo->apid[i])
        {
          pidinfo->apid[i] = 0;
          deleted = 1;
          continue;
        }

      if (pidinfo->apid[i] != 0)
        {
          ++count;
        }
    }

  if (!deleted)
    {
      return -1;
    }

  return count;
}

inline int
nsfw_pid_exist (nsfw_pidinfo * pidinfo, u32 pid)
{
  u32 i;

  for (i = 0; i < pidinfo->used_size && i < NSFW_MAX_FORK_NUM; i++)
    {
      if (pid == pidinfo->apid[i])
        {
          return 1;
        }
    }
  return 0;
}

inline int
nsfw_pidinfo_empty (nsfw_pidinfo * pidinfo)
{
  u32 i;
  for (i = 0; i < pidinfo->used_size && i < NSFW_MAX_FORK_NUM; i++)
    {
      if (pidinfo->apid[i] != 0)
        {
          return 0;
        }
    }
  return 1;
}
