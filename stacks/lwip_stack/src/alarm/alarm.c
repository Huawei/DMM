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

#include "types.h"
#include "nsfw_mgr_com_api.h"
#include "nsfw_fd_timer_api.h"
#include "alarm.h"
#include "alarm_api.h"
#include<time.h>
#include<sys/time.h>
#include "nsfw_init.h"
#include "nstack_log.h"
#include "json.h"
#include "nstack_securec.h"
#include <stdlib.h>
#include "nstack_dmm_adpt.h"

char g_vmid[MAX_VMID_LEN + 1] = "agent-node-x";

extern nsfw_timer_info *nsfw_timer_reg_timer (u32 timer_type, void *data,
                                              nsfw_timer_proc_fun fun,
                                              struct timespec time_left);

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif /* __cplusplus */

// note:the first element is reserved
alarm_data g_alarm_data[ALARM_EVENT_MAX] = { };

alarm_result
ms_alarm_check_func (void *para)
{
  alarm_result ret_alarm;
  int para_value = (long) para;

  ret_alarm.alarm_id_get = ALARM_EVENT_NSTACK_MAIN_ABNORMAL;

  if (para_value == ALARM_PRODUCT)
    {
      ret_alarm.alarm_flag_get = ALARM_PRODUCT;
    }
  else if (para_value == ALARM_CLEAN)
    {
      ret_alarm.alarm_flag_get = ALARM_CLEAN;
    }
  else
    {
      ret_alarm.alarm_id_get = ALARM_EVENT_MAX;
    }

  ret_alarm.alarm_reason_get = ALARM_REASON_NSMAIN;

  return ret_alarm;

}

/*******************************************************************
*                   Copyright 2017, Huawei Tech. Co., Ltd.
*                            ALL RIGHTS RESERVED
*Description   : period check alarm expire handle function
********************************************************************/

int
ns_alarm_timer_resend_fun (u32 timer_type, void *argv)
{
  int i;
  struct timespec time_left;
  alarm_data *alarm_value;

  for (i = ALARM_EVENT_BASE; i < ALARM_EVENT_MAX; i++)
    {
      alarm_value = &g_alarm_data[i];

      if ((alarm_value->valid_flag == 1)
          && (alarm_value->send_succ_flag == ALARM_SEND_FAIL))
        {
          /* nStack only send nsMain normal alarm */
          ns_send_alarm_inner (alarm_value->_alarm_id,
                               (void *) (alarm_value->_alarm_flag), 1, 0);
        }
    }

  time_left.tv_sec = ALARM_RESEND_TIMER_LENGTH;
  time_left.tv_nsec = 0;
  nsfw_timer_info *timer_info =
    nsfw_timer_reg_timer (0, NULL, ns_alarm_timer_resend_fun, time_left);
  if (NULL == timer_info)
    {
      NSAM_LOGERR ("nsfw_timer_reg_timer fail");
    }

  return 0;
}

void
ns_alarm_set_sendFlag (enum_alarm_id alarmId, int flag)
{
  if ((alarmId <= 0) || (alarmId >= ALARM_EVENT_MAX)
      || (0 == g_alarm_data[alarmId].valid_flag) || ((flag != ALARM_SEND_FAIL)
                                                     && (flag !=
                                                         ALARM_SEND_SUCC)))
    {
      NSAM_LOGERR ("alarm_id is invalid or not reg or flag invalid");
      return;
    }
  g_alarm_data[alarmId].send_succ_flag = flag;
}

