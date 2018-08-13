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

#include <stdlib.h>
#include "types.h"
#include "nstack_securec.h"
#include "nsfw_init.h"
#include "nstack_log.h"
#include "nsfw_maintain_api.h"
#include "nsfw_mem_api.h"
#include "nsfw_rti.h"
#include "nsfw_msg.h"
#ifdef HAL_LIB
#else
#include "common_pal_bitwide_adjust.h"
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

char g_dfx_switch = 1;

struct rti_queue *g_nsfw_rti_primary_stat = NULL;

void
nsfw_rti_stat (nsfw_rti_stat_type_t statType, const data_com_msg * m)
{
  if (!g_nsfw_rti_primary_stat || !m)
    {
      return;
    }

  struct rti_queue *primary_stat = ADDR_SHTOL (g_nsfw_rti_primary_stat);

  switch (statType)
    {
    case NSFW_STAT_PRIMARY_DEQ:
      if ((m->param.major_type >= MAX_MAJOR_TYPE)
          || (m->param.minor_type >= MAX_MINOR_TYPE))
        {
          return;
        }
      /*call_msg_fun() is only called in nStackMain, no reentrance risk, ++ operation is ok */
      primary_stat->tcpip_msg_deq[m->param.major_type]++;
      if (0 == m->param.major_type)     //SPL_TCPIP_NEW_MSG_API
        {
          primary_stat->api_msg_deq[m->param.minor_type]++;
        }
      break;
    case NSFW_STAT_PRIMARY_ENQ_FAIL:
      if ((m->param.major_type >= MAX_MAJOR_TYPE)
          || (m->param.minor_type >= MAX_MINOR_TYPE))
        {
          return;
        }
      __sync_fetch_and_add (&primary_stat->tcpip_msg_enq_fail
                            [m->param.major_type], 1);
      if (0 == m->param.major_type)     //SPL_TCPIP_NEW_MSG_API
        {
          __sync_fetch_and_add (&primary_stat->api_msg_enq_fail
                                [m->param.minor_type], 1);
        }
      break;
    case NSFW_STAT_PRIMARY_ENQ:
      //not use
      break;
    default:
      break;
    }
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
