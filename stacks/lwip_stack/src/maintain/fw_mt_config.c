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

#include "types.h"
#include "nsfw_mt_config.h"
#include <stdlib.h>
#include <pthread.h>
#include "nstack_log.h"
#include "nstack_securec.h"
#include "json.h"
#include "nsfw_init.h"
#include "nsfw_mgr_com_api.h"

/*==============================================*
 *      project-wide global variables           *
 *----------------------------------------------*/

// can be read from config file
u32 g_base_cfg_items[MAX_BASE_CFG] = { 0 };

// calculated according to base config
u32 g_custom_cfg_items[MAX_CUSTOM_CFG] = { 0 };

// note: if seg number greater than 16, such buffer should use malloc
// or it will be exceed 2K
struct cfg_item_info g_cfg_item_info[CFG_SEG_MAX][MAX_CFG_ITEM];

NSTACK_STATIC int g_cfg_item_count[CFG_SEG_MAX] = { 0 };

NSTACK_STATIC char *g_cfg_seg_name[CFG_SEG_MAX];

/*==============================================*
 *      routines' or functions' implementations *
 *----------------------------------------------*/

/* nStackCtrl cannot get the env path info, no start by shell script, need get
   the path info ,and legal check , add a input parameter proc_type*/

NSTACK_STATIC int
get_ctrl_dir_info (char *current_path, unsigned path_len)
{
  char ctrl_dir[MAX_FILE_NAME_LEN] = { 0 };
  int count = 0;
  unsigned int dir_len = 0;

  //nStackCtrl cannot get the path from the env, so need get from current pwd.
  count = readlink ("/proc/self/exe", ctrl_dir, MAX_FILE_NAME_LEN);
  if ((count < 0) || (count >= MAX_FILE_NAME_LEN))
    {
      save_pre_init_log (NSLOG_ERR,
                         "readlink get nStackCtrl path failed, write nothing!");
      return -1;
    }
  ctrl_dir[count] = '\0';

  dir_len = strlen (ctrl_dir);
  if ((dir_len > strlen ("nStackCtrl")) && (dir_len < MAX_FILE_NAME_LEN))
    {
      ctrl_dir[dir_len - strlen ("nStackCtrl")] = '\0';
    }
  else
    {
      save_pre_init_log (NSLOG_ERR, "path strlen is illegal, write nothing!");
      return -1;
    }

  if (NULL == strstr (ctrl_dir, "bin"))
    {
      /* Exit before nstack_log_init, use printf */
      printf
        ("the nStackServer content change, plz keep same with nStack release, exit!\n");
#ifdef FOR_NSTACK_UT
      return -1;
#else
      exit (1);
#endif
    }
  if (EOK !=
      STRNCAT_S (ctrl_dir, sizeof (ctrl_dir), "/../configure",
                 strlen ("/../configure")))
    {
      save_pre_init_log (NSLOG_ERR, "STRNCAT_S failed, current_dir = %s",
                         ctrl_dir);
      return -1;
    }

  if (-1 == SNPRINTF_S (current_path, path_len, path_len - 1, "%s", ctrl_dir))
    {
      save_pre_init_log (NSLOG_ERR,
                         "SNPRINTF_S path name failed, ctrl_dir %s.",
                         ctrl_dir);
      return -1;
    }

  return 0;

}

