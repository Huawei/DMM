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

#include <stdlib.h>
#include "nstack_rd.h"
#include "nstack_rd_data.h"
#include "nstack_rd_init.h"
#include "nstack_rd_priv.h"
#include "nstack_log.h"
#include "nstack_securec.h"

#define NSTACK_RD_ERR_CHECK_GOTO(ret, desc, lab)  \
if (NSTACK_RD_SUCCESS != (ret))\
{  \
   NSSOC_LOGERR("%s fail, return:%d", desc, ret); \
   goto lab;  \
}

#define NSTACK_RD_POINT_CHECK_GOTO(ptr, desc, lab)  \
if (!ptr)\
{  \
   NSSOC_LOGERR("%s fail", desc); \
   goto lab;  \
}

rd_local_data *g_rd_local_data = NULL;

/*****************************************************************************
*   Prototype    : nstack_rd_init
*   Description  : nstack rd init
*   Input        : nstack_stack_info *pstack
*                  int num
*                  nstack_get_route_data pfun
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*   pstack       : 1. priority 0: when router not fund, the first will be chose
*                  2. if have many same 0 priority, fist input will be chose,
*                  3. if none 0 priority, the last input will be choose
*****************************************************************************/
int
nstack_rd_init (nstack_stack_info * pstack, int num,
                nstack_get_route_data * pfun, int fun_num)
{
  int icnt = 0;
  int hindex = 0;
  int tindex = num - 1;
  int iindex = 0;
  int ret = NSTACK_RD_SUCCESS;
  nstack_rd_stack_info *ptemstack = NULL;

  if ((!pstack) || (!pfun))
    {
      NSSOC_LOGERR ("input err pstack:%p, pfun:%p", pstack, pfun);
      return NSTACK_RD_FAIL;
    }
  g_rd_local_data = (rd_local_data *) malloc (sizeof (rd_local_data));
  if (!g_rd_local_data)
    {
      NSSOC_LOGERR ("g_rd_local_data alloc fail");
      return NSTACK_RD_FAIL;
    }

  if (EOK !=
      MEMSET_S ((void *) g_rd_local_data, sizeof (rd_local_data), 0,
                sizeof (rd_local_data)))
    {
      NSSOC_LOGERR ("MEMSET_S fail");
      goto ERR;
    }
  for (icnt = 0; icnt < fun_num; icnt++)
    {
      g_rd_local_data->sys_fun[icnt] = pfun[icnt];
    }
  g_rd_local_data->fun_num = fun_num;
  ptemstack =
    (nstack_rd_stack_info *) malloc (sizeof (nstack_rd_stack_info) * num);
  NSTACK_RD_POINT_CHECK_GOTO (ptemstack, "rd stack info malloc fail", ERR);

  /*save stack info in priority order */
  for (icnt = 0; icnt < num; icnt++)
    {
      if (0 == pstack[icnt].priority)
        {
          iindex = hindex;
          hindex++;
        }
      else
        {
          iindex = tindex;
          tindex--;
        }

      /* modify destMax from RD_PLANE_NAMELEN to STACK_NAME_MAX */
      ret =
        STRCPY_S (ptemstack[iindex].name, STACK_NAME_MAX, pstack[icnt].name);
      if (ret != EOK)
        {
          NSSOC_LOGERR ("STRCPY_S failed");
          goto ERR;
        }

      ptemstack[iindex].priority = pstack[icnt].priority;
      ptemstack[iindex].stack_id = pstack[icnt].stack_id;
      NSSOC_LOGDBG
        ("nstack rd init]stackname=%s,priority=%d,stackid=%d was added",
         ptemstack[iindex].name,
         ptemstack[iindex].priority, ptemstack[iindex].stack_id);
    }

  g_rd_local_data->pstack_info = ptemstack;
  g_rd_local_data->stack_num = num;

  for (icnt = 0; icnt < RD_DATA_TYPE_MAX; icnt++)
    {
      INIT_HLIST_HEAD (&(g_rd_local_data->route_list[icnt].headlist));
    }
  return NSTACK_RD_SUCCESS;

ERR:
  if (g_rd_local_data)
    {
      free (g_rd_local_data);
      g_rd_local_data = NULL;
    }
  if (ptemstack)
    {
      free (ptemstack);
    }
  return NSTACK_RD_FAIL;
}

/*****************************************************************************
*   Prototype    : nstack_get_stackid_byname
*   Description  : get stack ip by stack name
*   Input        : char *name
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nstack_get_stackid_byname (char *name)
{
  int stacknum = g_rd_local_data->stack_num;
  int iindex = 0;
  nstack_rd_stack_info *pstack = NULL;
  for (iindex = 0; iindex < stacknum; iindex++)
    {
      pstack = &(g_rd_local_data->pstack_info[iindex]);
      if (0 == strcmp (pstack->name, name))
        {
          return pstack->stack_id;
        }
    }
  return -1;
}
