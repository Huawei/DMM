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

#include "nsfw_init.h"
//#include "sockets.h"
//#include <netinet/in.h>

#include "stackx_spl_share.h"
#include "stackx/spl_api.h"
//#include "stackx/ip.h"
#include "sharedmemory.h"
#include "spl_hal.h"
#include "hal_api.h"
#include "alarm_api.h"
#include "nsfw_mt_config.h"
#include "nsfw_recycle_api.h"
#include "nstack_dmm_adpt.h"

/* main entry for stackx */
int
spl_main_init (void *args)
{
  (void) args;
  if (init_by_main_thread () < 0)
    {
      return -1;
    }

  return 0;
}

/* *INDENT-OFF* */
NSFW_MODULE_NAME ("STACKX_MAIN")
NSFW_MODULE_PRIORITY (10)
NSFW_MODULE_DEPENDS (NSFW_ALARM_MODULE)
NSFW_MODULE_DEPENDS (NSTACK_DMM_MODULE)
NSFW_MODULE_INIT (spl_main_init)
/* *INDENT-ON* */
