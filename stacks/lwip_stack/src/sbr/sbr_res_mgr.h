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

#ifndef SBR_RES_MGR_H
#define SBR_RES_MGR_H
#include "sbr_protocol_api.h"
#include "sbr_index_ring.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef struct
{
  sbr_index_ring *sk_ring;
  sbr_socket_t sk[SBR_MAX_FD_NUM + 1];  /* unuse index 0 */
} sbr_res_group;

extern sbr_res_group g_res_group;

/*****************************************************************************
*   Prototype    : sbr_malloc_sk
*   Description  : malloc sock
*   Input        : None
*   Output       : None
*   Return Value : static inline sbr_socket_t *
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline sbr_socket_t *
sbr_malloc_sk ()
{
  int fd;

  if (sbr_index_ring_dequeue (g_res_group.sk_ring, &fd) != 1)
    {
      NSSBR_LOGERR ("malloc sk failed]");
      sbr_set_errno (EMFILE);
      return NULL;
    }

  NSSBR_LOGDBG ("malloc sk ok]fd=%d", fd);
  return &g_res_group.sk[fd];
}

/*****************************************************************************
*   Prototype    : sbr_free_sk
*   Description  : free sock
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
sbr_free_sk (sbr_socket_t * sk)
{
  sk->fdopt = NULL;
  sk->sk_obj = NULL;
  sk->stack_obj = NULL;

  if (sbr_index_ring_enqueue (g_res_group.sk_ring, sk->fd) != 1)
    {
      NSSBR_LOGERR ("sbr_index_ring_enqueue failed, this can not happen");
    }

  NSSBR_LOGDBG ("free sk ok]fd=%d", sk->fd);
}

/*****************************************************************************
*   Prototype    : sbr_lookup_sk
*   Description  : lookup socket
*   Input        : int fd
*   Output       : None
*   Return Value : static inline sbr_socket_t *
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline sbr_socket_t *
sbr_lookup_sk (int fd)
{
  if ((fd < 1) || (fd > SBR_MAX_FD_NUM))
    {
      NSSBR_LOGERR ("fd is not ok]fd=%d", fd);
      sbr_set_errno (EBADF);
      return NULL;
    }

  sbr_socket_t *sk = &g_res_group.sk[fd];
  if (!sk->sk_obj || !sk->stack_obj)
    {
      NSSBR_LOGERR
        ("data in sk is error, this can not happen]fd=%d,sk_obj=%p,stack_obj=%p",
         fd, sk->sk_obj, sk->stack_obj);
      sbr_set_errno (EBADF);
      return NULL;
    }

  return sk;
}

/*****************************************************************************
*   Prototype    : sbr_free_sk
*   Description  : free sock
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline void
sbr_free_fd (int fd)
{
  if ((fd < 1) || (fd > SBR_MAX_FD_NUM))
    {
      NSSBR_LOGERR ("fd is not ok]fd=%d", fd);
      sbr_set_errno (EBADF);
      return;
    }

  sbr_socket_t *sk = &g_res_group.sk[fd];
  if (!sk->fdopt && !sk->sk_obj && !sk->stack_obj)
    {
      NSSBR_LOGERR
        ("can't free empty fd] fd=%d, fdopt=%p, sk_obj=%p, stack_obj=%p", fd,
         sk->fdopt, sk->sk_obj, sk->stack_obj);
      return;
    }
  sbr_free_sk (sk);
}

int sbr_init_res ();

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
