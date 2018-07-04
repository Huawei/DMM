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

/*==============================================*
 *      constants or macros define              *
 *----------------------------------------------*/

/*==============================================*
 *      project-wide global variables           *
 *----------------------------------------------*/

/*==============================================*
 *      routines' or functions' implementations *
 *----------------------------------------------*/

#include <stdlib.h>
#include "types.h"
#include "nstack_securec.h"
#include "nsfw_init.h"
#include "common_mem_mbuf.h"

#include "nstack_log.h"
#include "nsfw_maintain_api.h"

#include "nsfw_mem_api.h"
#include "nsfw_fd_timer_api.h"
#include "nsfw_ring_data.h"

#include "common_func.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

nsfw_res_mgr_item_cfg g_all_res_can[NSFW_MAX_RES_SCAN_COUNT];

#define NSFW_RES_SCAN_TVLAUE_DEF 60
#define NSFW_RES_SCAN_TVLAUE (g_scan_cfg.scan_tvalue)

typedef struct _nsfw_res_scan_init_cfg
{
  nsfw_timer_info *scan_timer;
  u16 scan_tvalue;
} nsfw_res_scan_init_cfg;
nsfw_res_scan_init_cfg g_scan_cfg;

u8
nsfw_res_mgr_reg (nsfw_res_scn_cfg * cfg)
{
  if (NULL == cfg)
    {
      NSFW_LOGERR ("argv err!");
      return FALSE;
    }

  u32 i;
  for (i = 0; i < NSFW_MAX_RES_SCAN_COUNT; i++)
    {
      if ((NULL == g_all_res_can[i].scn_cfg.free_fun)
          &&
          (__sync_bool_compare_and_swap
           (&g_all_res_can[i].scn_cfg.free_fun, 0, cfg->free_fun)))
        {
          g_all_res_can[i].scn_cfg = *cfg;
          NSFW_LOGINF ("reg res_mgr fun suc]fun=%p,data=%p", cfg->free_fun,
                       cfg->data);
          return TRUE;
        }
    }

  NSFW_LOGERR
    ("reg]type=%u,per=%u,chk=%u,cyc=%u,total=%u,size=%u,offset=%u,fun=%p,data=%p",
     cfg->type, cfg->force_free_percent, cfg->force_free_chk_num,
     cfg->num_per_cyc, cfg->total_num, cfg->elm_size, cfg->res_mem_offset,
     cfg->res_mem_offset, cfg->free_fun, cfg->data);
  return FALSE;
}

static inline u32
nsfw_get_alloc_count (u32 head, u32 tail)
{
  if (head >= tail)
    {
      return head - tail;
    }

  return head + (0xFFFFFFFF - tail);
}

int
nsfw_res_sp_item_chk (void *data, void *argv)
{
  nsfw_res_mgr_item_cfg *res_scn_item = (nsfw_res_mgr_item_cfg *) argv;
  nsfw_res_scn_cfg *scn_cfg = &res_scn_item->scn_cfg;
  char *elm = (char *) data;

  if (NULL == scn_cfg || NULL == elm)
    {
      return FALSE;
    }

  nsfw_res *res_item = NULL;
  res_item = (nsfw_res *) (elm + scn_cfg->res_mem_offset);
  if (0 == res_item->chk_count)
    {
      res_item->data = res_scn_item->cons_head;
    }
  res_item->chk_count++;

  if (res_item->chk_count < scn_cfg->force_free_chk_num)
    {
      return FALSE;
    }

  if (res_scn_item->free_percent > scn_cfg->force_free_percent)
    {
      return FALSE;
    }

  if (scn_cfg->total_num * scn_cfg->alloc_speed_factor >
      nsfw_get_alloc_count (res_scn_item->cons_head, res_item->data))
    {
      return FALSE;
    }

  if (NULL == scn_cfg->free_fun)
    {
      return FALSE;
    }

  if (TRUE == scn_cfg->free_fun ((void *) elm))
    {
      res_scn_item->force_count++;
    }

  res_item->chk_count = 0;
  return TRUE;
}

