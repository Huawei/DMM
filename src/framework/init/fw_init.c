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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nstack_securec.h"
#include "fw_module.h"
#include "nstack_log.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

NSTACK_STATIC int
nsfw_module_instance_isIndepend (nsfw_module_instance_t * inst)
{
  nsfw_module_depends_t *dep = inst->depends;
  while (dep)
    {
      if (!dep->isReady)
        return 1;
      dep = dep->next;
    }

  return 0;
}

NSTACK_STATIC void
nsfw_module_instance_depend_check (nsfw_module_instance_t * inst)
{
  nsfw_module_instance_t *curInst = nsfw_module_getManager ()->inst;
  while (curInst)
    {
      if (curInst == inst)
        goto nextLoop;
      if (NSFW_INST_STAT_CHECKING == curInst->stat
          || NSFW_INST_STAT_DEPENDING == curInst->stat)
        {
          nsfw_module_depends_t *dep = curInst->depends;
          while (dep)
            {
              if (0 == dep->isReady && 0 == strcmp (dep->name, inst->name))
                {
                  dep->isReady = 1;     /*  Don't break for case that duplicate name exist, though I think it should
                                           not happen */
                }
              dep = dep->next;
            }
        }
    nextLoop:
      curInst = curInst->next;
    }

}

/**
 * @Function        nstack_framework_init
 * @Description     Init child modules
 * @param           father instance , NULL means root
 * @return          0 on success, -1 on error
 */
NSTACK_STATIC int
nstack_framework_initChild_unsafe (nsfw_module_instance_t * father)
{
  NSFW_LOGDBG ("init framework module] name=%s",
               father ? father->name : "NULL");

  nsfw_module_instance_t *inst = nsfw_module_getManager ()->inst;

  while (inst)
    {
      NSFW_LOGDBG
        ("init child] inst=%s, inst->father=%s, inst->depends=%s, inst->state=%d",
         inst->name, inst->father ? inst->father->name : "NULL",
         inst->depends ? inst->depends->name : "NULL", inst->stat);

      if (father != inst->father)
        {
          NSFW_LOGDBG ("inst->father not match] inst=%s, ", inst->name);

          inst = inst->next;
          continue;
        }

      switch (inst->stat)
        {
        case NSFW_INST_STAT_CHECKING:
          /* First, check if any depends, then check if other instance depends on it */
          if (nsfw_module_instance_isIndepend (inst))
            {
              inst->stat = NSFW_INST_STAT_DEPENDING;
              NSFW_LOGDBG ("inst is still depending] name=%s", inst->name);
              inst = inst->next;
              break;
            }

          NSFW_LOGINF ("Going to init module] name=%s, init fun=%p",
                       inst->name, inst->fnInit);
          if (NULL != inst->fnInit && 0 != inst->fnInit (inst->param))
            {
              NSFW_LOGERR ("initial fail!!!] inst=%s", inst->name);
              inst->stat = NSFW_INST_STAT_FAIL;
              return -1;
            }

          inst->stat = NSFW_INST_STAT_DONE;
          nsfw_module_instance_depend_check (inst);

          if (-1 == nsfw_module_addDoneNode (inst))
            {
              NSFW_LOGERR ("add done node fail");
            }

          inst = nsfw_module_getManager ()->inst;       /* check from begining */
          break;
        case NSFW_INST_STAT_DEPENDING:
          /* check if depending stat is still there */
          if (!nsfw_module_instance_isIndepend (inst))
            {
              inst->stat = NSFW_INST_STAT_CHECKING;
              break;
            }
        case NSFW_INST_STAT_FAIL:
        case NSFW_INST_STAT_DONE:
        default:
          inst = inst->next;
          break;
        }
    }

  return 0;
}

