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

#include "stackx_spl_share.h"
#include "common_pal_bitwide_adjust.h"
#include "stackx_event.h"
#include <netinet/in.h>

#define FREE_FD_SET(readfd, writefd, exceptfd)  {\
    if(readfd)\
        free(readfd);\
    if(writefd)\
        free(writefd);\
    if(exceptfd)\
        free(exceptfd);\
}
