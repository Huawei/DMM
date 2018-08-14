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

#include "nsfw_mem_api.h"
#include "nstack_share_res.h"
#include "nstack_securec.h"

char *g_nstack_ver_info = NULL;

int
init_unmatch_version (void)
{
  int ret;
  nsfw_mem_zone mzone;

  ret =
    STRCPY_S (mzone.stname.aname, NSFW_MEM_NAME_LENGTH, NSTACK_VERSION_SHM);
  if (EOK != ret)
    {
      NSPOL_LOGERR ("STRCPY_S fail]ret=%d", ret);
      return -1;
    }
  mzone.stname.entype = NSFW_SHMEM;
  mzone.isocket_id = -1;
  mzone.length =
    NSTACK_VERSION_LEN + MAX_UNMATCH_VER_CNT * sizeof (unmatch_ver_info_t);
  mzone.ireserv = 0;

  char *version = (char *) nsfw_mem_zone_create (&mzone);
  if (NULL == version)
    {
      NSPOL_LOGERR ("Failed to create unmatch_version memory");
      return -1;
    }

  ret = STRCPY_S (version, NSTACK_VERSION_LEN, NSTACK_VERSION);
  if (EOK != ret)
    {
      NSPOL_LOGERR ("STRCPY_S NSTACK_VERSION fail]ret=%d", ret);
      return -1;
    }

  g_nstack_ver_info = version;

  return 0;
}