/*******************************************************************
*                   Copyright 2017, Huawei Tech. Co., Ltd.
*                            ALL RIGHTS RESERVED
*Description   : alarm module init
********************************************************************/
int
ns_alarm_module_init (void *param)
{
  u32 proc_type = (u32) ((long long) param);
  enum_alarm_id i = ALARM_EVENT_BASE;
  alarm_reason j = ALARM_REASON_BEGIN;
  const char *pst_vm_id = NULL;
  alarm_para tcp_alarm_para;
  struct timespec time_left;

  for (i = ALARM_EVENT_BASE; i < ALARM_EVENT_MAX; i++)
    {
      g_alarm_data[i]._alarm_type = ALARM_SEND_ONCE;

      for (j = ALARM_REASON_BEGIN; j < ALARM_REASON_MAX; j++)
        {
          g_alarm_data[i]._alarm_para.func_alarm_check[j] = NULL;
          g_alarm_data[i]._alarm_para.alarm_reason_set[j] = ALARM_REASON_MAX;
          g_alarm_data[i].alarm_time_laps[j] = 0;
        }
      g_alarm_data[i]._alarm_para.period_alarm.time_length = 0;
      g_alarm_data[i]._alarm_id = i;
      g_alarm_data[i].valid_flag = 0;
      g_alarm_data[i]._alarm_flag = ALARM_CLEAN;
      g_alarm_data[i].alarm_reason_cnt = 0;
      g_alarm_data[i].send_succ_flag = ALARM_SEND_SUCC;
    }

  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
    case NSFW_PROC_CTRL:

      /* modify ip address to vm id */
      pst_vm_id = getenv ("VM_ID");

      if (INVALID_STR_LEN (pst_vm_id, MIN_VM_ID_LEN, MAX_VM_ID_LEN))
        {
          NSAM_LOGWAR
            ("invalid VM_ID,please check env VM_ID]vm_id=%s, proc_type=%u",
             pst_vm_id, proc_type);
        }
      else
        {
          int retVal = STRNCPY_S (g_vmid, MAX_VMID_LEN + 1, pst_vm_id,
                                  strlen (pst_vm_id));

          if (EOK != retVal)
            {
              NSAM_LOGERR ("STRNCPY_S failed]ret=%d", retVal);
            }
        }

      tcp_alarm_para.period_alarm.time_length = 0;      /* 5 second period */
      tcp_alarm_para.alarm_reason_count = 1;    /*both resource */

      tcp_alarm_para.func_alarm_check[0] = ms_alarm_check_func;
      tcp_alarm_para.alarm_reason_set[0] = ALARM_REASON_NSMAIN;
      (void) ns_reg_alarm (ALARM_EVENT_NSTACK_MAIN_ABNORMAL, ALARM_SEND_ONCE,
                           &tcp_alarm_para);

      time_left.tv_sec = ALARM_INIT_RESEND_TIMER_LENGTH;
      time_left.tv_nsec = 0;
      nsfw_timer_info *timer_info =
        nsfw_timer_reg_timer (0, NULL, ns_alarm_timer_resend_fun, time_left);
      if (NULL == timer_info)
        {
          NSAM_LOGERR ("nsfw_timer_reg_timer fail");
        }

      break;
    default:
      break;
    }

  return 0;
}

#define JSON_NEW_OBJ(obj, cb, para1, para2)\
    {\
        obj = json_object_new_object();\
        if (obj == NULL)\
        {\
            (void)cb(para1);\
            (void)cb(para2);\
            return -1;\
        }\
    }

#define JSON_NEW_STRING_OBJ(str, obj, cb, para1, para2)\
    {\
        obj = json_object_new_string((str));\
        if (obj == NULL)\
        {\
            (void)cb(para1);\
            (void)cb(para2);\
            return -1;\
        }\
    }

#define JSON_NEW_STRING_OBJ_1(val, obj, cb, para1, para2,para3)\
    {\
        obj = json_object_new_string((val));\
        if (obj == NULL)\
        {\
            (void)cb(para1);\
            (void)cb(para2);\
            (void)cb(para3);\
            return -1;\
        }\
    }

#define JSON_NEW_INT_OBJ(val, obj, cb, para1, para2)\
    {\
        obj = json_object_new_int((val));\
        if (obj == NULL)\
        {\
            (void)cb(para1);\
            (void)cb(para2);\
            return -1;\
        }\
    }

