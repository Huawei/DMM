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

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif /* __cplusplus */

#ifndef __NSTACK_DMM_ADPT_H__
#define __NSTACK_DMM_ADPT_H__

typedef enum
{
  NSTACK_MODEL_TYPE1 = 1,       /*nSocket and stack belong to the same process */
  NSTACK_MODEL_TYPE2 = 2,       /*nSocket and stack belong to different processes,
                                 *and nStack don't take care the communication between stack and stack adpt
                                 */
  NSTACK_MODEL_TYPE3 = 3,       /*nSocket and stack belong to different processes, and sbr was supplied to communicate whit stack */
  NSTACK_MODEL_TYPE_SIMPLE_STACK = 4,   /* like TYPE1, DMM will NOT provide SBR or pipeline mode, just allocate 32M, and use dpdk file
                                         * prefix to support multiple running app under DMM */
  NSTACK_MODEL_INVALID,
} nstack_model_deploy_type;

#define NSTACK_DMM_MODULE   "nstack_dmm_module"

typedef struct nsfw_com_attr
{
  int policy;
  int pri;
} nsfw_com_attr;

typedef struct __nstack_dmm_para
{
  nstack_model_deploy_type deploy_type;
  int proc_type;
  nsfw_com_attr attr;
  int argc;
  char **argv;
} nstack_dmm_para;

extern int nstack_adpt_init (nstack_dmm_para * para);
extern int nstack_event_callback (void *pdata, int events);

#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
