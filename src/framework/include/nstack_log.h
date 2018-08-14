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

#ifndef _NSTACK_LOG_H_
#define _NSTACK_LOG_H_
/*==============================================*
 *      include header files                    *
 *----------------------------------------------*/
#pragma GCC diagnostic ignored "-Wcpp"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include "types.h"

#include "glog/nstack_glog.ph"
#include "glog/nstack_glog_in.h"

#define NSTACK_GETVER_MODULE    "nStack"

#ifndef NSTACK_GETVER_VERSION
#error "need define version first"
#endif

#define NSTACK_GETVER_BUILDTIME "[" __DATE__ "]" "[" __TIME__ "]"
#define NSTACK_VERSION          NSTACK_GETVER_VERSION " (" NSTACK_GETVER_MODULE ") " NSTACK_GETVER_BUILDTIME

#define LOG_TIME_STAMP_LEN 17   // "YYYYMMDDHHMMSS";

/*==============================================*
 *      constants or macros define              *
 *----------------------------------------------*/
typedef enum _LOG_MODULE
{
  NSOCKET = 1,
  STACKX,
  OPERATION,
  MASTER,
  LOGTCP,
  LOGUDP,
  LOGIP,
  LOGCMP,
  LOGARP,
  LOGRTE,
  LOGHAL,
  LOGDFX,
  LOGFW,
  LOGSBR,
  MAX_LOG_MODULE
} LOG_MODULE;

enum _LOG_TYPE
{
  LOG_TYPE_NSTACK = 0,
  LOG_TYPE_OPERATION,
  LOG_TYPE_MASTER,
  LOG_TYPE_APP,
  MAX_LOG_TYPE
};

enum _LOG_PROCESS
{
  LOG_PRO_NSTACK = 0,
  LOG_PRO_MASTER,
  LOG_PRO_APP,
  LOG_PRO_OMC_CTRL,
  LOG_PRO_INVALID
};

#define LOG_INVALID_VALUE 0xFFFF

#define NSLOG_DBG     0x10
#define NSLOG_INF     0x08
#define NSLOG_WAR     0x04
#define NSLOG_ERR     0x02
#define NSLOG_EMG     0x01
#define NSLOG_OFF     0x00

#define LOG_LEVEL_EMG "emg"
#define LOG_LEVEL_ERR "err"
#define LOG_LEVEL_WAR "war"
#define LOG_LEVEL_DBG "dbg"
#define LOG_LEVEL_INF "inf"

#define GET_FILE_NAME(name_have_path) strrchr(name_have_path,'/')?strrchr(name_have_path,'/')+1:name_have_path

#define NSTACK_LOG_NAME "/var/log/nStack/"

#define STACKX_LOG_NAME "running.log"

#define OPERATION_LOG_NAME "operation.log"

#define MASTER_LOG_NAME "master.log"

#define OMC_CTRL_LOG_NAME "omc_ctrl.log"

#define FAILURE_LOG_NAME "fail_dump.log"

#define FLUSH_TIME 10

#define APP_LOG_SIZE 30
#define APP_LOG_COUNT 10
#define APP_LOG_PATH "/var/log"
#define APP_LOG_NAME "nStack_nSocket.log"

#define NS_LOG_STACKX_ON     0x80U
#define NS_LOG_STACKX_TRACE  0x40U
#define NS_LOG_STACKX_STATE  0x20U
#define NS_LOG_STACKX_FRESH  0x10U
#define NS_LOG_STACKX_HALT   0x08U
#define NS_LOG_STACKX_OFF    0x00U

#define NULL_STRING ""
#define MODULE_INIT_FORMAT_STRING "module %s]name=[%s]%s"
#define MODULE_INIT_START  "init"
#define MODULE_INIT_FAIL  "start failed"
#define MODULE_INIT_SUCCESS "start success"

#define PRE_INIT_LOG_LENGTH 128

struct nstack_logs
{
  uint32_t level;    /**< Log level. */
  int pad64;
  int inited;
  int file_type;
};