/*****************************************************************************
*   Prototype    : ns_get_alarm_body
*   Description  : get body by alarm parameter
*   Input        :  char *buf
*                      int buf_len
*                     alarm_result alarm_res
*   Output       : None
*   Return Value : int
*****************************************************************************/
int
ns_get_alarm_body (char *buf, int buf_len, alarm_result alarm_res)
{
#define ALARM_NAME_LENGTH 100
#define COMPONENT_NAME "nStack"
  int retVal;
  size_t str_len;
  int alarm_id = alarm_res.alarm_id_get + 27000;
  struct timeval t_val;
  struct tm now_time;
  char *alarm_string = NULL, *alarm_reason_info = NULL, *alarm_desc = NULL;
  json_object *temp_jso = NULL, *alarm_info = NULL;
  alarm_info_s_out *alarmpara_out = NULL;
  int perceivedSeverity_value = 0;
  char *action_string = NULL;
  if (buf == NULL || (buf_len < (int) sizeof (alarm_info_s_out)))
    {
      NSAM_LOGERR ("input para invalid");
      return -1;
    }
  alarmpara_out = (alarm_info_s_out *) buf;

  /*use sizeof(alarmpara_out->alarmId) instead of devil figure 16 */
  retVal =
    SPRINTF_S (alarmpara_out->alarmId, sizeof (alarmpara_out->alarmId), "%d",
               alarm_id);
  if (-1 == retVal)
    {
      NSAM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
      return -1;
    }

  (void) gettimeofday (&t_val, NULL);
  time_t t_sec = (time_t) t_val.tv_sec;
  /*return value check */
  if (NULL == localtime_r (&t_sec, &now_time))
    {
      NSAM_LOGERR ("localtime_r failed]");
      return -1;
    }

  /*use sizeof(alarmpara_out->alarmtime) instead of devil figure 32 */
  retVal =
    SPRINTF_S (alarmpara_out->alarmtime, sizeof (alarmpara_out->alarmtime),
               "%4d%02d%02d%02d%02d%02d%06ld", now_time.tm_year + 1900,
               now_time.tm_mon + 1, now_time.tm_mday, now_time.tm_hour,
               now_time.tm_min, now_time.tm_sec, (long) t_val.tv_usec);
  if (-1 == retVal)
    {
      NSAM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
      return -1;
    }

  retVal =
    STRNCPY_S (alarmpara_out->comptentname,
               sizeof (alarmpara_out->comptentname), COMPONENT_NAME,
               strlen (COMPONENT_NAME));
  if (EOK != retVal)
    {
      NSAM_LOGERR ("STRNCPY_S failed]ret=%d", retVal);
      return -1;
    }

  switch (alarm_res.alarm_reason_get)
    {
    case ALARM_REASON_SOCKET:
      /* alarmName and reason info is incorrect  */
      alarm_reason_info = "socketResource";
      if (alarm_res.alarm_flag_get == ALARM_PRODUCT)
        {
          alarm_string = "networkResourceOverload";
          alarm_desc = "more than 80 percent used";
          perceivedSeverity_value = 2;
          action_string = "alarm";
        }
      else
        {
          alarm_string = "networkResourceNormal";
          alarm_desc = "less than 60 percent used";
          perceivedSeverity_value = 5;
          action_string = "clear";
        }

      break;
    case ALARM_REASON_MSG_BUF:
      alarm_reason_info = "SendBufResource";
      if (alarm_res.alarm_flag_get == ALARM_PRODUCT)
        {
          alarm_string = "networkResourceOverload";
          alarm_desc = "more than 80 percent used";
          perceivedSeverity_value = 2;
          action_string = "alarm";
        }
      else
        {
          alarm_string = "networkResourceNormal";
          alarm_desc = "less than 60 percent used";
          perceivedSeverity_value = 5;
          action_string = "clear";
        }

      break;
    case ALARM_REASON_NSMAIN:
      /* process status alarm change from event to alarm/clear */
      alarm_reason_info = "networkProcStatus";
      if (alarm_res.alarm_flag_get == ALARM_PRODUCT)
        {
          alarm_string = "network-data-down";
          alarm_desc = "networkProcDown";
          perceivedSeverity_value = 2;
          action_string = "alarm";
        }
      else
        {
          alarm_string = "network-data-up";
          alarm_desc = "networkProcUp";
          perceivedSeverity_value = 5;
          action_string = "clear";
        }

      break;
    default:
      NSAM_LOGERR ("alarm_reason incorrect]ret=%d",
                   alarm_res.alarm_reason_get);
      return -1;
    }

  str_len = 0;

  json_object *alarmlist = json_object_new_object ();
  if (NULL == alarmlist)
    {
      NSAM_LOGERR ("alarmlist is NULL");
      return -1;
    }

  json_object *alarmItem = json_object_new_object ();

  if (NULL == alarmItem)
    {
      NSAM_LOGERR ("alarmItem is NULL");
      (void) json_object_put (alarmlist);
      return -1;
    }

  JSON_NEW_STRING_OBJ ("ICTS_BASE=1", temp_jso, json_object_put, alarmlist,
                       alarmItem);
  json_object_object_add (alarmItem, "neDN", temp_jso);
  JSON_NEW_STRING_OBJ ("service", temp_jso, json_object_put, alarmlist,
                       alarmItem);
  json_object_object_add (alarmItem, "neType", temp_jso);
  JSON_NEW_STRING_OBJ ("ICTS_BASE", temp_jso, json_object_put, alarmlist,
                       alarmItem);
  json_object_object_add (alarmItem, "neName", temp_jso);
  JSON_NEW_STRING_OBJ ("service", temp_jso, json_object_put, alarmlist,
                       alarmItem);
  json_object_object_add (alarmItem, "objectClass", temp_jso);
  JSON_NEW_STRING_OBJ ("nStack", temp_jso, json_object_put, alarmlist,
                       alarmItem);
  json_object_object_add (alarmItem, "moDN", temp_jso);
  JSON_NEW_STRING_OBJ ("nStack", temp_jso, json_object_put, alarmlist,
                       alarmItem);
  json_object_object_add (alarmItem, "moName", temp_jso);
  JSON_NEW_STRING_OBJ ("default", temp_jso, json_object_put, alarmlist,
                       alarmItem);
  json_object_object_add (alarmItem, "userId", temp_jso);

  JSON_NEW_INT_OBJ (alarm_id, temp_jso, json_object_put, alarmlist,
                    alarmItem);
  json_object_object_add (alarmItem, "alarmId", temp_jso);

  JSON_NEW_INT_OBJ (10001, temp_jso, json_object_put, alarmlist, alarmItem);
  json_object_object_add (alarmItem, "groupId", temp_jso);

  JSON_NEW_STRING_OBJ (g_vmid, temp_jso, json_object_put, alarmlist,
                       alarmItem);
  json_object_object_add (alarmItem, "objectInstance", temp_jso);
  JSON_NEW_INT_OBJ (8, temp_jso, json_object_put, alarmlist, alarmItem);
  json_object_object_add (alarmItem, "eventType", temp_jso);

  JSON_NEW_STRING_OBJ (alarm_string, temp_jso, json_object_put, alarmlist,
                       alarmItem);
  json_object_object_add (alarmItem, "alarmName", temp_jso);

  JSON_NEW_INT_OBJ (perceivedSeverity_value, temp_jso, json_object_put,
                    alarmlist, alarmItem);

  json_object_object_add (alarmItem, "perceivedSeverity", temp_jso);

  JSON_NEW_OBJ (alarm_info, json_object_put, alarmlist, alarmItem);
  JSON_NEW_STRING_OBJ_1 (alarm_reason_info, temp_jso, json_object_put,
                         alarmlist, alarmItem, alarm_info);
  json_object_object_add (alarm_info, "reason", temp_jso);
  JSON_NEW_STRING_OBJ_1 (alarm_desc, temp_jso, json_object_put, alarmlist,
                         alarmItem, alarm_info);
  json_object_object_add (alarm_info, "desc", temp_jso);
  json_object_object_add (alarmItem, "alarmInfo", alarm_info);

  JSON_NEW_STRING_OBJ (action_string, temp_jso, json_object_put, alarmlist,
                       alarmItem);

  json_object_object_add (alarmlist, "data", alarmItem);

  json_object_object_add (alarmlist, "action", temp_jso);

  const char *str = json_object_to_json_string (alarmlist);
  if (str == NULL)
    {
      NSMON_LOGERR ("json_object_to_json_string fail");
      (void) json_object_put (alarmlist);
      return -1;
    }

  str_len = strlen (str);

  if (str_len >= ALARM_PARA_LENGTH_OUTER)
    {
      NSAM_LOGERR ("str_len >= ALARM_PARA_LENGTH_OUTER");
      (void) json_object_put (alarmlist);
      return -1;
    }

  retVal =
    STRNCPY_S (alarmpara_out->alarmcontent, ALARM_PARA_LENGTH_OUTER, str,
               str_len + 1);

  if (EOK != retVal)
    {
      NSAM_LOGERR ("STRNCPY_S failed]ret=%d", retVal);
      (void) json_object_put (alarmlist);
      return -1;
    }

  (void) json_object_put (alarmlist);

  return 0;

}

