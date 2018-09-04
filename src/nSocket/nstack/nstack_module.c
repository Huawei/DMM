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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "nstack_module.h"
#include "common_mem_buf.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "nstack_dmm_api.h"
#include "nstack_dmm_adpt.h"
#include "nstack_rd_init.h"
#include "nstack_info_parse.h"

/* *INDENT-OFF* */
nstack_module_info g_nstack_modules = {
                                .modNum = 0,
                                .fix_mid = -1,
                                .modules = {{{0}}},
                                .defMod = NULL,
                            };

/* *INDENT-ON* */
nstack_module_keys g_nstack_module_desc[NSTACK_MAX_MODULE_NUM];

ns_uint32 g_module_num = 0;

int
nstack_get_deploy_type ()
{
  int icnt = 0;
  int type = 0;
  for (icnt = 0; icnt < g_module_num; icnt++)
    {
      if (g_nstack_module_desc[icnt].deploytype > type)
        {
          type = g_nstack_module_desc[icnt].deploytype;
        }
    }
  return type;
}

NSTACK_STATIC inline int
nstack_register_one_module (nstack_module_keys * pKeys)
{

  nstack_module *pmod = NULL;
  nstack_stack_register_fn stack_register_fn = NULL;
  nstack_event_cb val = { 0 };
  int retVal;
  int ret = 0;

  if (pKeys->modInx >= NSTACK_MAX_MODULE_NUM)
    {
      NSSOC_LOGERR ("modeindex is overload]index=%d", pKeys->modInx);
      ret = -1;
      goto err_return;
    }

  pmod = nstack_get_module (pKeys->modInx);

  retVal =
    STRNCPY_S (pmod->modulename, NSTACK_PLUGIN_NAME_LEN, pKeys->modName,
               NSTACK_PLUGIN_NAME_LEN);
  if (EOK != retVal)
    {
      NSSOC_LOGERR ("STRNCPY_S failed]ret=%d", retVal);
      ret = -1;
      goto err_return;
    }

  pmod->modulename[NSTACK_PLUGIN_NAME_LEN - 1] = '\0';
  pmod->priority = pKeys->priority;
  pmod->modInx = pKeys->modInx;
  pmod->maxfdid = pKeys->maxfdid;
  pmod->minfdid = pKeys->minfdid;
  pmod->ep_free_ref = pKeys->ep_free_ref;

  if (pKeys->libtype == NSTACK_LIB_LOAD_DYN)
    {
      pmod->handle = dlopen (pKeys->libPath, RTLD_LAZY);
      if (!pmod->handle)
        {
          /*optimize dlopen err print */
          NSSOC_LOGERR ("dlopen lib=%s of module=%s failed, err_string=%s",
                        pKeys->libPath, pKeys->modName, dlerror ());
          ret = -1;
          goto err_return;
        }
    }
  else
    {
      pmod->handle = RTLD_DEFAULT;
    }

  stack_register_fn = dlsym (pmod->handle, pKeys->register_fn_name);
  if (!stack_register_fn)
    {
      /* optimize dlopen err print */
      NSSOC_LOGERR ("register function not found]err_string=%s", dlerror ());
      if (pmod->handle)
        {
          dlclose (pmod->handle);
          pmod->handle = NULL;
        }
      ret = -1;
      goto err_return;
    }
  val.handle = pmod->handle;
  val.type = pKeys->modInx;
  val.event_cb = nstack_event_callback;
  if (stack_register_fn (&pmod->mops, &val))
    {
      NSSOC_LOGERR ("register function failed");
      if (pmod->handle)
        {
          dlclose (pmod->handle);
          pmod->handle = NULL;
        }
      ret = -1;
      goto err_return;
    }

  /* malloc length need protect
     malloc parameter type is size_t */
  if (((pmod->maxfdid + 1) < 1)
      || (SIZE_MAX / sizeof (ns_int32) < (pmod->maxfdid + 1)))
    {
      NSSOC_LOGERR ("malloc size is wrong]maxfdid=%d", pmod->maxfdid);
      if (pmod->handle)
        {
          dlclose (pmod->handle);
          pmod->handle = NULL;
        }
      ret = -1;
      goto err_return;
    }

err_return:
  return ret;
}