NSTACK_STATIC int
get_cfg_buf (u32 proc_type, char *cfg_buf, unsigned buf_size)
{
  char current_dir[MAX_FILE_NAME_LEN] = { 0 };
  char cfg_file_name[MAX_FILE_NAME_LEN] = { 0 };
  char *cfg_resolved_path = NULL;
  char *cfg_path = NULL;
  FILE *fp = NULL;
  int cfg_buf_len = 0;

  cfg_path = getenv (CFG_PATH);
  if ((NULL == cfg_path) && (NSFW_PROC_CTRL == proc_type))
    {
      if (-1 == get_ctrl_dir_info (current_dir, sizeof (current_dir)))
        {
          save_pre_init_log (NSLOG_ERR, "get_ctrl_dir_info failed.");
          return -1;
        }
      cfg_path = current_dir;
    }
  else if ((NULL == cfg_path) && (NSFW_PROC_CTRL != proc_type))
    {
      save_pre_init_log (NSLOG_ERR,
                         "main or master process get nstack config path failed, will use default config!");
      return -1;
    }

  if (-1 ==
      SPRINTF_S (cfg_file_name, sizeof (cfg_file_name), "%s/%s", cfg_path,
                 CFG_FILE_NAME))
    {
      save_pre_init_log (NSLOG_ERR,
                         "format config file name failed, path %s, name %s.",
                         cfg_path, CFG_FILE_NAME);
      return -1;
    }

  cfg_resolved_path = realpath (cfg_file_name, NULL);
  if (NULL == cfg_resolved_path)
    {
      save_pre_init_log (NSLOG_ERR, "config file path invalid, cfg name %s.",
                         cfg_file_name);
      return -1;
    }

  fp = fopen (cfg_resolved_path, "r");
  if (NULL == fp)
    {
      free (cfg_resolved_path);
      save_pre_init_log (NSLOG_ERR, "config file path invalid, cfg name %s.",
                         cfg_file_name);
      return -1;
    }

  free (cfg_resolved_path);
  // read config file to json buffer
  cfg_buf_len = fread (cfg_buf, 1, buf_size, fp);

  fclose (fp);

  return cfg_buf_len;
}

NSTACK_STATIC inline int
get_value_from_json_obj (struct json_object *obj, struct cfg_item_info *pitem)
{
  switch (pitem->type)
    {
    case CFG_ITEM_TYPE_INT:
      pitem->value = json_object_get_int (obj);
      break;
    case CFG_ITEM_TYPE_STRING:
      pitem->pvalue = (char *) json_object_get_string (obj);
      break;
    default:
      // print log here?
      return -1;
    }
  return 0;
}

NSTACK_STATIC inline void
get_cfg_item (struct json_object *obj, int seg_index)
{
  struct json_object *cfg_seg_obj = NULL;
  struct json_object *cfg_seg = NULL;
  struct json_object *cfg_item_obj[MAX_CFG_ITEM] = { 0 };
  int i = 0;
  int cfg_num = 0;

  (void) json_object_object_get_ex (obj, g_cfg_seg_name[seg_index],
                                    &cfg_seg_obj);
  if (NULL == cfg_seg_obj)
    {
      save_pre_init_log (NSLOG_ERR, "get config segment obj failed, seg:%s.",
                         g_cfg_seg_name[seg_index]);
      return;
    }

  cfg_num = json_object_array_length (cfg_seg_obj);
  if (cfg_num < 1)
    {
      save_pre_init_log (NSLOG_ERR,
                         "config segment count invalid, config segment %s, count %d.",
                         g_cfg_seg_name[seg_index], cfg_num);
      return;
    }

  // each config segment just has 1 array element
  cfg_seg = (struct json_object *) json_object_array_get_idx (cfg_seg_obj, 0);
  if (NULL == cfg_seg)
    {
      save_pre_init_log (NSLOG_ERR, "no config item in seg %s.",
                         g_cfg_seg_name[seg_index]);
      return;
    }

  for (; i < g_cfg_item_count[seg_index]; i++)
    {
      (void) json_object_object_get_ex (cfg_seg,
                                        g_cfg_item_info[seg_index][i].name,
                                        &cfg_item_obj[i]);

      if (NULL == cfg_item_obj[i])
        {
          save_pre_init_log (NSLOG_ERR,
                             "get config item failed, config item %s.",
                             g_cfg_item_info[seg_index][i].name);
          return;
        }

      // note: should specify the config item type if not only int item exist
      if (get_value_from_json_obj
          (cfg_item_obj[i], &g_cfg_item_info[seg_index][i]) != 0)
        {
          return;
        }
    }

  return;
}

