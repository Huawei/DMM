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

#ifndef _DMM_GROUP_H_
#define _DMM_GROUP_H_

#include "dmm_share.h"

struct dmm_group
{
  volatile int group_init;
  struct dmm_share share;
};

extern struct dmm_group *main_group;

void dmm_group_active ();
int dmm_group_create_main ();
int dmm_group_delete_main ();
int dmm_group_attach_main ();
int dmm_group_detach_main ();

#endif /* _DMM_GROUP_H_ */
