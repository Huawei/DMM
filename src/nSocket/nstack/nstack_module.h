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

#ifndef __NSTACK_MODULE_H__
#define __NSTACK_MODULE_H__

#include <poll.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include "nstack_types.h"
#include "nstack_dmm_api.h"
#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif

#define NSTACK_LIB_LOAD_DYN      0
#define NSTACK_LIB_LOAD_STATIC   1

#define NSTACK_PLUGIN_NAME_LEN   64

#define NSTACK_MAX_MODULE_NUM  8
#define NSTACK_PRO_MODULE      1

#define MIN_SOCK_FOR_STACKX 0

#define MOD_PRI_FOR_STACKX 1
#define REG_FUN_FOR_STACKX "nstack_stack_register"

#define NSTACK_EP_FREE_NEED_REF   1     /*when epoll information free, need to wait that stack would not notify event */
#define NSTACK_EP_FREE_NONEED_REF 0

#define MODULE_NAME_MAX    128

typedef struct __NSTACK_MODULE_KEYS
{
  ns_char modName[MODULE_NAME_MAX];     /*stack name */
  ns_char register_fn_name[MODULE_NAME_MAX];    /*stack register fun name */
  ns_char libPath[MODULE_NAME_MAX];     /*if libtype is dynamic, it is the path of lib */
  ns_char deploytype;           /*delpoly model type: model type1, model type2, model type3 */
  ns_char libtype;              /*dynamic lib or static lib */
  ns_char ep_free_ref;          /*when epoll information free,
                                 *need to wait that stack would not notify event
                                 */
  ns_char default_stack;        /*whether is default stack:when don't know how to choose stack,
                                 *just use default stack
                                 */
  ns_int32 priority;
  ns_int32 maxfdid;             //the max fd id
  ns_int32 minfdid;             //the min fd id
  ns_int32 modInx;              // This is alloced by nStack , not from configuration
} nstack_module_keys;

typedef struct __NSTACK_MODULE
{
  char modulename[NSTACK_PLUGIN_NAME_LEN];
  ns_int32 priority;
  void *handle;
  nstack_proc_cb mops;
  ns_int32 ep_free_ref;         //ep information need free with ref
  ns_int32 modInx;              // The index of module
  ns_int32 maxfdid;             //the max fd id
  ns_int32 minfdid;             //the min fd id
} nstack_module;

typedef struct
{
  ns_int32 modNum;              // Number of modules registered
  ns_int32 fix_mid;
  nstack_module *defMod;        // The default module
  nstack_module modules[NSTACK_MAX_MODULE_NUM];
} nstack_module_info;

/*register module according the modulecfg file*/
extern ns_int nstack_register_module ();

/*****************************************************************
Parameters    :  ops never be null;  nstack api calls it;
Return        :    0,not match; 1, match
Description   :
*****************************************************************/
extern nstack_module_info g_nstack_modules;
extern nstack_module_keys g_nstack_module_desc[];

#define nstack_defmod_name() (g_nstack_modules.defMod->modulename)
#define nstack_default_module() (g_nstack_modules.defMod)
#define nstack_defMod_inx() (g_nstack_modules.defMod->modInx)
#define nstack_get_module(inx) (&g_nstack_modules.modules[(inx)])
#define nstack_get_modNum() (g_nstack_modules.modNum)
#define nstack_get_module_name_by_idx(inx) (g_nstack_modules.modules[inx].modulename)
#define nstack_def_ops() (&g_nstack_modules.defMod->mops.socket_ops)
#define nstack_module_ops(modInx) (&g_nstack_modules.modules[(modInx)].mops.socket_ops)

#define nstack_get_maxfd_id(modInx) (g_nstack_modules.modules[modInx].maxfdid)
#define nstack_get_minfd_id(modInx) (g_nstack_modules.modules[modInx].minfdid)
#define nstack_get_descmaxfd_id(modInx) (g_nstack_module_desc[modInx].maxfdid)
#define nstack_epinfo_free_flag(modInx) (g_nstack_modules.modules[(modInx)].ep_free_ref == NSTACK_EP_FREE_NEED_REF)

#define nstack_extern_deal(modInx) (g_nstack_modules.modules[(modInx)].mops.extern_ops)

#define nstack_defMod_profd(fdInf) ((fdInf)->protoFD[g_nstack_modules.defMod->modInx].fd)
#define nstack_inf_modProfd(fdInf, pMod) ((fdInf)->protoFD[(pMod)->modInx].fd)

#define nstack_get_fix_mid() (g_nstack_modules.fix_mid)
#define nstack_fix_mid_ops() (&(g_nstack_modules.modules[g_nstack_modules.fix_mid].mops.socket_ops))
#define nstack_fix_stack_name() (g_nstack_modules.modules[g_nstack_modules.fix_mid].modulename)

#define nstack_fix_fd_check() ((g_nstack_modules.fix_mid >= 0) && (g_nstack_modules.fix_mid < NSTACK_MAX_MODULE_NUM) \
                               ? g_nstack_modules.modules[g_nstack_modules.fix_mid].mops.extern_ops.stack_fd_check  \
                               : NULL)

#define nstack_each_modOps(modInx, ops)  \
    for ((modInx) = 0; ((modInx) < nstack_get_modNum() && ((ops) = nstack_module_ops(modInx))); (modInx)++)

#define nstack_each_modInx(modInx) \
    for ((modInx) = 0; ((modInx) < nstack_get_modNum()); (modInx)++)

#define nstack_each_module(modInx, pMod) \
        for ((modInx) = 0; ((modInx) < nstack_get_modNum() && (pMod = nstack_get_module((modInx)))); (modInx)++)

int nstack_stack_module_init ();

int nstack_get_deploy_type ();

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __NSTACK_MODULE_H__ */
