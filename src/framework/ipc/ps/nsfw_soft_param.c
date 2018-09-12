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

/*==============================================*
 *      include header files                    *
 *----------------------------------------------*/

#include <stdlib.h>
#include "types.h"
#include "nstack_securec.h"
#include "nsfw_init.h"

#include "nstack_log.h"
#include "nsfw_maintain_api.h"

#include "nsfw_mem_api.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

typedef struct _nsfw_set_soft_item
{
  void *data;
  u32 size;
  u64 max;
  u64 min;
} nsfw_set_soft_item;

/* *INDENT-OFF* */
nsfw_set_soft_fun g_soft_fun[NSFW_MAX_SOFT_PARAM] = { 0 };
nsfw_set_soft_item g_soft_int_cfg[NSFW_MAX_SOFT_PARAM];
/* *INDENT-ON* */

int
nsfw_isdigitstr (const char *str)
{
  if (NULL == str || '\0' == str[0])
    return 0;

  while (*str)
    {
      if (*str < '0' || *str > '9')
        return 0;
      str++;
    }
  return 1;
}

u8
nsfw_soft_param_reg_fun (u32 param_name, nsfw_set_soft_fun fun)
{
  if (NULL == fun || param_name >= NSFW_MAX_SOFT_PARAM)
    {
      NSFW_LOGERR ("argv err]fun=%p,type=%u", fun, param_name);
      return FALSE;
    }

  g_soft_fun[param_name] = fun;
  NSFW_LOGINF ("reg]fun=%d,type=%u", fun, param_name);
  return TRUE;
}