/*******************************************************************
*                   Copyright 2017, Huawei Tech. Co., Ltd.
*                            ALL RIGHTS RESERVED
*Description   : send alarm to thirdparty alarm module, now only log
********************************************************************/
int
ns_send_alarm_to_alarm_module (alarm_result alarm_res)
{
  int retVal = -1;

  nsfw_mgr_msg *req_msg =
    nsfw_mgr_msg_alloc (MGR_MSG_LARGE_ALARM_RSP, NSFW_PROC_ALARM);

  if (NULL == req_msg)
    {
      NSAM_LOGERR ("mgr_msg_alloc fail]alarm_id=%d", alarm_res.alarm_id_get);
      return -1;
    }

  if (req_msg->msg_len < (NSFW_MGR_MSG_HDR_LEN + sizeof (alarm_info_s_out)))
    {
      NSAM_LOGERR ("mgr_msg_alloc length fail]alarm_id=%d",
                   alarm_res.alarm_id_get);
      nsfw_mgr_msg_free (req_msg);
      return -1;
    }
  /* husky agent receive alarm info using fixed length */
  req_msg->msg_len = (NSFW_MGR_MSG_HDR_LEN + sizeof (alarm_info_s_out));

  retVal =
    ns_get_alarm_body ((char *) req_msg->msg_body,
                       NSFW_MGR_LARGE_MSG_BODY_LEN, alarm_res);

  if (-1 == retVal)
    {
      NSAM_LOGERR ("ns_get_alarm_body fail]alarm_id=%d",
                   alarm_res.alarm_id_get);
      nsfw_mgr_msg_free (req_msg);
      return -1;
    }

  u8 ret = nsfw_mgr_send_msg (req_msg);

  if (FALSE == ret)
    {
      NSAM_LOGERR ("nsfw_mgr_send_msg fail]alarm_id=%d,reason=%d,flag=%d",
                   alarm_res.alarm_id_get, alarm_res.alarm_reason_get,
                   alarm_res.alarm_flag_get);
      ns_alarm_set_sendFlag (alarm_res.alarm_id_get, ALARM_SEND_FAIL);
      nsfw_mgr_msg_free (req_msg);
      return -1;
    }

  NSAM_LOGINF ("nsfw_mgr_send_msg succ]alarm_id=%d,reason=%d,flag=%d",
               alarm_res.alarm_id_get, alarm_res.alarm_reason_get,
               alarm_res.alarm_flag_get);
  ns_alarm_set_sendFlag (alarm_res.alarm_id_get, ALARM_SEND_SUCC);

  nsfw_mgr_msg_free (req_msg);

  return 0;
}

