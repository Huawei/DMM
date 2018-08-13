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

#ifndef _ALARM_API_H_
#define _ALARM_API_H_

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define NSFW_ALARM_MODULE "nstack_alarm"

/* alarm ID for every event, when need add new alarm, here add a alarm_id define */
typedef enum _alarm_id
{
  ALARM_EVENT_BASE,
  ALARM_EVENT_NSTACK_RESOURCE_ALARM,
  ALARM_EVENT_NSTACK_NO_USE,
  ALARM_EVENT_NSTACK_MAIN_ABNORMAL,
  ALARM_EVENT_MAX
} enum_alarm_id;

/* support both type alarm:
 1. support other module call func:ns_send_alarm to product alarm;
 2. alarm module call function periodly to check whether need product alarm */
typedef enum _alarm_type
{
  ALARM_PERIOD_CHECK,
  ALARM_SEND_ONCE
} alarm_type;

typedef enum _alarm_flag
{
  ALARM_PRODUCT,
  ALARM_CLEAN,
  ALARM_MAX
} alarm_flag;

typedef enum _alarm_reason
{
  ALARM_REASON_BEGIN,
  ALARM_REASON_SOCKET = 0,
  ALARM_REASON_MSG_BUF,
  ALARM_REASON_NSMAIN,
  ALARM_REASON_MAX
} alarm_reason;

typedef struct _alarm_result
{
  enum_alarm_id alarm_id_get;
  alarm_reason alarm_reason_get;
  alarm_flag alarm_flag_get;
} alarm_result;

/* check whether need product alarm,if return 0, then product alarm */
typedef alarm_result (*alarm_check_func) (void *para);
typedef alarm_result (*alarm_check_func_period) (void);

typedef struct _alarm_para
{
  union
  {
    int time_length;
  } period_alarm;
  union
  {
    alarm_check_func func_alarm_check[ALARM_REASON_MAX];
    alarm_check_func_period func_alarm_check_period[ALARM_REASON_MAX];
  };

  alarm_reason alarm_reason_set[ALARM_REASON_MAX];
  int alarm_reason_count;

} alarm_para;

/* for any alarm added, firstly call following function to reg */
int ns_reg_alarm (enum_alarm_id alarm_id, alarm_type _alarm_type,
                  alarm_para * _alarm_para);

void ns_send_init_alarm (enum_alarm_id alarm_id);

/* other module call this func to product alarm, here para is same as  alarm_check_func's para */
void ns_send_alarm (enum_alarm_id alarm_id, void *para);
int ns_alarm_module_init (void *param);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