int
nsfw_soft_set_int (u32 param, char *buf, u32 buf_len)
{
  u64 buf_value = 0;
  if (NULL == buf || param >= NSFW_MAX_SOFT_PARAM)
    {
      NSFW_LOGERR ("argv err]buf=%p,param=%u", buf, param);
      return FALSE;
    }

  if (!nsfw_isdigitstr (buf))
    {
      NSFW_LOGERR ("argv err]buf=%s,param=%u", buf, param);
      return FALSE;
    }

  char *parsing_end;
  buf_value = (u64) strtol (buf, &parsing_end, 10);
  nsfw_set_soft_item *int_item = &g_soft_int_cfg[param];
  if (NULL == int_item->data)
    {
      NSFW_LOGERR ("data err]buf=%s,param=%u,min=%llu,max=%llu", buf, param,
                   int_item->min, int_item->max);
      return FALSE;
    }

  if (buf_value < int_item->min || buf_value > int_item->max)
    {
      NSFW_LOGERR ("argv err]buf=%s,param=%u,min=%llu,max=%llu", buf, param,
                   int_item->min, int_item->max);
      return FALSE;
    }

  u32 size = int_item->size;
  if (size >= sizeof (u64))
    {
      size = sizeof (u64);
    }

  if (EOK != MEMCPY_S (int_item->data, size, &buf_value, size))
    {
      NSFW_LOGERR ("MEMCPY_S failed");
      return FALSE;
    }

  NSFW_LOGINF ("set soft param ok]param=%u,value=%llu,size=%u", param,
               buf_value, size);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_soft_param_reg_int
*   Description  : reg int param set
*   Input        : u32 param_name
*                  u32 size
*                  u32 min
*                  u32 max
*                  u64 *data
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_soft_param_reg_int (u32 param_name, u32 size, u32 min, u32 max,
                         u64 * data)
{
  if (NULL == data || param_name >= NSFW_MAX_SOFT_PARAM)
    {
      NSFW_LOGERR ("argv err]data=%p,type=%u", data, param_name);
      return FALSE;
    }

  g_soft_int_cfg[param_name].data = data;
  g_soft_int_cfg[param_name].size = size;
  g_soft_int_cfg[param_name].max = max;
  g_soft_int_cfg[param_name].min = min;
  return nsfw_soft_param_reg_fun (param_name, nsfw_soft_set_int);
}

void
nsfw_set_soft_para (fw_poc_type proc_type, u32 para_name, void *value,
                    u32 size)
{
  nsfw_mgr_msg *msg =
    (nsfw_mgr_msg *) nsfw_mgr_msg_alloc (MGR_MSG_SOF_PAR_REQ, proc_type);
  if (NULL == msg)
    {
      NSFW_LOGERR
        ("nsfw_mgr_msg_alloc failed] msg type=%d, proc type=%d, para_name=%u",
         MGR_MSG_SOF_PAR_REQ, proc_type, para_name);
      return;
    }

  nsfw_soft_param_msg *soft_msg = GET_USER_MSG (nsfw_soft_param_msg, msg);

  soft_msg->param_name = para_name;
  soft_msg->rsp_code = 0;

  if (EOK !=
      MEMCPY_S (soft_msg->param_value, sizeof (soft_msg->param_value),
                value, size))
    {
      NSFW_LOGERR
        ("MEMCPY_S failed] msg type=%d, proc type=%d, para_name=%u",
         MGR_MSG_SOF_PAR_REQ, proc_type, para_name);
      nsfw_mgr_msg_free (msg);
      return;
    }

  nsfw_mgr_msg *rsp = nsfw_mgr_null_rspmsg_alloc ();
  if (NULL == rsp)
    {
      NSFW_LOGERR
        ("nsfw_mgr_null_rspmsg_alloc failed] msg type=%d, proc type=%d, para_name=%u",
         MGR_MSG_SOF_PAR_REQ, proc_type, para_name);
      nsfw_mgr_msg_free (msg);
      return;
    }

  if (!nsfw_mgr_send_req_wait_rsp (msg, rsp))
    {
      NSFW_LOGERR
        ("can not get response] msg type=%d, proc type=%d, para_name=%u",
         MGR_MSG_SOF_PAR_REQ, proc_type, para_name);
      nsfw_mgr_msg_free (msg);
      nsfw_mgr_msg_free (rsp);
      return;
    }

  nsfw_soft_param_msg *soft_rsp_msg = GET_USER_MSG (nsfw_soft_param_msg, rsp);
  if (soft_rsp_msg->rsp_code != NSFW_EXIT_SUCCESS)
    {
      NSFW_LOGERR
        ("set soft param failed] msg type=%d, proc type=%d, para_name=%u, rsp code=%u",
         MGR_MSG_SOF_PAR_REQ, proc_type, para_name, soft_rsp_msg->rsp_code);
      nsfw_mgr_msg_free (msg);
      nsfw_mgr_msg_free (rsp);
      return;
    }

  NSFW_LOGINF
    ("set soft param success] msg type=%d, proc type=%d, para_name=%u",
     MGR_MSG_SOF_PAR_REQ, proc_type, para_name);

  nsfw_mgr_msg_free (msg);
  nsfw_mgr_msg_free (rsp);
  return;
}

int
nsfw_softparam_msg_proc (nsfw_mgr_msg * msg)
{
  nsfw_mgr_msg *rsp_msg = nsfw_mgr_rsp_msg_alloc (msg);
  if (NULL == rsp_msg)
    {
      NSFW_LOGERR ("alloc rsp failed,drop msg!" MSGINFO, PRTMSG (msg));
      return FALSE;
    }

  nsfw_soft_param_msg *soft_msg = GET_USER_MSG (nsfw_soft_param_msg, msg);
  nsfw_soft_param_msg *soft_rsp_msg =
    GET_USER_MSG (nsfw_soft_param_msg, rsp_msg);
  if ((soft_msg->param_name < NSFW_MAX_SOFT_PARAM)
      && (NULL != g_soft_fun[soft_msg->param_name]))
    {
      soft_msg->param_value[sizeof (soft_msg->param_value) - 1] = 0;
      (void) g_soft_fun[soft_msg->param_name] (soft_msg->param_name,
                                               (char *)
                                               soft_msg->param_value,
                                               sizeof
                                               (soft_msg->param_value));
      soft_rsp_msg->rsp_code = NSFW_EXIT_SUCCESS;
    }
  else
    {
      NSFW_LOGERR ("set soft failed!]soft=%u", soft_msg->param_name);
      soft_rsp_msg->rsp_code = NSFW_EXIT_FAILED;
    }

  (void) nsfw_mgr_send_msg (rsp_msg);
  nsfw_mgr_msg_free (rsp_msg);
  return TRUE;
}

int nsfw_softparam_module_init (void *param);
int
nsfw_softparam_module_init (void *param)
{
  u8 proc_type = (u8) ((long long) param);
  NSFW_LOGINF ("softparam module init]type=%u", proc_type);
  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
      (void) nsfw_mgr_reg_msg_fun (MGR_MSG_SOF_PAR_REQ,
                                   nsfw_softparam_msg_proc);
      return 0;
    default:
      if (proc_type < NSFW_PROC_MAX)
        {
          break;
        }
      return -1;
    }

  return 0;
}

/* *INDENT-OFF* */
NSFW_MODULE_NAME (NSFW_SOFT_PARAM_MODULE)
NSFW_MODULE_PRIORITY (99)
NSFW_MODULE_INIT (nsfw_softparam_module_init)
/* *INDENT-ON* */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
