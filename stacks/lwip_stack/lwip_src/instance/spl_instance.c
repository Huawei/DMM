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

#include "nsfw_msg_api.h"
#include "nsfw_init.h"
#include "stackx/spl_sbr.h"
#include "stackx/stackx_instance.h"

stackx_instance *p_def_stack_instance = NULL;

/**
 * process message from other module, but the MT module message will be delayed
 * to handle in the end of the loop to avoid to lose the message dequeued out.
 *
 * @param m the data_com_msg to handle
 */
int
spl_process (data_com_msg * m)
{
  return call_msg_fun (m);
}

void
add_disp_netif (struct netif *netif)
{
  struct disp_netif_list *item = malloc (sizeof (struct disp_netif_list));
  if (!item)
    {
      NSPOL_LOGERR ("malloc failed");
      return;
    }

  item->netif = netif;
  item->next = p_def_stack_instance->netif_list;
  p_def_stack_instance->netif_list = item;
}
