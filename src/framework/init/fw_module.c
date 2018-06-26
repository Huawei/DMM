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

#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "nstack_securec.h"
#include "fw_module.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

COMPAT_PROTECT (NSFW_MODULE_INSTANCE_POOL_SIZE, 64);
COMPAT_PROTECT (NSFW_MODULE_DEPENDS_POOL_SIZE, 128);

nsfw_module_instance_pool_t g_nsfw_module_inst_pool;
nsfw_module_depends_pool_t g_nsfw_module_deps_pool;

/*
* WARNING!!!:
*    This function is only used in constructor progress. Multi-thread concurrent is not supported!
*/
NSTACK_STATIC nsfw_module_instance_t *
nsfw_module_malloc_instance ()
{
  if (g_nsfw_module_inst_pool.last_idx >= NSFW_MODULE_INSTANCE_POOL_SIZE)
    {
      return NULL;
    }
  return
    &g_nsfw_module_inst_pool.module_instance_pool[g_nsfw_module_inst_pool.
                                                  last_idx++];
}

/*
* WARNING!!!:
*    This function is only used in constructor progress. Multi-thread concurrent is not supported!
*/
NSTACK_STATIC nsfw_module_depends_t *
nsfw_module_malloc_depends ()
{
  if (g_nsfw_module_deps_pool.last_idx >= NSFW_MODULE_DEPENDS_POOL_SIZE)
    {
      return NULL;
    }
  return
    &g_nsfw_module_deps_pool.module_depends_pool[g_nsfw_module_deps_pool.
                                                 last_idx++];
}

NSTACK_STATIC void
nsfw_module_setChildInstance (nsfw_module_instance_t *
                              father, nsfw_module_instance_t * child)
{
  if (NULL == father || NULL == child)
    return;
  child->father = father;
}

nsfw_module_depends_t *
nsfw_module_create_depends (char *name)
{
  if (NULL == name)
    return NULL;

  /*Change module malloc to pool array */
  nsfw_module_depends_t *dep = nsfw_module_malloc_depends ();

  if (NULL == dep)
    return NULL;

  if (EOK !=
      MEMSET_S (dep, sizeof (nsfw_module_depends_t), 0,
                sizeof (nsfw_module_depends_t)))
    {
      goto fail;
    }

  /*change destMax from nameSize - 1 to sizeof(dep->name) */
  if (EOK != STRCPY_S (dep->name, sizeof (dep->name), name))
    {
      goto fail;
    }
  dep->isReady = 0;

  return dep;

fail:
  // NOTE: if dep is not null, we do not free it and just leave it there.
  return NULL;
}

nsfw_module_instance_t *
nsfw_module_create_instance (void)
{
  /*Change module malloc to pool array */
  nsfw_module_instance_t *inst = nsfw_module_malloc_instance ();
  if (NULL == inst)
    return NULL;

  if (EOK !=
      MEMSET_S (inst, sizeof (nsfw_module_instance_t), 0,
                sizeof (nsfw_module_instance_t)))
    {
      // NOTE: if inst is not null, we do not free it and just leave it there.
      return NULL;
    }

  inst->stat = NSFW_INST_STAT_CHECKING;
  return inst;
}

void
nsfw_module_set_instance_name (nsfw_module_instance_t * inst, char *name)
{
  if (NULL == inst || NULL == name || inst->name[0] != '\0')
    {
      return;
    }

  /*change destMax from nameSize - 1 to sizeof(inst->name) */
  if (EOK != STRCPY_S (inst->name, sizeof (inst->name), name))
    {
      return;
    }

  // Now we need to search if it's any instance's father
  nsfw_module_instance_t *curInst = nsfw_module_getManager ()->inst;
  while (curInst)
    {
      if (curInst == inst)
        goto next_loop;

      if (0 == strcmp (curInst->fatherName, inst->name)
          && NULL == curInst->father)
        {
          nsfw_module_setChildInstance (inst, curInst);
        }
    next_loop:
      curInst = curInst->next;
    }
  return;
}

