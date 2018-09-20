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

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

typedef struct _nsfw_log_cfg
{
  u8 proc_type;
  char master_log_path[NSFW_LOG_VALUE_LEN];
  char runing_log_path[NSFW_LOG_VALUE_LEN];
  char opr_log_path[NSFW_LOG_VALUE_LEN];
} nsfw_log_cfg;

nsfw_log_cfg g_log_cfg;

const char nsfw_mas_log[NSFW_LOG_VALUE_LEN] = "maspath";
const char nsfw_flush_name[NSFW_LOG_VALUE_LEN] = "flush";

enum _set_log_type
{
  SET_LOG_LEVEL = 0,
  SET_LOG_FLUSH,
  SET_LOG_MASTER,
  SET_LOG_MAIN,

  SET_LOG_INVALID
};

/*****************************************************************************
*   Prototype    : nsfw_set_log_path
*   Description  : set log path
*   Input        : const char *param
*                  const char *value
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_set_log_path (const char *param, const char *value)
{
  if (NULL == param || NULL == value)
    {
      NSFW_LOGERR ("log param error!]param=%p,value=%p", param, value);
      return FALSE;
    }

  if (cmp_log_path (value))
    {
      if (check_log_dir_valid (value) < 0)
        {
          NSFW_LOGERR ("path is invalid]value=%s", value);
          return FALSE;
        }
      if (EOK !=
          STRCPY_S (g_log_cfg.master_log_path, NSFW_LOG_VALUE_LEN, value))
        {
          NSFW_LOGERR ("strcpy error!");
          return FALSE;
        }

      NSFW_LOGINF ("renew log path]%s", g_log_cfg.master_log_path);
      nstack_modify_log_dir (g_log_cfg.master_log_path);
      NSFW_LOGINF ("set log success]newpath=%s!", g_log_cfg.master_log_path);
      return TRUE;
    }
  return FALSE;
}

/*****************************************************************************
*   Prototype    : nsfw_flush_log_info
*   Description  : flush the log info
*   Input        : const char *param
*                  u8 proc_type
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_flush_log_info (const char *param, u8 proc_type)
{
  if (NULL == param)
    {
      NSFW_LOGERR ("log param error!]param=%p", param);
      return FALSE;
    }
  glogFlushLogFiles (GLOG_LEVEL_DEBUG);
  NSFW_LOGINF ("flush log success]proc_type=%u", proc_type);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_set_log_msg_proc
*   Description  : set log message process
*   Input        : nsfw_mgr_msg* msg
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_set_log_msg_proc (nsfw_mgr_msg * msg)
{
  int ret = -1;
  int status = -1;
  if (NULL == msg)
    {
      NSFW_LOGERR ("msg nul");
      return FALSE;
    }

  nsfw_set_log_msg *set_log_msg = GET_USER_MSG (nsfw_set_log_msg, msg);

  if (0 == strcmp (set_log_msg->module, nsfw_mas_log))
    {
      status = SET_LOG_MASTER;
    }
  else if (0 == strcmp (set_log_msg->module, nsfw_flush_name))
    {
      status = SET_LOG_FLUSH;
    }
  else if (nsfw_isdigitstr (set_log_msg->module))
    {
      status = SET_LOG_LEVEL;
    }

  switch (status)
    {
    case SET_LOG_LEVEL:
      ret = setlog_level_value (set_log_msg->module, set_log_msg->log_level);
      break;
    case SET_LOG_FLUSH:
      ret = nsfw_flush_log_info (set_log_msg->module, msg->dst_proc_type);
      break;
    case SET_LOG_MASTER:
      ret = nsfw_set_log_path (set_log_msg->module, set_log_msg->log_level);
      break;
    default:
      NSFW_LOGERR ("default error]status=%d,set_log_msg->module=%s", status,
                   set_log_msg->module);
      return FALSE;
    }

  nsfw_mgr_msg *rsp_msg = nsfw_mgr_rsp_msg_alloc (msg);
  if (NULL == rsp_msg)
    {
      NSFW_LOGERR ("alloc rsp failed,drop msg!" MSGINFO, PRTMSG (msg));
      return FALSE;
    }
  nsfw_set_log_msg *log_rsp_msg = GET_USER_MSG (nsfw_set_log_msg, rsp_msg);
  log_rsp_msg->rsp_code = ret;
  (void) nsfw_mgr_send_msg (rsp_msg);
  nsfw_mgr_msg_free (rsp_msg);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_cfg_module_init
*   Description  : module init
*   Input        : void* param
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*****************************************************************************/
static int nsfw_cfg_module_init (void *param);
static int
nsfw_cfg_module_init (void *param)
{
  u8 proc_type = (u8) ((long long) param);
  NSFW_LOGINF ("log cfg module init]type=%u", proc_type);
  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
      (void) nsfw_mgr_reg_msg_fun (MGR_MSG_SET_LOG_REQ,
                                   nsfw_set_log_msg_proc);
      g_log_cfg.proc_type = proc_type;
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
NSFW_MODULE_NAME (NSFW_LOG_CFG_MODULE)
NSFW_MODULE_PRIORITY (99)
NSFW_MODULE_INIT (nsfw_cfg_module_init)
/* *INDENT-ON* */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