int
nsfw_res_flash_data (nsfw_res_mgr_item_cfg * res_scn_item)
{
  nsfw_res_scn_cfg *scn_cfg = &res_scn_item->scn_cfg;

  u32 cur_head = 0;
  u32 cur_tail = 0;
  u32 elm_num = 0;
  u32 free_count = 0;

  switch (scn_cfg->type)
    {
    case NSFW_RES_SCAN_MBUF:
      {
        struct common_mem_ring *ring =
          (struct common_mem_ring *) scn_cfg->mgr_ring;
        struct common_mem_mempool *mp =
          (struct common_mem_mempool *) scn_cfg->data;
        if (NULL == ring)
          {
            ring = mp->pool_data;
            if (NULL == ring)
              return FALSE;
          }
        cur_head = ring->prod.head;
        cur_tail = ring->cons.head;
        elm_num = mp->size;
      }
      break;
    case NSFW_RES_SCAN_SPOOL:
      {
        struct nsfw_mem_ring *mem_ring =
          (struct nsfw_mem_ring *) scn_cfg->mgr_ring;
        if (NULL == mem_ring)
          {
            mem_ring = (struct nsfw_mem_ring *) scn_cfg->data;
            if (NULL == mem_ring)
              return FALSE;
          }

        cur_head = mem_ring->prod.head;
        cur_tail = mem_ring->cons.tail;
        elm_num = mem_ring->size;
      }
      break;
    case NSFW_RES_SCAN_ARRAY:
      {
        struct nsfw_mem_ring *mem_ring =
          (struct nsfw_mem_ring *) scn_cfg->mgr_ring;
        if (NULL == mem_ring)
          {
            return FALSE;
          }

        cur_head = mem_ring->prod.head;
        cur_tail = mem_ring->cons.tail;
        elm_num = scn_cfg->total_num;
      }
      break;
    default:
      return FALSE;
    }

  free_count = nsfw_get_alloc_count (cur_head, cur_tail);

  res_scn_item->cons_head = cur_head;
  res_scn_item->prod_head = cur_tail;
  if (0 != elm_num)
    {
      res_scn_item->free_percent = free_count * 100 / elm_num;
    }
  else
    {
      res_scn_item->free_percent = 100;
    }

  scn_cfg->total_num = elm_num;
  return TRUE;
}

void
nsfw_res_scan_mem (nsfw_res_mgr_item_cfg * res_scn_item)
{
  if (NULL == res_scn_item)
    {
      return;
    }

  nsfw_res_scn_cfg *scn_cfg = &res_scn_item->scn_cfg;
  if (NULL == scn_cfg->data)
    {
      return;
    }

  u32 start = res_scn_item->last_scn_idx;
  u32 end = start + scn_cfg->num_per_cyc;
  int res_chk_number = 0;
  if (NSFW_RES_SCAN_SPOOL == scn_cfg->type)
    {
      res_chk_number =
        nsfw_mem_sp_iterator (scn_cfg->data, start, end,
                              nsfw_res_sp_item_chk, (void *) res_scn_item);
    }
  else
    {
      res_chk_number =
        nsfw_mem_mbuf_iterator (scn_cfg->data, start, end,
                                nsfw_res_sp_item_chk, (void *) res_scn_item);
    }

  if (0 == res_chk_number)
    {
      res_scn_item->last_scn_idx = 0;
      start = res_scn_item->last_scn_idx;
      end = start + scn_cfg->num_per_cyc;
      if (NSFW_RES_SCAN_SPOOL == scn_cfg->type)
        {
          res_chk_number =
            nsfw_mem_sp_iterator (scn_cfg->data, start, end,
                                  nsfw_res_sp_item_chk,
                                  (void *) res_scn_item);
        }
      else
        {
          res_chk_number =
            nsfw_mem_mbuf_iterator (scn_cfg->data, start, end,
                                    nsfw_res_sp_item_chk,
                                    (void *) res_scn_item);
        }
    }

  if (res_chk_number + start < end)
    {
      res_scn_item->last_scn_idx = 0;
    }
  else
    {
      res_scn_item->last_scn_idx += res_chk_number;
    }

  return;
}

