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

#ifndef __NSTACK_TRACE_H__
#define __NSTACK_TRACE_H__

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif

/*==============================================*
 *      include header files                    *
 *----------------------------------------------*/
#include "types.h"
#include "nstack_log.h"
#include <time.h>
#define SPAN_NAME_MAX  64
#define TRACE_NULL         0
#define NSTACK_TRACE_ON     0
#define NSTACK_TRACE_OFF    1

typedef struct nst_trace_header
{
  u64 trace_id;
  u64 span_pid;
  u64 span_id;                  /*now us */
  int fd;
  int thread_id;
} trace_hread_t;

typedef struct nst_trace_fun
{
  u64 (*current_traceid) ();
  void (*span_raw) (char *msg);
  void (*tracing_enable) (int enable);
  void (*tracing_simpling) (int frequency);
  void (*tracing_cleanup) ();
  void (*tracing_setup) (u32 process);
    u64 (*tracing_string_to_traceid) (const char *input, size_t length);
} trace_fun_t;

extern __thread struct nst_trace_header strace_header;

#define get_trace_header()   (&strace_header)

//For Tracing define own tracing feature.
#define nstack_trace_init( a )
#define nstack_set_tracing_contex( a,b,c)
#define nstack_get_tracing_contex(traceid,spanid,spanpid,fd)
#define nstack_clear_tracing_contex( )
#define NSTACK_TRACING(level, module_name,famt,...)
#define nstack_tracing_enalbe()
#define nstack_get_traceid()    (NULL)

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
