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

#ifndef _NETWORK_H
#define _NETWORK_H
#include "ip_module_api.h"

void free_network_configuration (struct network_configuration *network,
                                 bool_t only_free);
struct network_configuration *parse_network_obj (struct json_object
                                                 *network_obj);
struct network_configuration *parse_network_json (char *param,
                                                  struct network_configuration
                                                  *network_list);
int add_network_configuration (struct network_configuration
                               *network_configuration);
struct network_configuration *get_network_by_name (char *name);
struct network_configuration *get_network_by_nic_name (char *name);
int del_network_by_name (char *name);
int get_network_all (char *jsonBuf, size_t size);
int nic_already_init (const char *nic_name);

#endif