NSTACK_STATIC inline void
parse_cfg (char *cfg_buf)
{
  if (NULL == cfg_buf)
    {
      return;
    }

  struct json_object *obj =
    (struct json_object *) json_tokener_parse (cfg_buf);
  int i = 0;

  for (; i < CFG_SEG_MAX; i++)
    {
      if (0 == g_cfg_item_count[i])
        {
          continue;
        }

      get_cfg_item (obj, i);
    }
}

NSTACK_STATIC inline int
is_valid (int value, int min_value, int max_value)
{
  if ((value < min_value) || (value > max_value))
    {
      return 0;
    }

  return 1;
}

NSTACK_STATIC inline void
check_cfg_item_int (struct cfg_item_info *pitem)
{
  if (!is_valid (pitem->value, pitem->min_value, pitem->max_value))
    {
      pitem->value = pitem->default_value;
    }
}

NSTACK_STATIC inline void
check_cfg_item_string (struct cfg_item_info *pitem)
{
  if ((NULL == pitem->pvalue) || ((pitem->pvalue) && (0 == pitem->pvalue[0])))
    {
      pitem->pvalue = pitem->default_str;
    }
}

NSTACK_STATIC inline void
check_cfg_item (struct cfg_item_info *pitem)
{
  switch (pitem->type)
    {
    case CFG_ITEM_TYPE_INT:
      check_cfg_item_int (pitem);
      if (pitem->custom_check)
        pitem->custom_check (pitem);
      break;
    case CFG_ITEM_TYPE_STRING:
      check_cfg_item_string (pitem);
      if (pitem->custom_check)
        pitem->custom_check (pitem);
      break;
    default:
      break;
    }
}

NSTACK_STATIC inline void
check_cfg ()
{
  int i = 0;
  int j = 0;
  for (i = 0; i < CFG_SEG_MAX; i++)
    {
      for (j = 0; j < g_cfg_item_count[i]; j++)
        {
          check_cfg_item (&g_cfg_item_info[i][j]);
        }
    }
}

NSTACK_STATIC inline void
print_item_info (char *seg_name, struct cfg_item_info *pitem)
{
  switch (pitem->type)
    {
    case CFG_ITEM_TYPE_INT:
      save_pre_init_log (NSLOG_INF, "config read seg:%s, name:%s, value:%d.",
                         seg_name, pitem->name, pitem->value);
      break;
    case CFG_ITEM_TYPE_STRING:
      save_pre_init_log (NSLOG_INF, "config read seg:%s, name:%s, pvalue:%s.",
                         seg_name, pitem->name, pitem->pvalue);
      break;
    default:
      break;
    }
}

NSTACK_STATIC inline void
print_config_item_info ()
{
  int i = 0;
  int j = 0;
  for (; i < CFG_SEG_MAX; i++)
    {
      for (j = 0; j < g_cfg_item_count[i]; j++)
        {
          print_item_info (g_cfg_seg_name[i], &g_cfg_item_info[i][j]);
        }
    }
}

void
check_socket_config (void *pitem)
{
  struct cfg_item_info *item = (struct cfg_item_info *) pitem;
  if (item->value > 0 && !(item->value & (item->value - 1)))
    return;
  save_pre_init_log (NSLOG_WAR,
                     "warning: config socket_num (%u) is not 2^n, will use the default value:%u",
                     item->value, item->default_value);
  item->value = item->default_value;
}

