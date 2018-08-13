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

#ifndef MSG_API_H
#define MSG_API_H
#include "nsfw_msg.h"
#include "nsfw_mem_api.h"
#include "nstack_log.h"
#include "nsfw_rti.h"
#include "common_mem_api.h"
#include "nsfw_recycle_api.h"
#include "common_pal_bitwide_adjust.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define SET_MSG_ERR(m, error) ((m)->param.err = (error))
#define GET_MSG_ERR(m) ((m)->param.err)

/* for sync message from sbr we should signal sem */
#define SYNC_MSG_ACK(m) sys_sem_s_signal(&((m)->param.op_completed))

/* for async message from sbr we should free the message */
#define ASYNC_MSG_FREE(m) msg_free(m)

#define MSG_ENTRY(_ptr, _type, _member) container_of((void *)_ptr, _type, _member)

#ifndef NSTACK_STATIC_CHECK
/*****************************************************************************
*   Prototype    : msg_malloc
*   Description  : malloc msg
*   Input        : mring_handle mhandle
*   Output       : None
*   Return Value : static inline data_com_msg*
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline data_com_msg *
msg_malloc (mring_handle mhandle)
{
  if (!mhandle)
    {
      NSFW_LOGERR ("mhandle is null");
      return NULL;
    }

  data_com_msg *m = NULL;
  if (nsfw_mem_ring_dequeue (mhandle, (void **) &m) != 1)
    {
      return NULL;
    }

  m->param.recycle_pid = get_sys_pid ();
  res_alloc (&m->param.res_chk);
  return m;
}

/*****************************************************************************
*   Prototype    : msg_free
*   Description  : free msg
*   Input        : data_com_msg* m
*                  mring_handle mhandle
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline void
msg_free (data_com_msg * m)
{
  if (!m)
    {
      NSFW_LOGERR ("m is NULL");
      return;
    }

  mring_handle mhandle = ADDR_SHTOL (m->param.msg_from);
  if (!mhandle)
    {
      return;
    }

  if (res_free (&m->param.res_chk))
    {
      NSFW_LOGERR ("m refree!]m=%p", m);
      return;
    }

  m->param.recycle_pid = 0;

  if (nsfw_mem_ring_enqueue (mhandle, (void *) m) != 1)
    {
      NSFW_LOGERR ("nsfw_mem_ring_enqueue failed,this can not happen");
    }
}

/*****************************************************************************
*   Prototype    : msg_post
*   Description  : post msg
*   Input        : data_com_msg* m
*                  mring_handle mhandle
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline int
msg_post (data_com_msg * m, mring_handle mhandle)
{
  int ret;
  if (!m || !mhandle)
    {
      NSFW_LOGERR ("param is not ok]m=%p,mhandle=%p", m, mhandle);
      return -1;
    }

  while (1)
    {
      ret = nsfw_mem_ring_enqueue (mhandle, (void *) m);
      switch (ret)
        {
        case 1:
          if (MSG_SYN_POST == m->param.op_type)
            {
              sys_arch_sem_s_wait (&m->param.op_completed, 0);
            }

          return 0;
        case 0:
          continue;
        default:
          nsfw_rti_stat_macro (NSFW_STAT_PRIMARY_ENQ_FAIL, m);
          return -1;
        }
    }
}

#define MSG_POST_FAILED 50
/*****************************************************************************
*   Prototype    : msg_post_with_lock_rel
*   Description  : post msg to tcpip thread in mgrcom thread
*   Input        : data_com_msg* m
*                  mring_handle mhandle
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline int
msg_post_with_lock_rel (data_com_msg * m, mring_handle mhandle)
{
  int ret;
  int try_count = 0;
  if (!m || !mhandle)
    {
      NSFW_LOGERR ("param is not ok]m=%p,mhandle=%p", m, mhandle);
      return -1;
    }

  while (1)
    {
      ret = nsfw_mem_ring_enqueue (mhandle, (void *) m);
      switch (ret)
        {
        case 1:
          if (MSG_SYN_POST == m->param.op_type)
            {
              sys_arch_sem_s_wait (&m->param.op_completed, 0);
            }

          return 0;
        case 0:
          try_count++;
          if (try_count > MSG_POST_FAILED)
            {
              try_count = 0;
              nsfw_recycle_rechk_lock ();
            }
          sys_sleep_ns (0, 1000000);
          continue;
        default:
          nsfw_rti_stat_macro (NSFW_STAT_PRIMARY_ENQ_FAIL, m);
          return -1;
        }
    }
}

/*****************************************************************************
*   Prototype    : msg_try_post
*   Description  : try post msg
*   Input        : data_com_msg* m
*                  mring_handle mhandle
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline int
msg_try_post (data_com_msg * m, mring_handle mhandle)
{
  if (!m || !mhandle)
    {
      NSFW_LOGERR ("param is not ok]m=%p,mhandle=%p", m, mhandle);
      return -1;
    }

  int ret = nsfw_mem_ring_enqueue (mhandle, (void *) m);
  if (1 == ret)
    {
      if (MSG_SYN_POST == m->param.op_type)
        {
          sys_arch_sem_s_wait (&m->param.op_completed, 0);
        }

      return 0;
    }

  return -1;
}

/*****************************************************************************
*   Prototype    : msg_fetch
*   Description  : fetch msg
*   Input        : mring_handle mhandle
*                  data_com_msg** m
*                  u32 num
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline int
msg_fetch (mring_handle mhandle, data_com_msg ** m, u32 num)
{
  if (!m || !mhandle)
    {
      NSFW_LOGERR ("param is not ok]m=%p,mhandle=%p", m, mhandle);
      return -1;
    }

  int ret;
  while (1)
    {
      ret = nsfw_mem_ring_dequeuev (mhandle, (void *) m, num);
      if (ret > 0)
        {
          break;
        }
    }

  return ret;
}

/*****************************************************************************
*   Prototype    : msg_try_fetch
*   Description  : try fetch msg
*   Input        : mring_handle mhandle
*                  data_com_msg** m
*                  u32 num
*   Output       : None
*   Return Value : static inline int
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline int
msg_try_fetch (mring_handle mhandle, data_com_msg ** m, u32 num)
{
  if (!m || !mhandle)
    {
      NSFW_LOGERR ("param is not ok]m=%p,mhandle=%p", m, mhandle);
      return -1;
    }

  return nsfw_mem_ring_dequeuev (mhandle, (void *) m, num);
}

#else
data_com_msg *msg_malloc (mring_handle mhandle);
void msg_free (data_com_msg * m);
int msg_post (data_com_msg * m, mring_handle mhandle);
int msg_try_post (data_com_msg * m, mring_handle mhandle);
int msg_fetch (mring_handle mhandle, data_com_msg ** m, u32 num);
int msg_try_fetch (mring_handle mhandle, data_com_msg ** m, u32 num);
int msg_post_with_lock_rel (data_com_msg * m, mring_handle mhandle);
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
