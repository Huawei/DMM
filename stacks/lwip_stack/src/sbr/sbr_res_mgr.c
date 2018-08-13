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

#include "nstack_securec.h"
#include "sbr_res_mgr.h"

sbr_res_group g_res_group = { };

/*****************************************************************************
*   Prototype    : sbr_init_sk
*   Description  : init sock pool
*   Input        : None
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
NSTACK_STATIC int
sbr_init_sk ()
{
  sbr_index_ring *ring = sbr_create_index_ring (SBR_MAX_FD_NUM - 1);

  if (!ring)
    {
      NSSBR_LOGERR ("init ring failed");
      return -1;
    }

  int i;
  /*the queue can't accept value=0, so i begin with 1 */
  for (i = 1; i <= SBR_MAX_FD_NUM; ++i)
    {
      g_res_group.sk[i].fd = i;
      if (sbr_index_ring_enqueue (ring, i) != 1)
        {
          NSSBR_LOGERR ("sbr_index_ring_enqueue failed, this can not happen");
          free (ring);
          return -1;
        }
    }

  g_res_group.sk_ring = ring;
  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_init_res
*   Description  : init sbr res
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_init_res ()
{
  if (sbr_init_sk () != 0)
    {
      return -1;
    }

  NSSBR_LOGDBG ("init socket ok");

  if (sbr_init_protocol () != 0)
    {
      return -1;
    }

  NSSBR_LOGDBG ("init protocol ok");

  return 0;
}
