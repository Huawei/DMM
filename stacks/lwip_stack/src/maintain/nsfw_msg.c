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

#include "nsfw_msg.h"
/* *INDENT-OFF* */
msg_fun g_msg_module_fun_array[MAX_MODULE_TYPE] = {NULL};
msg_fun g_msg_module_major_fun_array[MAX_MODULE_TYPE][MAX_MAJOR_TYPE] = {{NULL}};
msg_fun g_msg_module_major_minor_fun_array[MAX_MODULE_TYPE][MAX_MAJOR_TYPE][MAX_MINOR_TYPE] = {{{NULL}}};
msg_fun g_msg_unsupport_fun = NULL;
/* *INDENT-ON* */