struct log_init_para
{
  uint32_t run_log_size;
  uint32_t run_log_count;
  char *run_log_path;
  uint32_t mon_log_size;        //master and ctrl both use the parameter to reduce the redundancy
  uint32_t mon_log_count;       //master and ctrl both use the parameter to reduce the redundancy
  char *mon_log_path;           //master and ctrl both use the parameter to reduce the redundancy
};

enum LOG_CTRL_ID
{
  // for socket api
  LOG_CTRL_SEND = 0,
  LOG_CTRL_RECV,
  LOG_CTRL_SENDMSG,
  LOG_CTRL_RECVMSG,
  LOG_CTRL_READ,
  LOG_CTRL_WRITE,
  LOG_CTRL_READV,
  LOG_CTRL_WRITEV,
  LOG_CTRL_GETSOCKNAME,
  LOG_CTRL_GETPEERNAME,
  LOG_CTRL_GETSOCKOPT,

  // for nstack service
  LOG_CTRL_RECV_QUEUE_FULL,
  LOG_CTRL_L4_RECV_QUEUE_FULL,
  LOG_CTRL_HUGEPAGE_ALLOC_FAIL,
  LOG_CTRL_TCP_MEM_NOT_ENOUGH,
  LOG_CTRL_IPREASS_OVERFLOW,
  LOG_CTRL_ID_MAX
};

struct log_ctrl_info
{
  u32 expire_time;
  u32 unprint_count;
  struct timespec last_log_time;
};

struct pre_init_info
{
  uint32_t level;    /**< Log level. */
  char log_buffer[PRE_INIT_LOG_LENGTH];
};