/* thread schedule mode and thread priority should be matched */
void
check_thread_config (void *pitem)
{
  struct cfg_item_info *pri_cfg = (struct cfg_item_info *) pitem;
  struct cfg_item_info *policy_cfg =
    &g_cfg_item_info[CFG_SEG_PRI][CFG_SEG_THREAD_PRI_POLICY];

  int max_pri = sched_get_priority_max (policy_cfg->value);
  int min_pri = sched_get_priority_min (policy_cfg->value);
  if ((pri_cfg->value > max_pri) || (pri_cfg->value < min_pri))
    {
      save_pre_init_log (NSLOG_INF,
                         "detect invalid thread priority configuration, use default value] policy=%d, pri=%d, def policy=%d, def pri=%d",
                         policy_cfg->value, pri_cfg->value,
                         policy_cfg->default_value, pri_cfg->default_value);

      policy_cfg->value = policy_cfg->default_value;
      pri_cfg->value = pri_cfg->default_value;
    }
}

#define SET_CFG_ITEM(seg, item, field, value)  g_cfg_item_info[seg][item].field = (value)
#define SET_THREAD_CFG_ITEM(item, field, value) SET_CFG_ITEM(CFG_SEG_PRI, item, field, value)

NSTACK_STATIC void
init_main_def_config_items ()
{
  /* base config */
  g_cfg_seg_name[CFG_SEG_BASE] = "cfg_seg_socket";
  g_cfg_item_count[CFG_SEG_BASE] = CFG_SEG_BASE_MAX;
  /* -- socket number */
  g_cfg_item_info[CFG_SEG_BASE][CFG_SEG_BASE_SOCKET_NUM].name = "socket_num";
  g_cfg_item_info[CFG_SEG_BASE][CFG_SEG_BASE_SOCKET_NUM].type =
    CFG_ITEM_TYPE_INT;
  g_cfg_item_info[CFG_SEG_BASE][CFG_SEG_BASE_SOCKET_NUM].custom_check =
    check_socket_config;
  set_cfg_info (CFG_SEG_BASE, CFG_SEG_BASE_SOCKET_NUM, MIN_SOCKET_NUM,
                MAX_SOCKET_NUM, DEF_SOCKET_NUM);
  /* -- arp stale time */
  g_cfg_item_info[CFG_SEG_BASE][CFG_SEG_BASE_ARP_STALE_TIME].name =
    "arp_stale_time";
  g_cfg_item_info[CFG_SEG_BASE][CFG_SEG_BASE_ARP_STALE_TIME].type =
    CFG_ITEM_TYPE_INT;
  g_cfg_item_info[CFG_SEG_BASE][CFG_SEG_BASE_ARP_STALE_TIME].custom_check =
    NULL;
  set_cfg_info (CFG_SEG_BASE, CFG_SEG_BASE_ARP_STALE_TIME,
                MIN_ARP_STACLE_TIME, MAX_ARP_STACLE_TIME,
                DEF_ARP_STACLE_TIME);
  /* -- arp braodcast retransmission times */
  g_cfg_item_info[CFG_SEG_BASE][CFG_SEG_BASE_ARP_BC_RETRANS_NUM].name =
    "arp_bc_retrans_num";
  g_cfg_item_info[CFG_SEG_BASE][CFG_SEG_BASE_ARP_BC_RETRANS_NUM].type =
    CFG_ITEM_TYPE_INT;
  g_cfg_item_info[CFG_SEG_BASE][CFG_SEG_BASE_ARP_BC_RETRANS_NUM].custom_check
    = NULL;
  set_cfg_info (CFG_SEG_BASE, CFG_SEG_BASE_ARP_BC_RETRANS_NUM,
                MIN_ARP_BC_RETRANS_NUM, MAX_ARP_BC_RETRANS_NUM,
                DEF_ARP_BC_RETRANS_NUM);

  /* support thread priority configuration */
  g_cfg_seg_name[CFG_SEG_PRI] = "cfg_seg_thread_pri";
  g_cfg_item_count[CFG_SEG_PRI] = CFG_SEG_THREAD_PRI_MAX;
  SET_THREAD_CFG_ITEM (CFG_SEG_THREAD_PRI_POLICY, name, "sched_policy");
  SET_THREAD_CFG_ITEM (CFG_SEG_THREAD_PRI_POLICY, type, CFG_ITEM_TYPE_INT);
  SET_THREAD_CFG_ITEM (CFG_SEG_THREAD_PRI_POLICY, custom_check, NULL);
  set_cfg_info (CFG_SEG_PRI, CFG_SEG_THREAD_PRI_POLICY, 0, 2, 0);

  SET_THREAD_CFG_ITEM (CFG_SEG_THREAD_PRI_PRI, name, "thread_pri");
  SET_THREAD_CFG_ITEM (CFG_SEG_THREAD_PRI_PRI, type, CFG_ITEM_TYPE_INT);
  SET_THREAD_CFG_ITEM (CFG_SEG_THREAD_PRI_PRI, custom_check,
                       check_thread_config);
  set_cfg_info (CFG_SEG_PRI, CFG_SEG_THREAD_PRI_PRI, 0, 99, 0);

  /* remove unsed operation config set */
  /* log config */
  g_cfg_seg_name[CFG_SEG_LOG] = "cfg_seg_log";
  g_cfg_item_count[CFG_SEG_LOG] = 2;
  g_cfg_item_info[CFG_SEG_LOG][0].name = "run_log_size";
  g_cfg_item_info[CFG_SEG_LOG][0].type = CFG_ITEM_TYPE_INT;
  g_cfg_item_info[CFG_SEG_LOG][0].custom_check = NULL;
  g_cfg_item_info[CFG_SEG_LOG][1].name = "run_log_count";
  g_cfg_item_info[CFG_SEG_LOG][1].type = CFG_ITEM_TYPE_INT;
  g_cfg_item_info[CFG_SEG_LOG][1].custom_check = NULL;
  set_cfg_info (CFG_SEG_LOG, 0, 10, 100, 50);
  set_cfg_info (CFG_SEG_LOG, 1, 2, 20, 10);

  /* path config */
  /* set the path string and default str */
  g_cfg_seg_name[CFG_SEG_PATH] = "cfg_seg_path";
  g_cfg_item_count[CFG_SEG_PATH] = 1;
  g_cfg_item_info[CFG_SEG_PATH][0].name = "stackx_log_path";
  g_cfg_item_info[CFG_SEG_PATH][0].type = CFG_ITEM_TYPE_STRING;
  g_cfg_item_info[CFG_SEG_PATH][0].default_str = NSTACK_LOG_NAME;
  g_cfg_item_info[CFG_SEG_PATH][0].custom_check = NULL;
}

