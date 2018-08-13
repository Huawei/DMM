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

#ifndef _ALARM_H_
#define _ALARM_H_

#include <stdint.h>
#include "alarm_api.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* alarm clean time = 1*3 */

#define ALARM_RESEND_TIMER_LENGTH 60
#define ALARM_INIT_RESEND_TIMER_LENGTH 40

#define ALARM_SEND_SUCC 1
#define ALARM_SEND_FAIL 0

#define ALARM_PARA_LENGTH_OUTER 2048

#define ALARM_PERIOD_CHECK_TIMES 3

#define MAX_VMID_LEN 256

#define MIN_VM_ID_LEN 1
#define MAX_VM_ID_LEN 256

#define INVALID_STR_LEN(_str, _min_len, _maxlen) (NULL == _str || strlen(_str) < _min_len || strlen(_str) > _maxlen)

typedef struct _alarm_data
{
  alarm_para _alarm_para;
  alarm_type _alarm_type;
  enum_alarm_id _alarm_id;
  int alarm_time_laps[ALARM_REASON_MAX];
  alarm_flag _alarm_flag;
  int alarm_reason_cnt;
  int valid_flag;
  int send_succ_flag;

} alarm_data;

typedef struct _alarm_info_s_out
{
  char alarmId[16];
  char comptentname[32];
  char alarmtime[32];
  char alarmcontent[ALARM_PARA_LENGTH_OUTER];
} alarm_info_s_out;

int ns_send_alarm_to_alarm_module (alarm_result alarm_res);

void ns_send_alarm_inner (enum_alarm_id alarm_id, void *para, int check_times,
                          int check_state_flag);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