void
nsfw_module_set_instance_father (nsfw_module_instance_t * inst,
                                 char *fatherName)
{
  if (NULL == inst || NULL == fatherName)
    {
      return;
    }

  if (EOK !=
      STRCPY_S (inst->fatherName, sizeof (inst->fatherName), fatherName))
    {
      return;
    }

  nsfw_module_instance_t *fatherInst =
    nsfw_module_getModuleByName (fatherName);
  if (fatherInst)
    {
      nsfw_module_setChildInstance (fatherInst, inst);
    }
  return;
}

void
nsfw_module_set_instance_priority (nsfw_module_instance_t * inst,
                                   int priority)
{
  if (NULL == inst)
    return;
  inst->priority = priority;

  nsfw_module_del_instance (inst);
  nsfw_module_add_instance (inst);
  return;
}

void
nsfw_module_set_instance_initfn (nsfw_module_instance_t * inst,
                                 nsfw_module_init_fn fn)
{
  if (NULL == inst)
    return;
  inst->fnInit = fn;
  return;
}

void
nsfw_module_set_instance_depends (nsfw_module_instance_t * inst, char *name)
{
  if (NULL == inst || name == NULL)
    return;

  // Check if depends already set
  nsfw_module_depends_t *dep = inst->depends;
  while (dep)
    {
      if (0 == strcmp (dep->name, name))
        return;
      dep = dep->next;
    }

  dep = nsfw_module_create_depends (name);
  if (NULL == dep)
    return;

  if (NULL == inst->depends)
    inst->depends = dep;
  else
    inst->depends->next = dep;
}

/* *INDENT-OFF* */
nsfw_module_manager_t g_nsfw_module_manager = {.inst = NULL, .initMutex =
    PTHREAD_MUTEX_INITIALIZER, .done = 0};
/* *INDENT-ON* */

void
nsfw_module_add_instance (nsfw_module_instance_t * inst)
{
  nsfw_module_instance_t *curInst = nsfw_module_getManager ()->inst;

  if (NULL == curInst || curInst->priority > inst->priority)
    {
      inst->next = curInst;
      nsfw_module_getManager ()->inst = inst;
    }
  else
    {
      while (curInst->next && curInst->next->priority <= inst->priority)
        {
          curInst = curInst->next;
        }
      inst->next = curInst->next;
      curInst->next = inst;
    }
}

void
nsfw_module_del_instance (nsfw_module_instance_t * inst)
{
  if (nsfw_module_getManager ()->inst == inst)
    {
      nsfw_module_getManager ()->inst = inst->next;
      return;
    }

  nsfw_module_instance_t *curInst = nsfw_module_getManager ()->inst;

  while (curInst->next && curInst->next != inst)
    curInst = curInst->next;

  if (!curInst->next)
    return;
  curInst->next = inst->next;
}

nsfw_module_instance_t *
nsfw_module_getModuleByName (char *name)
{
  if (NULL == name)
    return NULL;

  nsfw_module_instance_t *curInst = nsfw_module_getManager ()->inst;
  while (curInst)
    {
      if (0 == strcmp (curInst->name, name))
        {
          return curInst;
        }
      curInst = curInst->next;
    }

  return NULL;
}

int
nsfw_module_addDoneNode (nsfw_module_instance_t * inst)
{
  nsfw_module_doneNode_t *node =
    (nsfw_module_doneNode_t *) malloc (sizeof (nsfw_module_doneNode_t));
  if (NULL == node)
    return -1;
  node->inst = inst;
  node->next = NULL;

  nsfw_module_manager_t *manager = nsfw_module_getManager ();
  if (NULL == manager->doneHead)
    {
      manager->doneHead = node;
    }
  else
    {
      nsfw_module_doneNode_t *tail = manager->doneHead;
      while (tail->next)
        {
          tail = tail->next;
        }

      tail->next = node;
    }

  return 0;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