/*  master and ctrl both use the function to reduce the redundancy,
*   as the parameter and operation all same.
*/
NSTACK_STATIC void
init_master_def_config_items ()
{
  int i = 0;
  for (; i < CFG_SEG_MAX; i++)
    {
      if (i != CFG_SEG_LOG)
        {
          g_cfg_item_count[i] = 0;
        }
    }

  g_cfg_seg_name[CFG_SEG_LOG] = "cfg_seg_log";
  g_cfg_item_count[CFG_SEG_LOG] = 2;
  g_cfg_item_info[CFG_SEG_LOG][0].name = "mon_log_size";
  g_cfg_item_info[CFG_SEG_LOG][0].type = CFG_ITEM_TYPE_INT;
  g_cfg_item_info[CFG_SEG_LOG][0].custom_check = NULL;
  g_cfg_item_info[CFG_SEG_LOG][1].name = "mon_log_count";
  g_cfg_item_info[CFG_SEG_LOG][1].type = CFG_ITEM_TYPE_INT;
  g_cfg_item_info[CFG_SEG_LOG][1].custom_check = NULL;

  set_cfg_info (CFG_SEG_LOG, 0, 2, 20, 10);
  set_cfg_info (CFG_SEG_LOG, 1, 2, 20, 10);

  g_cfg_seg_name[CFG_SEG_PATH] = "cfg_seg_path";
  g_cfg_item_count[CFG_SEG_PATH] = 1;
  g_cfg_item_info[CFG_SEG_PATH][0].name = "master_log_path";
  g_cfg_item_info[CFG_SEG_PATH][0].type = CFG_ITEM_TYPE_STRING;
  g_cfg_item_info[CFG_SEG_PATH][0].default_str = NSTACK_LOG_NAME;
  g_cfg_item_info[CFG_SEG_PATH][0].custom_check = NULL;
}