void
nsfw_res_scan_array (nsfw_res_mgr_item_cfg * res_scn_item)
{
  if (NULL == res_scn_item)
    {
      return;
    }

  nsfw_res_scn_cfg *scn_cfg = &res_scn_item->scn_cfg;
  if (NULL == scn_cfg->data)
    {
      return;
    }

  u32 i;
  char *elm =
    (char *) scn_cfg->data + (res_scn_item->last_scn_idx * scn_cfg->elm_size);
  for (i = res_scn_item->last_scn_idx; i < scn_cfg->total_num; i++)
    {
      if (i >= res_scn_item->last_scn_idx + scn_cfg->num_per_cyc)
        {
          break;
        }

      if (TRUE == nsfw_res_sp_item_chk (elm, (void *) res_scn_item))
        {
          NSFW_LOGINF ("force free item]data=%p,cfg=%p", elm, res_scn_item);
        }

      elm += scn_cfg->elm_size;
    }

  if (i >= scn_cfg->total_num)
    {
      res_scn_item->last_scn_idx = 0;
    }
  else
    {
      res_scn_item->last_scn_idx = i;
    }

  return;
}

void
nsfw_res_scan_proc (nsfw_res_mgr_item_cfg * res_scn_item)
{
  (void) nsfw_res_flash_data (res_scn_item);
  switch (res_scn_item->scn_cfg.type)
    {
    case NSFW_RES_SCAN_ARRAY:
      nsfw_res_scan_array (res_scn_item);
      break;
    case NSFW_RES_SCAN_SPOOL:
    case NSFW_RES_SCAN_MBUF:
      nsfw_res_scan_mem (res_scn_item);
      break;
    default:
      break;
    }
}

int
nsfw_res_scan_all (u32 timer_type, void *data)
{
  NSFW_LOGDBG ("scan start!");
  struct timespec time_left = { NSFW_RES_SCAN_TVLAUE, 0 };
  g_scan_cfg.scan_timer =
    nsfw_timer_reg_timer (0, NULL, nsfw_res_scan_all, time_left);

  if (g_hbt_switch)
    {
      return TRUE;
    }

  int i;
  for (i = 0; i < NSFW_MAX_RES_SCAN_COUNT; i++)
    {
      /*last fun */
      if (NULL == g_all_res_can[i].scn_cfg.data)
        {
          break;
        }

      nsfw_res_scan_proc (&g_all_res_can[i]);
    }

  return TRUE;
}

static int nsfw_resmgr_module_init (void *param);
static int
nsfw_resmgr_module_init (void *param)
{
  u8 proc_type = (u8) ((long long) param);
  NSFW_LOGINF ("res mgr module init]type=%u", proc_type);
  g_scan_cfg.scan_tvalue = NSFW_RES_SCAN_TVLAUE_DEF;
  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
      {
        struct timespec time_left = { NSFW_RES_SCAN_TVLAUE, 0 };
        g_scan_cfg.scan_timer =
          nsfw_timer_reg_timer (0, NULL, nsfw_res_scan_all, time_left);
        return 0;
      }
    default:
      if (proc_type < NSFW_PROC_MAX)
        {
          break;
        }
      return -1;
    }

  return 0;
}

/* *INDENT-OFF* */
NSFW_MODULE_NAME(NSFW_RES_MGR_MODULE)
NSFW_MODULE_PRIORITY(99)
NSFW_MODULE_INIT(nsfw_resmgr_module_init)
/* *INDENT-ON* */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