#define NS_LOG_STACKX(dbug,_module,_prestr,_level,fmt, ...) \
{\
    if ((dbug) & NS_LOG_STACKX_ON)\
    {\
        NS_LOGPID(_module,_prestr,_level,fmt,##__VA_ARGS__);\
    }\
}\


/*****************************************************************************
*    Prototype     : nstack_get_log_level
*    Description  : get log level
*    Input         : int module
*    Output         : None
*    Return Value : int
*    Calls         :
*    Called By     :
*****************************************************************************/
extern struct nstack_logs g_nstack_logs[MAX_LOG_MODULE];
static inline int
nstack_get_log_level (int module)
{
  /* validity check for path */
  if ((MAX_LOG_MODULE <= module) || (module < 0))
    {
      return -1;
    }

  return g_nstack_logs[module].level;
}

/*****************************************************************************
*   Prototype    : level_stoa
*   Description  : convert stack log level to app log level
*   Input        : unsigned int nstack_level
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline unsigned int
level_stoa (unsigned int level)
{
  unsigned int golg_level;
  switch (level)
    {
    case NSLOG_DBG:
      golg_level = GLOG_LEVEL_DEBUG;
      break;
    case NSLOG_INF:
      golg_level = GLOG_LEVEL_INFO;
      break;
    case NSLOG_WAR:
      golg_level = GLOG_LEVEL_WARNING;
      break;
    case NSLOG_ERR:
      golg_level = GLOG_LEVEL_ERROR;
      break;
    case NSLOG_EMG:
      golg_level = GLOG_LEVEL_FATAL;
      break;
    default:
      golg_level = GLOG_LEVEL_BUTT;
      break;
    }
  return golg_level;
}

/* use the glog function to replace old nstack log module */

/* segregate the dump info */
#define LOG_TYPE(_module, _level) \
       (((STACKX == _module) && (NSLOG_EMG == _level)) ? GLOG_LEVEL_ERROR : ((OPERATION == _module) ? GLOG_LEVEL_WARNING : GLOG_LEVEL_INFO))

#define log_shooting(_module,_level) \
        ((NULL == g_log_hook_tag.log_hook) ? (nstack_get_log_level(_module) >= _level) : (level_stoa(_level) >= g_log_hook_tag.level))

/* add the non reentry protection */
extern __thread unsigned int nstack_log_nonreentry;

/* hanging up version check log need restrain */
extern int ctrl_log_switch;

#define NS_LOGPID(_module,_prestr,_level,fmt, ...) \
{\
    if (log_shooting(_module, _level) && (0 == nstack_log_nonreentry) && (0 == ctrl_log_switch))\
    {\
        nstack_log_nonreentry = 1;\
        if(nstack_log_info_check(_module, _level))\
           glog_print(LOG_TYPE(_module,_level),_prestr,level_stoa(_level),-1,GET_FILE_NAME(__FILE__),\
        __LINE__,__func__,fmt, ##__VA_ARGS__);\
        nstack_log_nonreentry = 0;\
    }\
}

#define NS_LOG_CTRL(_id, _module, _prestr, _level, fmt, ...) \
{\
    if (log_shooting(_module, _level) && (0 == nstack_log_nonreentry) && check_log_prt_time(_id))\
    {\
        nstack_log_nonreentry = 1;\
        if(nstack_log_info_check(_module, _level))\
           glog_print(LOG_TYPE(_module,_level),_prestr,level_stoa(_level),get_unprt_log_count(_id),\
        GET_FILE_NAME(__FILE__),__LINE__,__func__,fmt, ##__VA_ARGS__);\
        clr_unprt_log_count(_id);\
        nstack_log_nonreentry = 0;\
    }\
}

#define NS_LOG_CTRL_STACKX(_id, dbug,_module,_prestr,_level,fmt, ...) \
{\
    if ((dbug) & NS_LOG_STACKX_ON)\
    {\
        NS_LOG_CTRL(_id, _module,_prestr,_level,fmt,##__VA_ARGS__);\
    }\
}\


/*for every log modules should def marcos below use a sort module name, just like MON means Monitor*/
#define NSMON_LOGINF(fmt, ...) NS_LOGPID(MASTER,"NSMON",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSMON_LOGDBG(fmt, ...) NS_LOGPID(MASTER,"NSMON",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSMON_LOGWAR(fmt, ...) NS_LOGPID(MASTER,"NSMON",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSMON_LOGERR(fmt, ...) NS_LOGPID(MASTER,"NSMON",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSPOL_LOGINF(debug,fmt, ...)NS_LOG_STACKX(debug,STACKX,"NSPOL",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSPOL_LOGDBG(debug,fmt, ...) NS_LOG_STACKX(debug,STACKX,"NSPOL",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSPOL_LOGWAR(debug,fmt, ...) NS_LOG_STACKX(debug,STACKX,"NSPOL",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSPOL_LOGERR(fmt, ...) NS_LOGPID(STACKX,"NSPOL",NSLOG_ERR,fmt,##__VA_ARGS__)
#define NSPOL_LOGEMG(fmt, ...) NS_LOGPID(STACKX,"NSPOL",NSLOG_EMG,fmt,##__VA_ARGS__)

#define NSOPR_LOGINF(fmt, ...) NS_LOGPID(OPERATION,"NSOPR",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSOPR_LOGDBG(fmt, ...) NS_LOGPID(OPERATION,"NSOPR",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSOPR_LOGWAR(fmt, ...) NS_LOGPID(OPERATION,"NSOPR",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSOPR_LOGERR(fmt, ...) NS_LOGPID(OPERATION,"orchestration",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSSOC_LOGINF(fmt, ...) NS_LOGPID(NSOCKET,"NSSOC",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSSOC_LOGDBG(fmt, ...) NS_LOGPID(NSOCKET,"NSSOC",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSSOC_LOGWAR(fmt, ...) NS_LOGPID(NSOCKET,"NSSOC",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSSOC_LOGERR(fmt, ...) NS_LOGPID(NSOCKET,"NSSOC",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSSBR_LOGINF(fmt, ...) NS_LOGPID(LOGSBR,"NSSBR",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSSBR_LOGDBG(fmt, ...) NS_LOGPID(LOGSBR,"NSSBR",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSSBR_LOGWAR(fmt, ...) NS_LOGPID(LOGSBR,"NSSBR",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSSBR_LOGERR(fmt, ...) NS_LOGPID(LOGSBR,"NSSBR",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSCOMM_LOGINF(fmt, ...) NS_LOGPID(LOGRTE, "NSRTE",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSCOMM_LOGDBG(fmt, ...) NS_LOGPID(LOGRTE, "NSRTE",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSCOMM_LOGWAR(fmt, ...) NS_LOGPID(LOGRTE, "NSRTE",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSCOMM_LOGERR(fmt, ...) NS_LOGPID(LOGRTE, "NSRTE",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSTCP_LOGINF(fmt, ...) NS_LOGPID(LOGTCP,"NSTCP",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSTCP_LOGDBG(fmt, ...) NS_LOGPID(LOGTCP,"NSTCP",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSTCP_LOGWAR(fmt, ...) NS_LOGPID(LOGTCP,"NSTCP",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSTCP_LOGERR(fmt, ...) NS_LOGPID(LOGTCP,"NSTCP",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSIP_LOGINF(fmt, ...) NS_LOGPID(LOGIP,"NSIP",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSIP_LOGDBG(fmt, ...) NS_LOGPID(LOGIP,"NSIP",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSIP_LOGWAR(fmt, ...) NS_LOGPID(LOGIP,"NSIP",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSIP_LOGERR(fmt, ...) NS_LOGPID(LOGIP,"NSIP",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSUDP_LOGINF(fmt, ...) NS_LOGPID(LOGUDP,"NSUDP",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSUDP_LOGDBG(fmt, ...) NS_LOGPID(LOGUDP,"NSUDP",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSUDP_LOGWAR(fmt, ...) NS_LOGPID(LOGUDP,"NSUDP",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSUDP_LOGERR(fmt, ...) NS_LOGPID(LOGUDP,"NSUDP",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSHAL_LOGINF(fmt, ...) NS_LOGPID(LOGHAL,"NSHAL",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSHAL_LOGDBG(fmt, ...) NS_LOGPID(LOGHAL,"NSHAL",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSHAL_LOGWAR(fmt, ...) NS_LOGPID(LOGHAL,"NSHAL",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSHAL_LOGERR(fmt, ...) NS_LOGPID(LOGHAL,"NSHAL",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSARP_LOGINF(fmt, ...) NS_LOGPID(LOGARP,"NSARP",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSARP_LOGDBG(fmt, ...) NS_LOGPID(LOGARP,"NSARP",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSARP_LOGWAR(fmt, ...) NS_LOGPID(LOGARP,"NSARP",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSARP_LOGERR(fmt, ...) NS_LOGPID(LOGARP,"NSARP",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSDFX_LOGINF(fmt, ...) NS_LOGPID(LOGDFX,"NSDFX",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSDFX_LOGDBG(fmt, ...) NS_LOGPID(LOGDFX,"NSDFX",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSDFX_LOGWAR(fmt, ...) NS_LOGPID(LOGDFX,"NSDFX",NSLOG_WAR,fmt,##__VA_ARGS__)
#define NSDFX_LOGERR(fmt, ...) NS_LOGPID(LOGDFX,"NSDFX",NSLOG_ERR,fmt,##__VA_ARGS__)

#define NSFW_LOGINF(fmt, ...) NS_LOGPID(LOGFW,"NSFW",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSFW_LOGDBG(fmt, ...) NS_LOGPID(LOGFW,"NSFW",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSFW_LOGERR(fmt, ...) NS_LOGPID(LOGFW,"NSFW",NSLOG_ERR,fmt,##__VA_ARGS__)
#define NSFW_LOGWAR(fmt, ...) NS_LOGPID(LOGFW,"NSFW",NSLOG_WAR,fmt,##__VA_ARGS__)

#define NSAM_LOGINF(fmt, ...) NS_LOGPID(LOGFW,"NSAM",NSLOG_INF,fmt,##__VA_ARGS__)
#define NSAM_LOGDBG(fmt, ...) NS_LOGPID(LOGFW,"NSAM",NSLOG_DBG,fmt,##__VA_ARGS__)
#define NSAM_LOGERR(fmt, ...) NS_LOGPID(LOGFW,"NSAM",NSLOG_ERR,fmt,##__VA_ARGS__)
#define NSAM_LOGWAR(fmt, ...) NS_LOGPID(LOGFW,"NSAM",NSLOG_WAR,fmt,##__VA_ARGS__)

#define INIT_LOG_ASSEM(log_module,_prestr,_level, init_module , function, errString, errValue, status) \
    if ((LOG_INVALID_VALUE <= errValue) && (1 == sizeof(errString))) \
    { \
        NS_LOGPID(log_module,_prestr, _level,MODULE_INIT_FORMAT_STRING, (char*)status, init_module, function ); \
    } \
    else if (LOG_INVALID_VALUE <= errValue)\
    { \
        NS_LOGPID(log_module,_prestr, _level,MODULE_INIT_FORMAT_STRING",err_string=%s", (char*)status, init_module, function, errString ); \
    } \
    else if (1 == sizeof(errString))\
    { \
        NS_LOGPID(log_module,_prestr, _level,MODULE_INIT_FORMAT_STRING",err_value=%d", (char*)status, init_module, function, errValue ); \
    } \
    else \
    { \
        NS_LOGPID(log_module,_prestr, _level,MODULE_INIT_FORMAT_STRING",err_string=%s,err_value=%d", (char*)status, init_module, function, errString, errValue ); \
    } \

#define INITPOL_LOGINF(init_module_name, function, err_string, err_value, status) \
    INIT_LOG_ASSEM(STACKX,"NSPOL",NSLOG_INF,init_module_name , function, err_string, err_value, status)\

#define INITPOL_LOGERR(init_module_name, function, err_string, err_value, status) \
    INIT_LOG_ASSEM(STACKX,"NSPOL",NSLOG_ERR,init_module_name , function, err_string, err_value, status)\

#define INITTCP_LOGINF(init_module_name , function, err_string, err_value, status) \
    INIT_LOG_ASSEM(LOGTCP,"NSTCP",NSLOG_INF,init_module_name , function, err_string, err_value, status)\

#define INITTCP_LOGERR(init_module_name , function, err_string, err_value, status) \
    INIT_LOG_ASSEM(LOGTCP,"NSTCP",NSLOG_ERR,init_module_name , function, err_string, err_value, status)\

#define INITMON_LOGERR(init_module_name , function, err_string, err_value, status) \
    INIT_LOG_ASSEM(MASTER,"NSMON",NSLOG_ERR,init_module_name , function, err_string, err_value, status)\

#define INITSOC_LOGERR(init_module_name , function, err_string, err_value, status) \
    INIT_LOG_ASSEM(NSOCKET,"NSSOC",NSLOG_ERR,init_module_name , function, err_string, err_value, status)

#define NSPOL_DUMP_LOGINF(fmt, ...) NSPOL_LOGINF(0x80, fmt, ##__VA_ARGS__)
#define NSPOL_DUMP_LOGDBG(fmt, ...) NSPOL_LOGDBG(0x80, fmt, ##__VA_ARGS__)
#define NSPOL_DUMP_LOGERR(fmt, ...) NSPOL_LOGERR(fmt, ##__VA_ARGS__)
#define NSPOL_DUMP_LOGWAR(fmt, ...) NSPOL_LOGWAR(0x80, fmt, ##__VA_ARGS__)

/*==============================================*
 *      routines' or functions' implementations *
 *----------------------------------------------*/

void save_pre_init_log (uint32_t level, char *fmt, ...);
void write_pre_init_log ();

void set_log_init_para (struct log_init_para *para);

void nstack_setlog_level (int module, uint32_t level);
bool nstack_log_info_check (uint32_t module, uint32_t level);
int nstack_log_init ();
void nstack_log_init_app ();
int get_app_env_log_path (char *app_file_path, unsigned int app_file_size);
void set_log_proc_type (int log_proc_type);

int setlog_level_value (const char *param, const char *value);
int get_str_value (const char *arg);
int check_log_dir_valid (const char *path);
void nstack_segment_error (int s);
void init_log_ctrl_info ();
void set_log_ctrl_time (int id, int ctrl_time);

int cmp_log_path (const char *path);

#ifndef sys_sleep_ns
#define sys_sleep_ns(_s, _ns)\
        {\
            if ((_s) >= 0 && (_ns) >= 0){\
                struct timespec delay, remain;\
                delay.tv_sec=(_s);\
                delay.tv_nsec=(_ns);\
                while (nanosleep (&delay, &remain) < 0)\
                {\
                    delay = remain;\
                }\
            }\
        }
#endif /* sys_sleep_ns */

int check_log_prt_time (int id);
int get_unprt_log_count (int id);
void clr_unprt_log_count (int id);

void get_current_time (char *buf, const int len);

#endif
