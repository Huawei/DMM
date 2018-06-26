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

#ifndef _NSTACK_SOCKOPS_H_
#define _NSTACK_SOCKOPS_H_

#include "nstack_log.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif

#define NSTACK_CAL_FUN(ops, fun, args, retval) \
{\
    if(NULL != (ops) && NULL != (ops)->pf##fun)\
    {\
        if((retval = ((ops)->pf##fun args)) == -1)\
        {\
            NSSOC_LOGDBG("function=%s execute failed,ret=%d.errno=%d.",  #fun, retval, errno);  \
        }\
    }\
    else\
    {\
        NSSOC_LOGERR("NULL address erro, ops_pointer=%p,function=%s", (ops), #fun); \
        errno = EBADF;  \
    }\
}

#define NSTACK_SET_OPS_FUN(ops, fun, destFunc) \
        if(NULL != (ops)){      \
            (ops)->pf##fun = (destFunc);\
         }

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