NSTACK_STATIC void
read_init_config (u32 proc_type)
{
  int cfg_buf_len = 0;
  char cfg_json_buf[CFG_BUFFER_LEN] = { 0 };

  cfg_buf_len = get_cfg_buf (proc_type, cfg_json_buf, sizeof (cfg_json_buf));
  if (cfg_buf_len < 0)
    {
      save_pre_init_log (NSLOG_WAR,
                         "warning:file not exist, use default config.");
      return;
    }
  else
    {
      /* parse json buffer */
      parse_cfg (cfg_json_buf);
    }
  save_pre_init_log (NSLOG_INF, "read configuration finished.");
}

/* =========== set config items ========= */
NSTACK_STATIC inline void
set_base_config ()
{
  g_base_cfg_items[CFG_BASE_RING_SIZE] = DEF_RING_BASE_SIZE;
  g_base_cfg_items[CFG_BASE_HAL_PORT_NUM] = DEF_HAL_PORT_NUM;

  g_base_cfg_items[CFG_BASE_SOCKET_NUM] =
    (u32) get_cfg_info (CFG_SEG_BASE, CFG_SEG_BASE_SOCKET_NUM);
  g_base_cfg_items[CFG_BASE_ARP_STALE_TIME] =
    (u32) get_cfg_info (CFG_SEG_BASE, CFG_SEG_BASE_ARP_STALE_TIME);
  g_base_cfg_items[CFG_BASE_ARP_BC_RETRANS_NUM] =
    (u32) get_cfg_info (CFG_SEG_BASE, CFG_SEG_BASE_ARP_BC_RETRANS_NUM);
}

NSTACK_STATIC void
init_base_config (cfg_module_param * param)
{
  /* initial default config */
  /* omc_ctrl single log file should be 10M */
  if (param->proc_type == NSFW_PROC_CTRL)
    {
      init_master_def_config_items ();
    }
  else
    {
      init_main_def_config_items ();
    }

  /* read base config from file */
  read_init_config (param->proc_type);
  /* check config and reset value */
  check_cfg ();

  /* print config info */
  print_config_item_info ();

  set_base_config ();
}

NSTACK_STATIC void
init_stackx_config ()
{
  u32 socket_num_per_thread = CUR_CFG_SOCKET_NUM;
  u32 factor = socket_num_per_thread / SOCKET_NUM_PER_THREAD;

  if (factor == 0 || socket_num_per_thread % SOCKET_NUM_PER_THREAD > 0)
    {
      factor += 1;
    }

  save_pre_init_log (NSLOG_INF, "socket num:%d, factor:%d",
                     CUR_CFG_SOCKET_NUM, factor);

  /* MBUF config */
  set_custom_cfg_item (CFG_MBUF_DATA_SIZE, DEF_MBUF_DATA_SIZE);
  set_custom_cfg_item (CFG_TX_MBUF_NUM, DEF_TX_MBUF_POOL_SIZE);
  set_custom_cfg_item (CFG_RX_MBUF_NUM, DEF_RX_MBUF_POOL_SIZE);
  set_custom_cfg_item (CFG_MP_TCPSEG_NUM, DEF_MEMP_NUM_TCP_SEG);        /* tcp segment number */
  set_custom_cfg_item (CFG_MP_MSG_NUM, DEF_TX_MSG_POOL_SIZE);   /* msg number */

  /* ring config */
  set_custom_cfg_item (CFG_HAL_RX_RING_SIZE, DEF_HAL_RX_RING_SIZE);     /* netif ring size not changed */
  set_custom_cfg_item (CFG_HAL_TX_RING_SIZE, DEF_HAL_TX_RING_SIZE);     /* netif ring size not changed */
  set_custom_cfg_item (CFG_MBOX_RING_SIZE, DEF_MBOX_RING_SIZE);
  set_custom_cfg_item (CFG_SPL_MAX_RING_SIZE, DEF_SPL_MAX_RING_SIZE);   /* stackx ring size */

  /* pcb config */
  set_custom_cfg_item (CFG_TCP_PCB_NUM, DEF_TCP_PCB_NUM * factor);
  set_custom_cfg_item (CFG_UDP_PCB_NUM, DEF_UDP_PCB_NUM * factor);
  set_custom_cfg_item (CFG_RAW_PCB_NUM, DEF_RAW_PCB_NUM * factor);
  set_custom_cfg_item (CFG_ARP_QUEUE_NUM,
                       CUR_CFG_SOCKET_NUM >
                       DEF_SOCKET_NUM ? LARGE_ARP_QUEUE_NUM :
                       DEF_ARP_QUEUE_NUM);
}

