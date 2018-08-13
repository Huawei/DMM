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

#include "stackxopts.h"
#include "nstack_log.h"
#include "stackx_tx_box.h"
#include "nsfw_mt_config.h"

#define MZ_STACKX_RING_NAME "VppTCP_Ring"
#define MZ_STACKX_PRIORITY_RING_NAME "VppTCP_Priority%u_Ring"

/*
 * Given the stackx ring name template above, get the queue name
 */
const char *
get_stackx_ring_name ()
{
  /* buffer for return value. Size calculated by %u being replaced
   * by maximum 3 digits (plus an extra byte for safety) */
  static char rbuffer[sizeof (MZ_STACKX_RING_NAME) + 16];

  int retVal =
    spl_snprintf (rbuffer, sizeof (rbuffer) - 1, MZ_STACKX_RING_NAME);

  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return rbuffer;
}

const char *
get_stackx_priority_ring_name (unsigned priority)
{
  /* buffer for return value. Size calculated by %u being replaced
   * by maximum 3 digits (plus an extra byte for safety) */
  static char prbuffer[sizeof (MZ_STACKX_PRIORITY_RING_NAME) + 32];

  int retVal = spl_snprintf (prbuffer, sizeof (prbuffer) - 1,
                             MZ_STACKX_PRIORITY_RING_NAME, priority);

  if (-1 == retVal)
    {
      NSPOL_LOGERR ("spl_snprintf failed]");
      return NULL;
    }

  return prbuffer;
}
