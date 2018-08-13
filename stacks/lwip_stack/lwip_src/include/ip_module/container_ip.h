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

#ifndef _CONTAINER_IP_H
#define _CONTAINER_IP_H
#include "ip_module_api.h"
#include "json.h"

struct ip_action_param
{
  char container_id[IP_MODULE_LENGTH_256];
  char port_name[IP_MODULE_LENGTH_256];
};

void free_container (struct container_ip *container, bool_t only_free);
struct container_port *parse_port_obj (struct json_object *port_obj);
struct container_ip *parse_container_ip_json (char *param);
int add_container (struct container_ip *container);
struct container_ip *get_container_by_container_id (char *container_id);

int del_port (char *container_id, char *port_name);
struct container_port *get_port (char *container_id, char *port_name);
struct container_port *get_port_from_container (struct container_port *port);
int getIpCfgAll (char *jsonBuf, size_t size);

#endif
