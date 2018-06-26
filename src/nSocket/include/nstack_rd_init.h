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

#ifndef _NSTACK_RD_INIT_H_
#define _NSTACK_RD_INIT_H_
#include "nstack_rd_data.h"

#define NSTACK_RD_CHECK_BELONG  (1)
#define NSTACK_RD_CHECK_NOT_BELONG (0)

typedef struct __nstack_stack_info
{
  /*stack name */
  char name[STACK_NAME_MAX];
  /*stack id */
  int stack_id;
  /*when route info not found, high priority stack was chose, same priority chose fist input one */
  int priority;                 /*0: highest: route info not found choose first */
} nstack_stack_info;

/*get rd info. if return ok, data callee alloc memory, caller free, else caller don't free*/
typedef int (*nstack_get_route_data) (rd_route_data ** data, int *num);

/*
 *rd init
 *default id: if all module check fail, just return default id
 *return : 0 success, -1 fail
 */
int nstack_rd_init (nstack_stack_info * pstack, int num,
                    nstack_get_route_data * pfun, int fun_num);

#endif