void
print_final_config_para ()
{
  save_pre_init_log (NSLOG_INF, "socket_num          :%u",
                     get_base_cfg (CFG_BASE_SOCKET_NUM));
  save_pre_init_log (NSLOG_INF, "base_ring_size      :%u",
                     get_base_cfg (CFG_BASE_RING_SIZE));
  save_pre_init_log (NSLOG_INF, "hal_port_num        :%u",
                     get_base_cfg (CFG_BASE_HAL_PORT_NUM));
  save_pre_init_log (NSLOG_INF, "arp_stale_num       :%u",
                     get_base_cfg (CFG_BASE_ARP_STALE_TIME));
  save_pre_init_log (NSLOG_INF, "arp_bc_retrans_num  :%u",
                     get_base_cfg (CFG_BASE_ARP_BC_RETRANS_NUM));

  save_pre_init_log (NSLOG_INF, "mbuf_data_size      :%u",
                     get_custom_cfg (CFG_MBUF_DATA_SIZE));
  save_pre_init_log (NSLOG_INF, "tx_mbuf_num         :%u",
                     get_custom_cfg (CFG_TX_MBUF_NUM));
  save_pre_init_log (NSLOG_INF, "rx_mbuf_num         :%u",
                     get_custom_cfg (CFG_RX_MBUF_NUM));
  save_pre_init_log (NSLOG_INF, "tcp_seg_mp_num      :%u",
                     get_custom_cfg (CFG_MP_TCPSEG_NUM));
  save_pre_init_log (NSLOG_INF, "msg_mp_num          :%u",
                     get_custom_cfg (CFG_MP_MSG_NUM));
  save_pre_init_log (NSLOG_INF, "hal_tx_ring_size    :%u",
                     get_custom_cfg (CFG_HAL_TX_RING_SIZE));
  save_pre_init_log (NSLOG_INF, "hal_rx_ring_size    :%u",
                     get_custom_cfg (CFG_HAL_RX_RING_SIZE));
  save_pre_init_log (NSLOG_INF, "mbox_ring_size      :%u",
                     get_custom_cfg (CFG_MBOX_RING_SIZE));
  save_pre_init_log (NSLOG_INF, "spl_ring_size       :%u",
                     get_custom_cfg (CFG_SPL_MAX_RING_SIZE));
  save_pre_init_log (NSLOG_INF, "tcp_pcb_num         :%u",
                     get_custom_cfg (CFG_TCP_PCB_NUM));
  save_pre_init_log (NSLOG_INF, "udp_pcb_num         :%u",
                     get_custom_cfg (CFG_UDP_PCB_NUM));
  save_pre_init_log (NSLOG_INF, "raw_pcb_num         :%u",
                     get_custom_cfg (CFG_RAW_PCB_NUM));
}

NSTACK_STATIC void
init_module_cfg_default ()
{
  init_stackx_config ();

  print_final_config_para ();
}

