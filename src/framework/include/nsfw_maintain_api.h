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

#ifndef _NSFW_MEM_STAT_API_H
#define _NSFW_MEM_STAT_API_H

#include "types.h"
#include "nsfw_mgr_com_api.h"
#include "compiling_check.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

/*################MEM_STAT######################*/
#define NSFW_MEM_MODULE_LEN 32
#define NSFW_MEM_NAME_LEN   64

#define OMC_PROC_MM "omc_proc_maintain"

#define MEM_STAT(module, mem_name, mem_type, mem_size)\
    nsfw_mem_stat(module, mem_name, mem_type, mem_size)

extern void nsfw_mem_stat (char *module, char *mem_name, u8 mem_type,
                           u32 mem_size);
extern void nsfw_mem_stat_print ();
/*##############################################*/

/*################SRV_CTRL######################*/
typedef enum _nsfw_srv_ctrl_state
{
  NSFW_SRV_CTRL_RESUME = 1,
  NSFW_SRV_CTRL_SUSPEND = 2
} nsfw_srv_ctrl_state;

typedef struct _nsfw_srv_ctrl_msg
{
  nsfw_srv_ctrl_state srv_state;
  u16 rsp_code;
} nsfw_srv_ctrl_msg;
extern u8 nsfw_srv_ctrl_send (nsfw_srv_ctrl_state state, u8 rsp_flag);
/*#############################################*/

/*#################RES_MGR######################*/
#define NSFW_RES_MGR_MODULE "nsfw_res_mgr"

typedef enum _nsfw_res_scan_type
{
  NSFW_RES_SCAN_ARRAY = 0,
  NSFW_RES_SCAN_SPOOL,
  NSFW_RES_SCAN_MBUF,
  NSFW_RES_SCAN_MAX
} nsfw_res_scan_type;

typedef int (*nsfw_res_free_fun) (void *pdata);

typedef struct _nsfw_res_scn_cfg
{
  u8 type;                      /*nsfw_res_scan_type */
  u8 force_free_percent;        /*if the resource free percent below this value, begin to force free the element */
  u16 force_free_chk_num;       /*if the check count beyone this value, call free fun release this element */
  u16 alloc_speed_factor;       /*alloc fast with higher value */

  u32 num_per_cyc;              /*define the element number in one scan cycle process and increase chk_count of every element */
  u32 total_num;                /*total number of elements */
  u32 elm_size;                 /*element size */
  u32 res_mem_offset;           /*the nsfw_res offset from the element start */

  void *data;                   /*the array addr or spool addr */
  void *mgr_ring;

  nsfw_res_free_fun free_fun;
} nsfw_res_scn_cfg;

typedef struct _nsfw_res_mgr_item_cfg
{
  nsfw_res_scn_cfg scn_cfg;
  u32 cons_head;
  u32 prod_head;
  u32 free_percent;
  u32 last_scn_idx;
  u64 force_count;
} nsfw_res_mgr_item_cfg;

#define NSFW_MAX_RES_SCAN_COUNT 256

extern u8 nsfw_res_mgr_reg (nsfw_res_scn_cfg * cfg);
extern i32 nsfw_proc_start_with_lock (u8 proc_type);
/*#############################################*/

typedef enum _nsfw_exit_code
{
  NSFW_EXIT_SUCCESS = 0,
  NSFW_EXIT_FAILED = 1,
  NSFW_EXIT_DST_ERROR = 2,
  NSFW_EXIT_TIME_OUT = 3,

  NSFW_EXIT_MAX_COM_ERR = 31,
} nsfw_exit_code;

/*#############################################*/

/*#################SOFT_PARAM##################*/
#define NSFW_SOFT_PARAM_MODULE "nsfw_soft_param"

typedef struct _nsfw_soft_param_msg
{
  u32 param_name;
  u32 rsp_code;
  u8 param_value[NSFW_MGR_MSG_BODY_LEN - sizeof (u32) - sizeof (u32)];
}
nsfw_soft_param_msg;

typedef enum _nsfw_soft_param
{
  NSFW_DBG_MODE_PARAM = 1,
  NSFW_HBT_TIMER = 2,
  NSFW_HBT_COUNT_PARAM = 3,
  NSFW_APP_EXIT_TIMER = 4,
  NSFW_SRV_RESTORE_TIMER = 5,
  NSFW_APP_RESEND_TIMER = 6,
  NSFW_APP_SEND_PER_TIME = 7,

  NSFW_MAX_SOFT_PARAM = 1024
} nsfw_soft_param;

typedef int (*nsfw_set_soft_fun) (u32 param, char *buf, u32 buf_len);
extern u8 nsfw_soft_param_reg_fun (u32 param_name, nsfw_set_soft_fun fun);
extern u8 nsfw_soft_param_reg_int (u32 param_name, u32 size, u32 min,
                                   u32 max, u64 * data);

extern void nsfw_set_soft_para (fw_poc_type proc_type, u32 para_name,
                                void *value, u32 size);

extern int nsfw_isdigitstr (const char *str);
#define NSFW_REG_SOFT_INT(_param,_data,_min, _max) nsfw_soft_param_reg_int(_param,sizeof(_data),_min,_max,(u64*)&_data)
/*#############################################*/

