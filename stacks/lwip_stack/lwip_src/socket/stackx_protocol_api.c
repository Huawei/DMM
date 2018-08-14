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

#include "sbr_protocol_api.h"
#include "stackx_epoll_api.h"
#include "stackx_socket.h"
#include "stackx_res_mgr.h"
#include "stackx_common_opt.h"
#include "common_mem_api.h"
#include "stackx_event.h"

extern sbr_fdopt tcp_fdopt;
extern sbr_fdopt udp_fdopt;
extern sbr_fdopt raw_fdopt;
extern sbr_fdopt custom_fdopt;

/*****************************************************************************
*   Prototype    : sbr_init_protocol
*   Description  : init protocol
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_init_protocol ()
{
  return sbr_init_stackx ();
}

/*****************************************************************************
*   Prototype    : sbr_get_fdopt
*   Description  : get fdopt by domain type protocol
*   Input        : int domain
*                  int type
*                  int protocol
*   Output       : None
*   Return Value : sbr_fdopt*
*   Calls        :
*   Called By    :
*
*****************************************************************************/
sbr_fdopt *
sbr_get_fdopt (int domain, int type, int protocol)
{
  if (domain != AF_INET)
    {
      NSSBR_LOGERR ("domain is not AF_INET]domain=%d", domain);
      sbr_set_errno (EAFNOSUPPORT);
      return NULL;
    }

  sbr_fdopt *fdopt = NULL;

  switch (type & (~(O_NONBLOCK | SOCK_CLOEXEC)))
    {
    case SOCK_DGRAM:
      fdopt = &udp_fdopt;
      break;
    case SOCK_STREAM:
      fdopt = &tcp_fdopt;
      break;

    default:
      NSSBR_LOGDBG ("type is unknown]type=%d", type);
      sbr_set_errno (ESOCKTNOSUPPORT);
      return NULL;
    }

  return fdopt;
}

/* app send its version info to nStackMain */
extern void sbr_handle_app_touch (void);
void
sbr_app_touch_in (void)
{
  sbr_handle_app_touch ();
}