/*******************************************************************
*                   Copyright 2017, Huawei Tech. Co., Ltd.
*                            ALL RIGHTS RESERVED
*Description   : alarm product common func
********************************************************************/
void
ns_send_alarm_inner (enum_alarm_id alarm_id, void *para, int check_times,
                     int check_state_flag)
{
  alarm_data *alarm_value = NULL;
  alarm_result alarm_id_report;
  int alarm_idx;
  int alarm_loop;
  int temp_alarm_time_laps;
  int abnormal_alarm_flag = 0, normal_alarm_flag =
    0, total_abnormal_alarm_flag = 0;
  int need_check_flag = check_state_flag;
  int ret = 0;

  if ((alarm_id <= 0) || (alarm_id >= ALARM_EVENT_MAX)
      || (0 == g_alarm_data[alarm_id].valid_flag))
    {
      NSAM_LOGERR ("alarm_id is invalid or not reg");
      return;
    }

  alarm_id_report.alarm_id_get = ALARM_EVENT_MAX;

  alarm_idx = alarm_id;

  alarm_value = &g_alarm_data[alarm_idx];

  for (alarm_loop = 0; alarm_loop < alarm_value->alarm_reason_cnt;
       alarm_loop++)
    {
      abnormal_alarm_flag = 0;
      if (ALARM_PERIOD_CHECK == alarm_value->_alarm_type)
        {
          if (NULL ==
              alarm_value->_alarm_para.func_alarm_check_period[alarm_loop])
            {
              NSAM_LOGERR ("alarm id func_alarm_check is invalid]alarm_id=%d",
                           alarm_id);
              return;
            }
          alarm_id_report =
            alarm_value->_alarm_para.func_alarm_check_period[alarm_loop] ();
        }
      else if (ALARM_SEND_ONCE == alarm_value->_alarm_type)
        {
          if (NULL == alarm_value->_alarm_para.func_alarm_check[alarm_loop])
            {
              NSAM_LOGERR ("alarm id func_alarm_check is invalid]alarm_id=%d",
                           alarm_id);
              return;
            }
          alarm_id_report =
            alarm_value->_alarm_para.func_alarm_check[alarm_loop] (para);
        }

      if ((alarm_id_report.alarm_id_get <= ALARM_EVENT_BASE)
          || alarm_id_report.alarm_id_get >= ALARM_EVENT_MAX)
        {
          NSAM_LOGDBG ("don't satisfy alarm condition");
          return;
        }

      alarm_idx = alarm_id_report.alarm_id_get;

      if (ALARM_EVENT_NSTACK_MAIN_ABNORMAL == alarm_idx)
        {
          need_check_flag = 0;
        }
      /* for send current status alarm, needn't check count also status.  */
      /* for sending current state alarm, needn't check current state */
      if ((alarm_id_report.alarm_flag_get == ALARM_PRODUCT)
          && (((alarm_value->alarm_time_laps[alarm_loop] < check_times)
               && (alarm_value->_alarm_flag != ALARM_PRODUCT))
              || (need_check_flag == 0)))
        {
          if ((0 == check_state_flag)
              || ++(alarm_value->alarm_time_laps[alarm_loop]) >= check_times)
            {
              alarm_value->_alarm_flag = ALARM_PRODUCT;
              abnormal_alarm_flag = 1;
              total_abnormal_alarm_flag++;
            }

        }
      else if ((alarm_id_report.alarm_flag_get == ALARM_CLEAN)
               && ((alarm_value->_alarm_flag == ALARM_PRODUCT)
                   || (need_check_flag == 0)))
        {
          if ((1 == check_state_flag)
              && (alarm_value->alarm_time_laps[alarm_loop] > 0))
            {
              --alarm_value->alarm_time_laps[alarm_loop];
            }
          if ((alarm_value->alarm_time_laps[alarm_loop] <= 0)
              || (0 == check_state_flag))
            {
              normal_alarm_flag++;
            }
        }

      temp_alarm_time_laps = alarm_value->alarm_time_laps[alarm_loop];

      /* can't product same alarm multi times */
      /* only overload alarm can send */
      if (abnormal_alarm_flag != 1)
        {
          NSAM_LOGDBG
            ("don't satisfy alarm condition]alarm_idx=%d,alarm_time_laps=%d",
             alarm_idx, temp_alarm_time_laps);
          continue;
        }

      ret = ns_send_alarm_to_alarm_module (alarm_id_report);

      if (-1 == ret)
        {
          ns_alarm_set_sendFlag (alarm_id, ALARM_SEND_FAIL);
        }
      else
        {
          ns_alarm_set_sendFlag (alarm_id, ALARM_SEND_SUCC);
        }
      /* for alarm current status, only can send one  */
      /* if it have multi-scearo, only can send a overload alarm */
      if (0 == need_check_flag)
        {
          break;
        }

    }

  if ((total_abnormal_alarm_flag == 0)
      && (alarm_value->alarm_reason_cnt == normal_alarm_flag))
    {
      alarm_value->_alarm_flag = ALARM_CLEAN;
      ret = ns_send_alarm_to_alarm_module (alarm_id_report);
      if (-1 == ret)
        {
          ns_alarm_set_sendFlag (alarm_id, ALARM_SEND_FAIL);
        }
      else
        {
          ns_alarm_set_sendFlag (alarm_id, ALARM_SEND_SUCC);
        }
    }

  return;

}

