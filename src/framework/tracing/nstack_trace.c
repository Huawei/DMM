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

/*==============================================*
 *      include header files                    *
 *----------------------------------------------*/
#include "nstack_trace.h"
#include "nstack_securec.h"
#include <stdarg.h>
#include <dlfcn.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif

/*==============================================*
 *      constants or macros define              *
 *----------------------------------------------*/
#define EVENT_MAX   5
#define NSTACK_TRACE_SWICH     "NSTACK_TRACE_ON"
/*==============================================*
 *      project-wide global variables           *
 *----------------------------------------------*/
trace_fun_t nstack_trace_fun;
__thread trace_hread_t strace_header = {
  .thread_id = -1,
  .trace_id = TRACE_NULL,
};

int tracing_inited = 0;
__thread char *cad_traceID_string = NULL;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
