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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _NSTACK_INFO_PARSE_H_
#define _NSTACK_INFO_PARSE_H_

#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "nstack_module.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "nstack_rd.h"
#include "nstack_rd_data.h"

#define DEFALT_MODULE_CFG_FILE   "./module_config.json"
#define DEFALT_RD_CFG_FILE   "./rd_config.json"
#define FILE_PATCH_MAX   256
#define NSTACK_CFG_FILELEN_MAX   (1 * 1024 * 1024)
#define NSTACK_RETURN_OK     0
#define NSTACK_RETURN_FAIL   -1

#define NSTACK_MOD_CFG_FILE  "NSTACK_MOD_CFG_FILE"
#define NSTACK_MOD_CFG_RD    "NSTACK_MOD_CFG_RD"

extern nstack_module_keys g_nstack_module_desc[NSTACK_MAX_MODULE_NUM];
extern ns_uint32 g_module_num;

extern int nstack_module_parse ();
extern int nstack_stack_rd_parse (rd_route_data ** data, int *num);

#endif