/*******************************************************************
*                   Copyright 2017, Huawei Tech. Co., Ltd.
*                            ALL RIGHTS RESERVED
*Description   : API, any app want to product alarm,need call this function
********************************************************************/
void
ns_send_alarm (enum_alarm_id alarm_id, void *para)
{
  ns_send_alarm_inner (alarm_id, para, 1, 1);
}

/*******************************************************************
*                   Copyright 2017, Huawei Tech. Co., Ltd.
*                            ALL RIGHTS RESERVED
*Description   : period check alarm expire handle function
********************************************************************/
int
ns_alarm_timer_proc_fun (u32 timer_type, void *argv)
{
  struct timespec time_left;
  if (NULL == argv)
    {
      NSAM_LOGERR ("abnormal: argv is NULL ");
      return -1;
    }

  alarm_data *alarm_value = (alarm_data *) argv;

  if (alarm_value->_alarm_type == ALARM_PERIOD_CHECK)
    {
      ns_send_alarm_inner (alarm_value->_alarm_id, NULL,
                           ALARM_PERIOD_CHECK_TIMES, 1);
      time_left.tv_sec = alarm_value->_alarm_para.period_alarm.time_length;
      time_left.tv_nsec = 0;
      nsfw_timer_info *timer_info =
        nsfw_timer_reg_timer (0, (void *) (u64) alarm_value,
                              ns_alarm_timer_proc_fun, time_left);
      if (NULL == timer_info)
        {
          NSAM_LOGERR ("nsfw_timer_reg_timer fail");
          return -1;
        }
    }
  NSAM_LOGDBG ("abnormal: alarm is not period ");

  return 0;

}