NSTACK_STATIC void
init_main_log_cfg_para ()
{
  struct log_init_para log_para;
  log_para.run_log_size = g_cfg_item_info[CFG_SEG_LOG][0].value;
  log_para.run_log_count = g_cfg_item_info[CFG_SEG_LOG][1].value;

  /* log path valid check */
  if (0 == access (g_cfg_item_info[CFG_SEG_PATH][0].pvalue, W_OK))
    {
      log_para.run_log_path = g_cfg_item_info[CFG_SEG_PATH][0].pvalue;
    }
  else
    {
      log_para.run_log_path = g_cfg_item_info[CFG_SEG_PATH][0].default_str;
    }

  set_log_init_para (&log_para);
}

/* nStackCtrl is the diff process with main, cannot use main process info,
   need get the configure info respectively */
/* omc_ctrl single log file should be 10M */
NSTACK_STATIC void
init_ctrl_log_cfg_para ()
{
  struct log_init_para log_para;
  log_para.mon_log_size = g_cfg_item_info[CFG_SEG_LOG][0].value;
  log_para.mon_log_count = g_cfg_item_info[CFG_SEG_LOG][1].value;

  /* log path valid check */
  if (0 == access (g_cfg_item_info[CFG_SEG_PATH][0].pvalue, W_OK))
    {
      log_para.mon_log_path = g_cfg_item_info[CFG_SEG_PATH][0].pvalue;
    }
  else
    {
      log_para.mon_log_path = g_cfg_item_info[CFG_SEG_PATH][0].default_str;
    }

  set_log_init_para (&log_para);
}

/*===========config init for nstack main=============*/

NSTACK_STATIC void
init_module_cfg_nstackmain ()
{
  /* init config data */
  init_module_cfg_default ();

  /* init log para */
  init_main_log_cfg_para ();
}

/*===========config init for nstack ctrl=============*/

/* nStackCtrl is the diff process with main,
   cannot use main process info, need get the configure info respectively */

NSTACK_STATIC void
init_module_cfg_nstackctrl ()
{
  init_ctrl_log_cfg_para ();
}

/*===========init config module=============*/
void
config_module_init (cfg_module_param * param)
{
  save_pre_init_log (NSLOG_INF, "config module init begin] proc type=%d",
                     param->proc_type);

  init_base_config (param);

  switch (param->proc_type)
    {
    case NSFW_PROC_MAIN:
      init_module_cfg_nstackmain ();
      break;

    case NSFW_PROC_CTRL:
      init_module_cfg_nstackctrl ();
      break;

    default:
      init_module_cfg_default ();
      break;
    }

  save_pre_init_log (NSLOG_INF, "config module init end.");
}

u32
get_cfg_share_mem_size ()
{
  return sizeof (g_base_cfg_items) + sizeof (g_custom_cfg_items);
}

int
get_share_cfg_from_mem (void *mem)
{
  if (EOK !=
      MEMCPY_S (g_base_cfg_items, sizeof (g_base_cfg_items), mem,
                sizeof (g_base_cfg_items)))
    {
      return -1;
    }

  char *custom_cfg_mem = (char *) mem + sizeof (g_base_cfg_items);

  if (EOK !=
      MEMCPY_S (g_custom_cfg_items, sizeof (g_custom_cfg_items),
                custom_cfg_mem, sizeof (g_custom_cfg_items)))
    {
      return -1;
    }

  return 0;
}

int
set_share_cfg_to_mem (void *mem)
{
  if (EOK !=
      MEMCPY_S (mem, sizeof (g_base_cfg_items), g_base_cfg_items,
                sizeof (g_base_cfg_items)))
    {
      return -1;
    }

  char *custom_cfg_mem = (char *) mem + sizeof (g_base_cfg_items);

  if (EOK !=
      MEMCPY_S (custom_cfg_mem, sizeof (g_custom_cfg_items),
                g_custom_cfg_items, sizeof (g_custom_cfg_items)))
    {
      return -1;
    }

  return 0;
}
