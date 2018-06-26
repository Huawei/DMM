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

#include "nstack_share_res.h"
#include "nsfw_mem_api.h"
#include "types.h"
#include "nsfw_recycle_api.h"
#include "nstack_securec.h"
#include "nstack_log.h"
#include "nsfw_maintain_api.h"
#include "nstack_types.h"

#define NSTACK_SHARE_FORK_LOCK "share_fork_lock"

typedef struct
{
  common_mem_spinlock_t *fork_share_lock;
} nstack_share_res;

NSTACK_STATIC nstack_share_res g_nstack_share_res;

/** global timer tick */
nstack_tick_info_t g_nstack_timer_tick;

NSTACK_STATIC int
nstack_create_share_fork_lock ()
{
  mzone_handle zone;
  nsfw_mem_zone param;
  int ret;

  param.isocket_id = -1;
  param.length = sizeof (common_mem_spinlock_t);
  param.stname.entype = NSFW_SHMEM;

  ret =
    STRCPY_S (param.stname.aname, NSFW_MEM_NAME_LENGTH,
              NSTACK_SHARE_FORK_LOCK);
  if (EOK != ret)
    {
      NSSOC_LOGERR ("STRCPY_S failed]name=%s,ret=%d", NSTACK_SHARE_FORK_LOCK,
                    ret);
      return -1;
    }

  zone = nsfw_mem_zone_create (&param);
  if (!zone)
    {
      NSSOC_LOGERR ("nsfw_mem_zone_create failed]name=%s",
                    NSTACK_SHARE_FORK_LOCK);
      return -1;
    }

  g_nstack_share_res.fork_share_lock = (common_mem_spinlock_t *) zone;
  common_mem_spinlock_init (g_nstack_share_res.fork_share_lock);

  NSSOC_LOGDBG ("ok");
  return 0;
}

NSTACK_STATIC int
nstack_lookup_share_fork_lock ()
{
  mzone_handle zone;
  nsfw_mem_name param;

  param.entype = NSFW_SHMEM;
  param.enowner = NSFW_PROC_MAIN;
  if (STRCPY_S (param.aname, NSFW_MEM_NAME_LENGTH, NSTACK_SHARE_FORK_LOCK) !=
      0)
    {
      NSSOC_LOGERR ("STRCPY_S failed]name=%s", NSTACK_SHARE_FORK_LOCK);
      return -1;
    }

  zone = nsfw_mem_zone_lookup (&param);
  if (!zone)
    {
      NSSOC_LOGERR ("nsfw_mem_zone_lookup failed]name=%s",
                    NSTACK_SHARE_FORK_LOCK);
      return -1;
    }

  g_nstack_share_res.fork_share_lock = (common_mem_spinlock_t *) zone;

  NSSOC_LOGDBG ("ok");

  return 0;
}

NSTACK_STATIC int
nstack_lookup_share_global_tick ()
{
  int ret;
  nsfw_mem_name name = {.entype = NSFW_SHMEM,.enowner = NSFW_PROC_MAIN };

  ret = STRCPY_S (name.aname, NSFW_MEM_NAME_LENGTH, NSTACK_GLOBAL_TICK_SHM);
  if (EOK != ret)
    {
      NSSOC_LOGERR ("STRCPY_S failed]name=%s,ret=%d", NSTACK_GLOBAL_TICK_SHM,
                    ret);
      return -1;
    }

  g_nstack_timer_tick.tick_ptr = (u64_t *) nsfw_mem_zone_lookup (&name);
  if (NULL == g_nstack_timer_tick.tick_ptr)
    {
      NSPOL_LOGERR ("Failed to lookup global timer tick memory");
      return -1;
    }

  NSSOC_LOGDBG ("ok");
  return 0;
}

int
nstack_init_share_res ()
{
  if (nstack_create_share_fork_lock () != 0)
    {
      return -1;
    }

  return 0;
}

int
nstack_attach_share_res ()
{
  if (nstack_lookup_share_fork_lock () != 0)
    {
      return -1;
    }

  if (nstack_lookup_share_global_tick () != 0)
    {
      return -1;
    }

  return 0;
}

common_mem_spinlock_t *
nstack_get_fork_share_lock ()
{
  return g_nstack_share_res.fork_share_lock;
}

NSTACK_STATIC nsfw_rcc_stat
nstack_recycle_fork_share_lock (u32 exit_pid, void *pdata, u16 rec_type)
{
  NSSOC_LOGDBG ("recycle]pid=%u", exit_pid);

  if (g_nstack_share_res.fork_share_lock
      && (g_nstack_share_res.fork_share_lock->locked == exit_pid))
    {
      common_mem_spinlock_unlock (g_nstack_share_res.fork_share_lock);
    }

  return NSFW_RCC_CONTINUE;
}

REGIST_RECYCLE_LOCK_REL (nstack_recycle_fork_share_lock, NULL, NSFW_PROC_APP)