/*nstack_register_module can't concurrent*/
int
nstack_register_module ()
{
  unsigned int idx = 0;
  nstack_stack_info *pstacks = NULL;
  int ret = 0;

  nstack_get_route_data rd_fun[] = {
    nstack_stack_rd_parse,
  };

  pstacks =
    (nstack_stack_info *) malloc (sizeof (nstack_stack_info) * g_module_num);
  if (!pstacks)
    {
      NSSOC_LOGERR ("malloc failed]");
      return ns_fail;
    }
  ret =
    MEMSET_S (pstacks, sizeof (nstack_stack_info) * g_module_num, 0,
              sizeof (nstack_stack_info) * g_module_num);
  if (EOK != ret)
    {
      NSSOC_LOGERR ("MEMSET_S failed]ret=%d", ret);
      free (pstacks);
      return ns_fail;
    }

  for (idx = 0; idx < g_module_num; idx++)
    {
      if (0 != nstack_register_one_module (&g_nstack_module_desc[idx]))
        {
          NSSOC_LOGERR
            ("can't register module]modInx=%d,modName=%s,libPath=%s",
             g_nstack_module_desc[idx].modInx,
             g_nstack_module_desc[idx].modName,
             g_nstack_module_desc[idx].libPath);
          free (pstacks);
          return ns_fail;
        }
      ret =
        STRCPY_S (pstacks[idx].name, STACK_NAME_MAX,
                  g_nstack_module_desc[idx].modName);
      if (EOK != ret)
        {
          NSSOC_LOGERR ("STRCPY_S fail]idx=%d,modName=%s,ret=%d", idx,
                        g_nstack_module_desc[idx].modName, ret);
          free (pstacks);
          return ns_fail;
        }
      pstacks[idx].priority = g_nstack_module_desc[idx].priority;
      pstacks[idx].stack_id = g_nstack_module_desc[idx].modInx;
      if (g_nstack_module_desc[idx].default_stack == 1)
        {
          g_nstack_modules.defMod =
            &g_nstack_modules.modules[g_nstack_module_desc[idx].modInx];
          g_nstack_modules.fix_mid = g_nstack_module_desc[idx].modInx;
        }
    }

  if (g_nstack_modules.fix_mid < 0)
    {
      free (pstacks);
      NSSOC_LOGERR ("nstack fix mid is unknown and return fail");
      return ns_fail;
    }

  g_nstack_modules.modNum = g_module_num;
  /*rd module init */
  if (ns_success !=
      nstack_rd_init (pstacks, idx, rd_fun,
                      sizeof (rd_fun) / sizeof (nstack_get_route_data)))
    {
      free (pstacks);
      NSSOC_LOGERR ("nstack rd init fail");
      return ns_fail;
    }
  free (pstacks);
  return ns_success;
}

int
nstack_stack_module_init ()
{
  ns_uint32 idx;
  for (idx = 0; idx < g_module_num; idx++)
    {
      if (g_nstack_modules.modules[idx].mops.extern_ops.module_init)
        {
          if (0 !=
              g_nstack_modules.modules[idx].mops.extern_ops.module_init ())
            {
              NSSOC_LOGERR ("nstack[%s] modx:%d init fail",
                            g_nstack_modules.modules[idx].modulename, idx);
            }
        }
    }
  return 0;
}

int
nstack_stack_module_init_child ()
{
  ns_uint32 idx;
  for (idx = 0; idx < g_module_num; idx++)
    {
      if (g_nstack_modules.modules[idx].mops.extern_ops.module_init_child)
        {
          if (0 !=
              g_nstack_modules.modules[idx].mops.
              extern_ops.module_init_child ())
            {
              NSSOC_LOGERR ("nstack[%s] modx:%d init child fail",
                            g_nstack_modules.modules[idx].modulename, idx);
            }
        }
    }
  return 0;
}
