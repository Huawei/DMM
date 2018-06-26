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

#ifndef _FW_INIT_H
#define _FW_INIT_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSFW_INIT_PRIORITY_ROOT 101
#define NSFW_INIT_PRIORITY_BASE 102
#define NSFW_INIT_PRIORITY_INITFN 103

#define NSFW_INIT_MODULE_PRIORITY_BASE 1
#define NSFW_INIT_MODULE_PRIORITY(x) (NSFW_INIT_MODULE_PRIORITY_BASE + x)

#define NSFW_SET_INSTANCE_VALUE(_attr, _inst, _value) \
        nsfw_module_set_instance_##_attr(_inst, _value)

#define NSFW_INIT_CREATE_LOCAL_INSTANCE() \
    if (!nsfwLocalInitInst) {\
        nsfwLocalInitInst = nsfw_module_create_instance(); \
        nsfw_module_add_instance(nsfwLocalInitInst);\
    }

#define _NSFW_MODULE_ATTRIBUTE_DEFINE_SURFIX(_attr, _value, _priority, _surfix) \
    static __attribute__((__constructor__(_priority))) void nsfw_module_attribute_##_attr##_surfix(void){\
        NSFW_INIT_CREATE_LOCAL_INSTANCE(); \
        NSFW_SET_INSTANCE_VALUE(_attr, nsfwLocalInitInst, _value);\
    } \

#define NSFW_MODULE_ATTRIBUTE_DEFINE_SURFIX(_attr, _value, _priority, _surfix) \
    _NSFW_MODULE_ATTRIBUTE_DEFINE_SURFIX(_attr, _value, _priority, _surfix)

#define NSFW_MODULE_ATTRIBUTE_DEFINE_UNIQUE(_attr, _value, _priority) \
    NSFW_MODULE_ATTRIBUTE_DEFINE_SURFIX(_attr, _value, _priority, __LINE__)

#define NSFW_MODULE_ATTRIBUTE_DEFINE(_attr, _value, _priority) \
        NSFW_MODULE_ATTRIBUTE_DEFINE_UNIQUE(_attr, _value, _priority)

#define NSFW_MODULE_NAME(_name) \
    NSFW_MODULE_ATTRIBUTE_DEFINE(name, _name, NSFW_INIT_PRIORITY_BASE)

#define NSFW_MODULE_FATHER(_father) \
        NSFW_MODULE_ATTRIBUTE_DEFINE(father, _father, NSFW_INIT_PRIORITY_BASE)

#define NSFW_MODULE_PRIORITY(_priority) \
    NSFW_MODULE_ATTRIBUTE_DEFINE(priority, _priority, NSFW_INIT_PRIORITY_BASE)

#define NSFW_MODULE_DEPENDS(_depends) \
    NSFW_MODULE_ATTRIBUTE_DEFINE(depends, _depends, NSFW_INIT_PRIORITY_BASE)

#define NSFW_MODULE_INIT(_initfn) \
    NSFW_MODULE_ATTRIBUTE_DEFINE(initfn, _initfn, NSFW_INIT_PRIORITY_INITFN)

#define NSFW_MAX_STRING_LENGTH 128

#define NSFW_DEPENDS_SIZE 8
typedef struct _nsfw_module_depends
{
  char name[NSFW_MAX_STRING_LENGTH];
  int isReady;
  struct _nsfw_module_depends *next;    /* It is a list, not just only one */
} nsfw_module_depends_t;

typedef enum
{
  NSFW_INST_STAT_CHECKING,      /* Not check yet */
  NSFW_INST_STAT_DEPENDING,     /* Blocked, waiting for other module instances */
  NSFW_INST_STAT_DONE,          /* Check done */
  NSFW_INST_STAT_FAIL           /* Check Fail */
} nsfw_module_instance_stat_t;

typedef int (*nsfw_module_init_fn) (void *);

typedef struct _nsfw_module_instance
{
  nsfw_module_init_fn fnInit;
  char name[NSFW_MAX_STRING_LENGTH];
  char fatherName[NSFW_MAX_STRING_LENGTH];
  int priority;
  nsfw_module_depends_t *depends;
  nsfw_module_instance_stat_t stat;
  void *param;
  struct _nsfw_module_instance *next;
  struct _nsfw_module_instance *child;
  struct _nsfw_module_instance *father;
} nsfw_module_instance_t;

static nsfw_module_instance_t *nsfwLocalInitInst __attribute__ ((unused)) =
  (void *) 0;

extern nsfw_module_instance_t *nsfw_module_create_instance ();
extern nsfw_module_instance_t *nsfw_module_getModuleByName (char *);
extern void nsfw_module_add_instance (nsfw_module_instance_t * inst);
extern void nsfw_module_del_instance (nsfw_module_instance_t * inst);
extern void nsfw_module_set_instance_name (nsfw_module_instance_t * inst,
                                           char *name);
extern void nsfw_module_set_instance_father (nsfw_module_instance_t * inst,
                                             char *father);
extern void nsfw_module_set_instance_priority (nsfw_module_instance_t *
                                               inst, int priority);
extern void nsfw_module_set_instance_initfn (nsfw_module_instance_t * inst,
                                             nsfw_module_init_fn fn);
extern void nsfw_module_set_instance_depends (nsfw_module_instance_t * inst,
                                              char *name);

/**
 * @Function        nstack_framework_init
 * @Description     This function will do framework initial work, it will invoke all initial functions
 *                      registered using macro NSFW_MODULE_INIT before
 * @param           none
 * @return          0 on success, -1 on error
 */
extern int nstack_framework_init (void);

/**
 * @Function        nstack_framework_setModuleParam
 * @Description     This function set parameter of module initial function parameter
 * @param           module - name of module
 * @param           param - parameter to set
 * @return          0 on success, -1 on error
 */
extern int nstack_framework_setModuleParam (char *module, void *param);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _FW_INIT_H  */