/*#################LOG_CONFIG##################*/
#define NSFW_LOG_CFG_MODULE "nsfw_log_cfg"

#define NSFW_MODULE_NAME_LEN 20
#define NSFW_LOG_LEVEL_LEN 10
#define NSFW_LOG_VALUE_LEN 256

typedef struct _nsfw_set_log_msg
{
  u16 rsp_code;
  char module[NSFW_MODULE_NAME_LEN];
  char log_level[NSFW_LOG_VALUE_LEN];
} nsfw_set_log_msg;
/*#############################################*/

/*################## DFX ######################*/
#define MAX_DFX_QRY_RES_LEN 28

#define SPL_DFX_RES_ALL         "all"
#define SPL_DFX_RES_QUEUE       "queue"
#define SPL_DFX_RES_CONN        "conn"
#define SPL_DFX_RES_L2TO4       "l2to4"
#define SPL_DFX_RES_UNMATCH     "version"
#define SPL_DFX_RES_SOCKET_CB    "socketcb"
#define SPL_DFX_RES_COMM_MEMPOOL "mbufpool"
#define SPL_DFX_RES_PCBLIST     "pcblist"
#define SPL_DFX_RES_ARPLIST     "arplist"

typedef enum
{
  DFX_ACTION_SNAPSHOT,
  DFX_ACTION_RST_STATS,
  DFX_ACTION_SWITCH,
  DFX_ACTION_MAX
} dfx_module_action;

typedef struct _nsfw_dfx_qry_msg
{
  dfx_module_action action;
  char resource[MAX_DFX_QRY_RES_LEN];
  char flag;                    //for snapshot print "all"
} nsfw_dfx_qry_msg;

typedef enum
{
  QUERY_ACTION_GET,
  QUERY_ACTION_MAX
} query_action;

typedef struct _nsfw_qry_msg
{
  query_action action;
  char resource[MAX_DFX_QRY_RES_LEN];
} nsfw_get_qry_msg;

/*##################DFX#########################*/

/*#################for tcpdump#####################*/

#ifndef nstack_min
#define nstack_min(a, b) (a) < (b) ? (a) : (b)
#endif

#define GET_CUR_TIME(ptime) \
    (void)clock_gettime(CLOCK_MONOTONIC, ptime);

#define TCPDUMP_MODULE "tcpdump_tool"

#define DUMP_MSG_NUM (64 * 1024)
COMPAT_PROTECT (DUMP_MSG_NUM, 64 * 1024);
#define DUMP_MSG_SIZE 128       // can not be less than 14
COMPAT_PROTECT (DUMP_MSG_SIZE, 128);

#define DEFAULT_DUMP_TIME 600
#define MAX_DUMP_TIME 86400
#define MIN_DUMP_TIME 1

#define MAX_DUMP_TASK 16
#define DUMP_HBT_INTERVAL 2
#define DUMP_HBT_CHK_INTERVAL 4
#define DUMP_TASK_HBT_TIME_OUT 30

#define DUMP_SHMEM_RIGN_NAME "tcpdump_ring"
#define DUMP_SHMEM_POOL_NAME "tcpdump_pool"

enum L2_PROTOCOL
{
  PROTOCOL_IP = 0x0800,
  PROTOCOL_ARP = 0x0806,
  PROTOCOL_RARP = 0x8035,
  PROTOCOL_OAM_LACP = 0x8809,
  INVALID_L2_PROTOCOL = 0xFFFF
};

enum L3_PROTOCOL
{
  PROTOCOL_ICMP = 1,
  PROTOCOL_TCP = 6,
  PROTOCOL_UDP = 17,
  INVALID_L3_PROTOCOL = 0xFF
};

enum DUMP_MSG_DIRECTION
{
  DUMP_SEND = 1,
  DUMP_RECV = 2,
  DUMP_SEND_RECV = 3
};

enum DUMP_MSG_TYPE
{
  START_DUMP_REQ,
  STOP_DUMP_REQ,
  TOOL_COM_HBT_REQ,

  DUMP_MSG_TYPE_RSP = 0x00010000,

  START_DUMP_RSP = START_DUMP_REQ + DUMP_MSG_TYPE_RSP,
  STOP_DUMP_RSP = STOP_DUMP_REQ + DUMP_MSG_TYPE_RSP,

  DUMP_MSG_TYPE_INVALID
};

typedef struct _nsfw_tool_hbt
{
  u32 seq;
  i16 task_id;
} nsfw_tool_hbt;

typedef struct _nsfw_tool_dump_msg
{
  u16 op_type;
  i16 task_id;
  u32 task_keep_time;
} nsfw_tool_dump_msg;

typedef struct _dump_msg_info
{
  u32 len;
  u16 direction;                // 1:SEND, 2:RECV
  u32 dump_sec;
  u32 dump_usec;
  nsfw_res res_chk;
  char buf[1];
} dump_msg_info;

typedef struct _dump_timer_info
{
  u32 seq;
  i16 task_id;
  void *interval;
  void *ptimer;
} dump_timer_info;

extern void ntcpdump_loop (void *buf, u32 buf_len, u16 direction,
                           void *eth_addr);
extern void ntcpdump (void *buf, u32 buf_len, u16 direction);

/*##############for tcpdump######################*/

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSFW_MEM_STAT_API_H  */
