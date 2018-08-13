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

#ifndef RTI_H
#define RTI_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define nsfw_rti_stat_macro(type, m) if (1 == g_dfx_switch) { nsfw_rti_stat(type, m); }

typedef enum nsfw_rti_stat_type
{
  NSFW_STAT_PRIMARY_ENQ,
  NSFW_STAT_PRIMARY_ENQ_FAIL,
  NSFW_STAT_PRIMARY_DEQ,
} nsfw_rti_stat_type_t;

typedef struct nsfw_app_info
{
  int nsocket_fd;
  int sbr_fd;

  int hostpid;
  int pid;
  int ppid;

  u64 reserve1;
  u64 reserve2;
  u64 reserve3;
  u64 reserve4;

  u64 extend_member_bit;
} nsfw_app_info_t;

struct rti_queue;
struct msg_t;

extern char g_dfx_switch;
extern struct rti_queue *g_nsfw_rti_primary_stat;

void nsfw_rti_stat (nsfw_rti_stat_type_t statType, const struct msg_t *m);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
