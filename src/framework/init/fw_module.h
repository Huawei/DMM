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

#ifndef _FW_MODULE
#define _FW_MODULE
#include <pthread.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* _cplusplus */

#include "nsfw_init.h"

/* Unique name declare */
#define NSFW__STRINGIFY(x) #x
#define NSFW_STRINGIFY(x) NSFW__STRINGIFY(x)

#define NSFW__PASTE(a,b) a##b
#define NSFW_PASTE(a,b) NSFW__PASTE(a,b)

#define NSFW_UNIQUE_ID(prefix) NSFW_PASTE(NSFW_PASTE(__LINE__, prefix), __COUNTER__)

#define NSFW_MODULE_INITFN(_fn) \
    static int _fn(void* param)
extern nsfw_module_depends_t *nsfw_module_create_depends (char *name);

#define NSFW_MODULE_SET_STATE(inst, state) ((inst)->stat = (state))

typedef struct _nsfw_module_doneNode
{
  nsfw_module_instance_t *inst;
  struct _nsfw_module_doneNode *next;
} nsfw_module_doneNode_t;

typedef struct _nsfw_module_manager
{
  pthread_mutex_t initMutex;
  int done;                     // 0 - not finished, 1 - finished, -1 - error
  nsfw_module_instance_t *inst;
  nsfw_module_doneNode_t *doneHead;
} nsfw_module_manager_t;

extern int nsfw_module_addDoneNode (nsfw_module_instance_t * inst);

extern nsfw_module_manager_t g_nsfw_module_manager;
#define nsfw_module_getManager() (&g_nsfw_module_manager)

#define NSFW_MODULE_INSTANCE_POOL_SIZE 64
#define NSFW_MODULE_DEPENDS_POOL_SIZE 128

typedef struct _nsfw_module_instance_pool
{
  int last_idx;
    nsfw_module_instance_t
    module_instance_pool[NSFW_MODULE_INSTANCE_POOL_SIZE];
} nsfw_module_instance_pool_t;

typedef struct _nsfw_module_depends_pool
{
  int last_idx;
  nsfw_module_depends_t module_depends_pool[NSFW_MODULE_DEPENDS_POOL_SIZE];
} nsfw_module_depends_pool_t;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* _cplusplus */

#endif /* _FW_MODULE */
