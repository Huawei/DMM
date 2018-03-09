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
#include "types.h"
#include "nstack_securec.h"
#include "nsfw_init.h"

#include "nstack_log.h"
#include "nsfw_maintain_api.h"
#include "nsfw_mem_api.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSFW_MEM_STAT_NUM  512

#define NSFW_MEM_STAT_MODULE "nsfw_mem_stat_module"

typedef struct _nsfw_mem_stat
{
  u8 mem_type;
  u8 alloc_flag;
  char module[NSFW_MEM_MODULE_LEN];
  char mem_name[NSFW_MEM_NAME_LEN];
  u32 mem_size;
} nsfw_mem_stat_t;

nsfw_mem_stat_t g_mem_stat[NSFW_MEM_STAT_NUM];

#ifdef SYS_MEM_RES_STAT
#define MAX_STAT_ITEM_NUM 20
typedef struct _mem_stat_item_t
{
  char name[32];
  u64 size;
} mem_stat_item_t;

typedef struct _mem_stat_mgr_t
{
  u32 item_num;
  mem_stat_item_t item[MAX_STAT_ITEM_NUM];
} mem_stat_mgr;

mem_stat_mgr g_max_mem_list;
#endif

/*****************************************************************************
*   Prototype    : nsfw_mem_stat
*   Description  : add memory stat
*   Input        : char *module
*                  char *mem_name
*                  u8 mem_type
*                  u32 mem_size
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
nsfw_mem_stat (char *module, char *mem_name, u8 mem_type, u32 mem_size)
{
  if (NULL == module || NULL == mem_name)
    {
      NSFW_LOGERR ("argv err]module=%p,mem_name=%p", module, mem_name);
      return;
    }

  int i;
  nsfw_mem_stat_t *mem_stat_item = NULL;
  for (i = 0; i < NSFW_MEM_STAT_NUM; i++)
    {
      if (FALSE == g_mem_stat[i].alloc_flag)
        {
          g_mem_stat[i].alloc_flag = TRUE;
          mem_stat_item = &g_mem_stat[i];
          break;
        }
    }

  if (NULL == mem_stat_item)
    {
      NSFW_LOGERR ("mem stat full]module=%s,type=%u,name=%s,size=%u",
                   module, mem_type, mem_name, mem_size);
      return;
    }

  mem_stat_item->mem_type = mem_type;
  mem_stat_item->mem_size = mem_size;

  if (EOK != STRCPY_S (mem_stat_item->module, NSFW_MEM_MODULE_LEN, module))
    {
      NSFW_LOGERR ("STRNCPY_S failed");
      return;
    }
  if (EOK != STRCPY_S (mem_stat_item->mem_name, NSFW_MEM_NAME_LEN, mem_name))
    {
      NSFW_LOGERR ("STRNCPY_S failed");
      return;
    }

  return;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_stat_print
*   Description  : print all memory info
*   Input        : None
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
nsfw_mem_stat_print ()
{
  int i;
  for (i = 0; i < NSFW_MEM_STAT_NUM; i++)
    {
      if (TRUE == g_mem_stat[i].alloc_flag)
        {
          NSFW_LOGINF ("mem_module=%s,name=%s,type=%u,size=%u",
                       g_mem_stat[i].module, g_mem_stat[i].mem_name,
                       g_mem_stat[i].mem_type, g_mem_stat[i].mem_size);
        }
    }

}

#ifdef SYS_MEM_RES_STAT
void
clear_mem_stat_item ()
{
  if (EOK != MEMSET_S ((char *) &g_max_mem_list, sizeof (mem_stat_mgr),
                       0, sizeof (mem_stat_mgr)))
    {
      NSFW_LOGERR ("MEMSET_S failed");
    }
}

void
insert_mem_stat_item (char *name, u64 len)
{
  int j, temp;

  if (g_max_mem_list.item_num == 0)
    {
      if (EOK !=
          STRCPY_S (g_max_mem_list.item[0].name,
                    sizeof (g_max_mem_list.item[0].name), name))
        {
          NSFW_LOGERR ("STRCPY_S failed");
        }
      g_max_mem_list.item[0].size = len;
      g_max_mem_list.item_num++;
      return;
    }
  else if (g_max_mem_list.item_num < MAX_STAT_ITEM_NUM)
    {
      if (len <= g_max_mem_list.item[g_max_mem_list.item_num - 1].size)
        {
          if (EOK !=
              STRCPY_S (g_max_mem_list.item[g_max_mem_list.item_num].name,
                        sizeof (g_max_mem_list.item
                                [g_max_mem_list.item_num].name), name))
            {
              NSFW_LOGERR ("STRCPY_S failed");
            }
          g_max_mem_list.item[g_max_mem_list.item_num].size = len;
          g_max_mem_list.item_num++;
          return;
        }
      j = 0;
      temp = g_max_mem_list.item_num;
      while (j < temp)
        {
          if (len >= g_max_mem_list.item[j].size)
            {
              goto insert_it;
            }
          j++;
        }
      if (j == temp)
        {
          if (EOK !=
              STRCPY_S (g_max_mem_list.item[j].name,
                        sizeof (g_max_mem_list.item[j].name), name))
            {
              NSFW_LOGERR ("STRCPY_S failed");
            }
          g_max_mem_list.item[j].size = len;
          g_max_mem_list.item_num++;
          return;
        }
    }
  else
    {
      j = 0;
      temp = MAX_STAT_ITEM_NUM - 1;
      while (j < MAX_STAT_ITEM_NUM)
        {
          if (len >= g_max_mem_list.item[j].size)
            {
              goto insert_it;
            }
          j++;
        }
    }

  return;

insert_it:
  while (temp - 1 >= j)
    {
      if (EOK !=
          STRCPY_S (g_max_mem_list.item[temp].name,
                    sizeof (g_max_mem_list.item[temp].name),
                    g_max_mem_list.item[temp - 1].name))
        {
          NSFW_LOGERR ("STRCPY_S failed");
        }
      g_max_mem_list.item[temp].size = g_max_mem_list.item[temp - 1].size;
      temp--;
    }
  if (EOK !=
      STRCPY_S (g_max_mem_list.item[j].name,
                sizeof (g_max_mem_list.item[j].name), name))
    {
      NSFW_LOGERR ("STRCPY_S failed");
    }
  g_max_mem_list.item[j].size = len;
  g_max_mem_list.item_num++;
  return;
}

int
get_mem_stat_item (int idx, char **name, u64 * len)
{
  if (idx < 0 || idx >= MAX_STAT_ITEM_NUM)
    {
      return -1;
    }

  *name = g_max_mem_list.item[idx].name;
  *len = g_max_mem_list.item[idx].size;

  return 0;
}
#endif

static int nsfw_mem_stat_init (void *param);
static int
nsfw_mem_stat_init (void *param)
{
  MEM_STAT (NSFW_MEM_STAT_MODULE, "g_mem_stat", NSFW_NSHMEM,
            sizeof (g_mem_stat));
  nsfw_mem_stat_print ();
#ifdef SYS_MEM_RES_STAT
  clear_mem_stat_item ();
#endif
  return 0;
}

/* *INDENT-OFF* */
NSFW_MODULE_NAME (NSFW_MEM_STAT_MODULE)
NSFW_MODULE_PRIORITY (99)
NSFW_MODULE_INIT (nsfw_mem_stat_init)
/* *INDENT-ON* */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