/*******************************************************************
*                   Copyright 2017, Huawei Tech. Co., Ltd.
*                            ALL RIGHTS RESERVED
*Description   :  app reg alarm info to alarm module
********************************************************************/
int
ns_reg_alarm (enum_alarm_id alarm_id, alarm_type alarmType,
              alarm_para * alarmPara)
{
  alarm_data *alarm_value = NULL;
  alarm_reason loop = ALARM_REASON_BEGIN;
  struct timespec time_left;

  if ((alarm_id <= ALARM_EVENT_BASE) || (alarm_id >= ALARM_EVENT_MAX)
      || (NULL == alarmPara)
      || (alarmPara->alarm_reason_count > ALARM_REASON_MAX))
    {
      NSAM_LOGERR ("para invalid]alarm_id=%d ", alarm_id);
      return -1;
    }

  for (loop = ALARM_REASON_BEGIN; loop < alarmPara->alarm_reason_count;
       loop++)
    {
      if (NULL == alarmPara->func_alarm_check[loop])
        {
          NSAM_LOGERR ("para invalid]func_alarm_check=NULL,loop=%d", loop);
          return -1;
        }
    }

  alarm_value = &g_alarm_data[alarm_id];
  alarm_value->_alarm_type = alarmType;
  alarm_value->_alarm_para.period_alarm.time_length =
    alarmPara->period_alarm.time_length;
  alarm_value->_alarm_id = alarm_id;
  alarm_value->alarm_reason_cnt = alarmPara->alarm_reason_count;
  alarm_value->_alarm_flag = ALARM_CLEAN;

  for (loop = ALARM_REASON_BEGIN; loop < alarmPara->alarm_reason_count;
       loop++)
    {
      alarm_value->alarm_time_laps[loop] = alarmPara->alarm_reason_set[loop];
      alarm_value->_alarm_para.func_alarm_check[loop] =
        alarmPara->func_alarm_check[loop];
      alarm_value->_alarm_para.func_alarm_check_period[loop] =
        alarmPara->func_alarm_check_period[loop];
      alarm_value->alarm_time_laps[loop] = 0;
    }

  alarm_value->valid_flag = 1;

  if (ALARM_PERIOD_CHECK == alarmType)
    {
      time_left.tv_sec = alarm_value->_alarm_para.period_alarm.time_length;
      time_left.tv_nsec = 0;
      nsfw_timer_info *timer_info =
        nsfw_timer_reg_timer (0, (void *) (u64) alarm_value,
                              ns_alarm_timer_proc_fun, time_left);
      if (NULL == timer_info)
        {
          NSAM_LOGERR ("nsfw_timer_reg_timer fail");
        }
    }

  return 0;
}

/*******************************************************************
*                   Copyright 2017, Huawei Tech. Co., Ltd.
*                            ALL RIGHTS RESERVED
*Description   : send alarm as per current state
********************************************************************/
void
ns_send_init_alarm (enum_alarm_id alarm_id)
{
  if ((alarm_id <= 0) || (alarm_id >= ALARM_EVENT_MAX))
    {
      NSAM_LOGDBG ("alarm_id is invalid");
      return;
    }
  ns_send_alarm_inner (alarm_id,
                       (void *) (g_alarm_data[alarm_id]._alarm_flag), 1, 0);
}

NSFW_MODULE_NAME (NSFW_ALARM_MODULE)
NSFW_MODULE_PRIORITY (10)
NSFW_MODULE_DEPENDS (NSTACK_DMM_MODULE)
NSFW_MODULE_INIT (ns_alarm_module_init)
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
