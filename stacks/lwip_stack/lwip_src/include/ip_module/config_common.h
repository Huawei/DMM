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

#ifndef _NSTACK_CONFIG_COMMON_H_
#define _NSTACK_CONFIG_COMMON_H_

#include "ip_module_api.h"

#define MAX_IP_MODULE_BUFF_SIZE 1024*1024

/* ip module config types */
#define IP_MODULE_TYPE_IP           "ip"
#define IP_MODULE_TYPE_NETWORK      "network"
#define IP_MODULE_TYPE_PORT         "port"
#define IP_MODULE_TYPE_SETLOG       "setlog"
#define IP_MODULE_TYPE_SNAPSHOT "snapshot"
#define IP_MODULE_TYPE_SETTRACE  "settrace"

#define TCP_MODULE_TYPE_SET_OOS_LEN     "ooslen"

#define IP_MODULE_NAME                 "./nStackCtrl: "
#define IP_MODULE_INVALID_ARGUMENT_S "invalid argument -- \"%s\"\n"
#define IP_MODULE_MORE_OPTION  "need more options -- "
#define IP_MODULE_LESS_OPTION     "no need option -- "

/* Error codes */
#define IP_MODULE_OK             0
#define IP_MODULE_DATA_ERROR     1
#define IP_MODULE_DATA_NOT_EXIST 2

struct config_param
{
  int action;
  char type[IP_MODULE_LENGTH_256];
  char name[IP_MODULE_LENGTH_256];
  char value[IP_MODULE_LENGTH_64];
  char container_id[IP_MODULE_LENGTH_256];
  int error;

  char error_desc[NSCRTL_ERRBUF_LEN];

  u64 traceid;
};

struct config_data
{
  struct config_param param;
  char json_buff[MAX_IP_MODULE_BUFF_SIZE - sizeof (struct config_param)];
};

#endif