NSTACK_STATIC void
nstack_framework_printInstanceInfo (nsfw_module_instance_t * inst)
{

  if (NULL == inst)
    {
      NSFW_LOGERR ("param error,inst==NULL");
      return;
    }

  char info[1024] = "";
  int plen = 0;

  int ret = SPRINTF_S (info, sizeof (info), "Inst:%s,father:%s,depends:",
                       inst->name,
                       inst->father ? inst->father->name : "NULL");

  if (ret <= 0)
    {
      NSFW_LOGERR ("Sprintf Error] module=%s,state=%d, ret=%d", inst->name,
                   inst->stat, ret);
      return;
    }
  else
    {
      plen += ret;
    }

  if (NULL == inst->depends)
    {
      ret = SPRINTF_S (info + plen, sizeof (info) - plen, "NULL");
      if (ret <= 0)
        {
          NSFW_LOGERR ("Sprintf Error] module=%s,state=%d, ret=%d",
                       inst->name, inst->stat, ret);
          return;
        }
      NSFW_LOGINF ("] inst info=%s", info);
      return;
    }

  nsfw_module_depends_t *dep = inst->depends;
  while (dep)
    {
      ret = SPRINTF_S (info + plen, sizeof (info) - plen, "%s ", dep->name);
      if (ret <= 0)
        {
          NSFW_LOGERR ("Sprintf Error] module=%s,state=%d, ret=%d",
                       inst->name, inst->stat, ret);
          return;
        }
      plen += ret;
      dep = dep->next;
    }

  NSFW_LOGINF ("] inst info=%s", info);
}

NSTACK_STATIC void
nstack_framework_printInitialResult ()
{
  nsfw_module_manager_t *manager = nsfw_module_getManager ();

  if (manager->doneHead)
    {
      NSFW_LOGINF ("Here is the initial done modules: ");

      nsfw_module_doneNode_t *curNode = manager->doneHead;
      while (curNode)
        {
          nstack_framework_printInstanceInfo (curNode->inst);
          curNode = curNode->next;
        }
    }
  else
    {
      NSFW_LOGERR ("No initial done modules");
    }

  nsfw_module_instance_t *curInst = manager->inst;
  int unDoneNum = 0;
  while (curInst)
    {
      if (curInst->stat != NSFW_INST_STAT_DONE)
        {
          if (0 == unDoneNum)
            {
              NSFW_LOGINF ("Here is the unInited modules:");
            }
          unDoneNum++;
          nstack_framework_printInstanceInfo (curInst);
        }
      curInst = curInst->next;
    }
  if (0 == unDoneNum)
    NSFW_LOGINF ("All modules are inited");
}

/**
 * @Function        nstack_framework_init
 * @Description     This function will do framework initial work, it will invoke all initial functions
 *                      registered using macro NSFW_MODULE_INIT before
 * @param           none
 * @return          0 on success, -1 on error
 */
int
nstack_framework_init (void)
{
  int ret = -1;
  if (nsfw_module_getManager ()->done)
    {
      goto init_finished;
    }

  if (pthread_mutex_lock (&nsfw_module_getManager ()->initMutex))
    {
      return -1;
    }

  if (nsfw_module_getManager ()->done)
    {
      goto done;
    }

  ret = nstack_framework_initChild_unsafe (NULL);

  if (0 == ret)
    {
      nsfw_module_getManager ()->done = 1;
    }
  else
    {
      nsfw_module_getManager ()->done = -1;
    }

  // Going to print done modules and undone modules
  nstack_framework_printInitialResult ();

done:
  if (pthread_mutex_unlock (&nsfw_module_getManager ()->initMutex))
    {
      return -1;
    }
init_finished:
  ret = nsfw_module_getManager ()->done == 1 ? 0 : -1;
  return ret;
}

/**
 * @Function        nstack_framework_setModuleParam
 * @Description     This function set parameter of module initial function parameter
 * @param           module - name of module
 * @param           param - parameter to set
 * @return          0 on success, -1 on error
 */
int
nstack_framework_setModuleParam (char *module, void *param)
{
  nsfw_module_instance_t *inst = nsfw_module_getModuleByName (module);
  if (!inst)
    return -1;

  inst->param = param;
  return 0;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
