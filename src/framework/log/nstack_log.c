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

#include "nstack_log.h"
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include "nstack_securec.h"
#include <pthread.h>

/*==============================================*
 *      constants or macros define              *
 *----------------------------------------------*/

#define FILE_NAME_LEN 256
#define MAX_PRE_INIT_LOG_COUNT 256
#define MAX_LOCK_RETRY_COUNT 50000      /* changed MAX_LOCK_RETRY_COUNT to 50000 to avoid function being blocked for too long */
/* *INDENT-OFF* */
int g_l4_dump_enable = 0;
static int pre_init_log_count = 0;
static struct pre_init_info pre_init_log[MAX_PRE_INIT_LOG_COUNT] = {{0, ""}};
/* *INDENT-ON* */
__thread unsigned int nstack_log_nonreentry = 0;
int ctrl_log_switch = 0;

/*==============================================*
 *      project-wide global variables           *
 *----------------------------------------------*/
struct nstack_logs g_nstack_logs[MAX_LOG_MODULE] = { {0, 0, 0, 0}, {0xFFFF, 0, 0, 0} }; /* Clear compile warning */
struct log_init_para g_log_init_para =
  { 50, 10, NSTACK_LOG_NAME, 10, 10, NSTACK_LOG_NAME };
static int g_my_pro_type = LOG_PRO_INVALID;

#define DEFAULT_LOG_CTR_TIME 5
static struct log_ctrl_info g_log_ctrl_info[LOG_CTRL_ID_MAX];

/*==============================================*
 *      routines' or functions' implementations *
 *----------------------------------------------*/

/* change the print level, not only has err, Begin.*/
void
save_pre_init_log (uint32_t level, char *fmt, ...)
{
  va_list ap;
  int ret = 0;
  /* add pre_init_log_count rang check */
  if (level > NSLOG_DBG || pre_init_log_count >= MAX_PRE_INIT_LOG_COUNT
      || pre_init_log_count < 0)

    {
      return;
    }
  pre_init_log[pre_init_log_count].log_buffer[PRE_INIT_LOG_LENGTH - 1] = '\0';
  (void) va_start (ap, fmt);
  ret =
    VSNPRINTF_S (pre_init_log[pre_init_log_count].log_buffer,
                 PRE_INIT_LOG_LENGTH, PRE_INIT_LOG_LENGTH - 1, fmt, ap);
  if (-1 == ret)
    {
      va_end (ap);
      return;
    }
  va_end (ap);
  pre_init_log[pre_init_log_count].level = level;
  pre_init_log_count++;
}

void
write_pre_init_log ()
{
  int i = 0;
  for (; i < pre_init_log_count; i++)
    {
      if (NSLOG_ERR == pre_init_log[i].level)
        {
          NSPOL_LOGERR ("pre init log: %s", pre_init_log[i].log_buffer);
        }
      else if (NSLOG_WAR == pre_init_log[i].level)
        {
          NSPOL_LOGWAR (NS_LOG_STACKX_ON, "pre init log: %s",
                        pre_init_log[i].log_buffer);
        }
      else if (NSLOG_INF == pre_init_log[i].level)
        {
          NSPOL_LOGINF (NS_LOG_STACKX_ON, "pre init log: %s",
                        pre_init_log[i].log_buffer);
        }
    }
}

int
cmp_log_path (const char *path)
{
  if (NULL == path)
    {
      return 1;
    }

  /* remove struct log_info g_nstack_log_info */
  if (NULL != g_log_init_para.mon_log_path)
    {
      if (strcmp (g_log_init_para.mon_log_path, path) == 0)
        {
          return 0;
        }
    }
  return 1;
}

void
get_current_time (char *buf, const int len)
{
  int retVal;
  time_t cur_tick;
  struct tm cur_time;
  (void) time (&cur_tick);
  /* add return value check, */
  if (NULL == localtime_r (&cur_tick, &cur_time))
    {
      return;
    }

  // from man page of localtime_r:
  // tm_year   The number of years since 1900.
  // tm_mon    The number of months since January, in the range 0 to 11.
  retVal =
    SNPRINTF_S (buf, len, len - 1, "%04d%02d%02d%02d%02d%02d",
                cur_time.tm_year + 1900, cur_time.tm_mon + 1,
                cur_time.tm_mday, cur_time.tm_hour, cur_time.tm_min,
                cur_time.tm_sec);
  if (-1 == retVal)
    {
      return;
    }
  buf[len - 1] = 0;
}

/*****************************************************************************
*   Prototype    : nstack_setlog_level
*   Description  : Set global log level
*   Input        : int module
*                  uint32_t level
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nstack_setlog_level (int module, uint32_t level)
{
  if (MAX_LOG_MODULE <= module || module < 0)
    {
      return;
    }
  g_nstack_logs[module].level = level;
}

/*****************************************************************************
*   Prototype    : nstack_log_info_check
*   Description  : log info check
*   Input        : uint32_t module
*                  uint32_t level
*                  ...
*   Output       : None
*   Return Value : bool
*   Calls        :
*   Called By    :
*****************************************************************************/
bool
nstack_log_info_check (uint32_t module, uint32_t level)
{
  if (MAX_LOG_MODULE <= module)
    {
      return false;
    }

  /* no need compare module ,which is done ahead */
  if (LOG_PRO_INVALID == g_my_pro_type)
    {
      return false;
    }
  return true;
}

/* *INDENT-OFF* */
NSTACK_STATIC inline void init_operation_log_para()
{
    g_nstack_logs[OPERATION].file_type = LOG_TYPE_OPERATION;
}

NSTACK_STATIC inline void init_nstack_log_para()
{
    int i = 0;

    (void)glogLevelSet(GLOG_LEVEL_DEBUG);
    glogBufLevelSet(GLOG_LEVEL_WARNING);
    for(; i<GLOG_LEVEL_BUTT; i++)
        glogSetLogSymlink(i,"");
    glogDir(g_log_init_para.run_log_path);
    nstack_log_count_set(g_log_init_para.run_log_count);
    glogMaxLogSizeSet(g_log_init_para.run_log_size);
    glogSetLogFilenameExtension(STACKX_LOG_NAME);
    glogFlushLogSecsSet(FLUSH_TIME);

    for (i = 0; i < MAX_LOG_MODULE ; i++ )
    {
        if (i == OPERATION)
        {
            continue;
        }
        g_nstack_logs[i].file_type = LOG_TYPE_NSTACK;
    }
    init_operation_log_para();
}

NSTACK_STATIC inline void init_ctrl_log_para()
{
    int i = 0;

    (void)glogLevelSet(GLOG_LEVEL_DEBUG);
    glogBufLevelSet(GLOG_LEVEL_WARNING);
    for(; i<GLOG_LEVEL_BUTT; i++)
        glogSetLogSymlink(i,"");
    glogDir(g_log_init_para.mon_log_path);
    nstack_log_count_set(g_log_init_para.mon_log_count);
    glogMaxLogSizeSet(g_log_init_para.mon_log_size);
    glogSetLogFilenameExtension(OMC_CTRL_LOG_NAME);
    glogFlushLogSecsSet(FLUSH_TIME);
}

NSTACK_STATIC inline void init_master_log_para()
{
    int i = 0;

    (void)glogLevelSet(GLOG_LEVEL_DEBUG);
    glogBufLevelSet(GLOG_LEVEL_WARNING);
    for(; i<GLOG_LEVEL_BUTT; i++)
        glogSetLogSymlink(i,"");
    glogDir(g_log_init_para.mon_log_path);
    nstack_log_count_set(g_log_init_para.mon_log_count);
    glogMaxLogSizeSet(g_log_init_para.mon_log_size);
    glogSetLogFilenameExtension(MASTER_LOG_NAME);
    glogFlushLogSecsSet(FLUSH_TIME);
    for (i = 0; i < MAX_LOG_MODULE ; i++ )
    {
        g_nstack_logs[i].file_type = LOG_TYPE_MASTER;
    }
}
/* *INDENT-OFF* */

/*****************************************************************************
*   Prototype    : nstack_log_init
*   Description  : called by environment-specific log init function
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int nstack_log_init()
{
  char *pst_temp = NULL;
  int log_level = NSLOG_INF;
       pst_temp = getenv("NSTACK_LOG_ON");
   if (pst_temp)
    {
      if (strcmp (pst_temp, "INF") == 0)
    {
      log_level = NSLOG_INF;
    }
      else if (strcmp (pst_temp, "DBG") == 0)
    {
      log_level = NSLOG_DBG;
    }
      else if (strcmp (pst_temp, "WAR") == 0)
    {
      log_level = NSLOG_WAR;
    }
      else if (strcmp (pst_temp, "ERR") == 0)
    {
      log_level = NSLOG_ERR;
    }
      else if (strcmp (pst_temp, "EMG") == 0)
    {
      log_level = NSLOG_EMG;
    }
      else
    {
      log_level = NSLOG_ERR;
    }
     }
  else
    {
      log_level = NSLOG_INF;
    }
   int i = 0;
  for (i = 0; i < MAX_LOG_MODULE; i++)
    {
      nstack_setlog_level (i, log_level);
    }
   if (log_level <= NSLOG_WAR)
    {
    /*MONITOR log level must set to larger than warning */
    nstack_setlog_level (MASTER, NSLOG_WAR);
    }

    /* monitor and nstack write the same file, it will cause synchronize problem */
    switch (g_my_pro_type)
    {
    case LOG_PRO_NSTACK:
      glogInit ("NSTACK");
      init_nstack_log_para ();
      break;
    case LOG_PRO_OMC_CTRL:
      glogInit ("CTRL");
      init_ctrl_log_para ();
      break;
    case LOG_PRO_MASTER:
      glogInit ("MASTER");
      init_master_log_para ();
      break;
    default:
      return 0;
    }

    init_log_ctrl_info();

    // this is for monitr to check whether log has beed inited
    g_nstack_logs[NSOCKET].inited = 1;
   NSPOL_LOGERR ("nStackMain_version=%s", NSTACK_VERSION);
    write_pre_init_log();
   return 0;
}

/*****************************************************************************
*   Prototype    : get_str_value
*   Description  : get int value
*   Input        : const char *arg
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
get_str_value (const char *arg)
{
  char *parsing_end;
  int iValue = 0;
  int oldErrno = errno;
   if (arg == NULL)
    {
      return -1;
    }
  errno = 0;
    iValue = (int) strtol (arg, &parsing_end, 0);
  if (errno || (!parsing_end) || parsing_end[0] != 0)
    {
      iValue = -1;
    }
    errno = oldErrno;
  return iValue;
}


/*****************************************************************************
*   Prototype    : setlog_level_value
*   Description  : proc log level config
*   Input        : const char *param
*                  const char *value
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
setlog_level_value (const char *param, const char *value)
{
  int i = 0;
  int module = 0;
  int logLevel = 0;
  module = get_str_value (param);
  if ((module < 0) || (MAX_LOG_MODULE <= module))
    {
      NSOPR_LOGERR ("input module error]param=%s,module=%d", param, module);
      return 1;
    }

    if (strcmp (value, LOG_LEVEL_ERR) == 0)
    {
      logLevel = NSLOG_ERR;
    }
  else if (strcmp (value, LOG_LEVEL_WAR) == 0)
    {
      logLevel = NSLOG_WAR;
    }
  else if (strcmp (value, LOG_LEVEL_DBG) == 0)
    {
      logLevel = NSLOG_DBG;
    }
  else if (strcmp (value, LOG_LEVEL_INF) == 0)
    {
      logLevel = NSLOG_INF;
    }
  else if (strcmp (value, LOG_LEVEL_EMG) == 0)
    {
      logLevel = NSLOG_EMG;
    }
  else
    {
      NSOPR_LOGERR ("input log level error!");
      return 1;
    }
   NSOPR_LOGINF ("set module log with level]module=%d,logLevel=0x%x",
           module, logLevel);
   if (module > 0)
    {
      nstack_setlog_level (module, logLevel);
      return 0;
    }
   if (0 == module)
    {
        for ( i = 0; i < MAX_LOG_MODULE ; i++ )
        {
             nstack_setlog_level(i,logLevel);
        }
    }
   return 0;
}


/*****************************************************************************
*   Prototype    : check_log_dir_valid
*   Description  : check the log dir valid or not
*   Input        : const char *arg
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
check_log_dir_valid (const char *path)
{
  size_t length;
  struct stat statbuf;
  if (NULL == path)
    {
      return -1;
    }
   length = strlen (path) + 1;
  if ((length <= 1) || (length > FILE_NAME_LEN))
    {
      return -1;
    }

    /* only write permission is allowed */
    if ((0 != access (path, W_OK)))
    {
    /* if path can access, use env path */
    return -1;
    }

    if ((0 == lstat (path, &statbuf)) && S_ISDIR (statbuf.st_mode))
    {
      return 0;
     }
  else
    {
      return -1;
    }
}


/*****************************************************************************
*   Prototype    : get_app_env_log_path
*   Description  : called by environment-specific log init function
*   Input        : app_file_path, a char pointer to store the log path
*   Input        : app_file_size, the app_file_path size
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
get_app_env_log_path (char *app_file_path, unsigned int app_file_size)
{
  char *pst_app_log_path_flag = NULL;
  char *pst_app_log_path_string = NULL;
  int log_to_file = 0;
  int ret = -1;
  char *app_dir = NULL;

   if ((NULL == app_file_path) || (0 == app_file_size))
    {
      return 0;
    }
   pst_app_log_path_flag = getenv ("NSTACK_LOG_FILE_FLAG");
   if (pst_app_log_path_flag && strcmp (pst_app_log_path_flag, "1") == 0)
    {
        /* if set environment variable to 1,then output to file*/
        log_to_file = 1;
    }
  else
    {
        /*  if environment variable is not equal 1 or
         don't set this environment variable ,output to STDOUT */
        return 0;
    }

    /* add the realpath and dir check */
    /* APP LOG can be set by user */
    pst_app_log_path_string = getenv("NSTACK_APP_LOG_PATH");

    if ((NULL == pst_app_log_path_string)
    ||(strlen (pst_app_log_path_string) > FILE_NAME_LEN - 1))
    {
      goto app_default;
    }

    app_dir = realpath (pst_app_log_path_string, NULL);
    if (check_log_dir_valid (pst_app_log_path_string) < 0)
    {
      goto app_default;
    }
    ret = STRCPY_S (app_file_path, app_file_size, app_dir);
    if(EOK != ret)
    {
        log_to_file = 0;
    }

    free(app_dir);
    return log_to_file;

app_default:

  if ((0 == access (APP_LOG_PATH, W_OK)))
    {
      ret = STRCPY_S (app_file_path, app_file_size, APP_LOG_PATH);
      if (EOK != ret)
        {
          log_to_file = 0;
        }
    }
  else
    {
      log_to_file = 0;
    }

  if (NULL != app_dir)
    {
      free (app_dir);
    }

  return log_to_file;

}

/*****************************************************************************
*   Prototype    : nstack_get_app_logname
*   Description  : get the name of app's log
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nstack_get_app_logname (char* log_name)
{
  int pid = getpid ();
  char processname[FILE_NAME_LEN] = {0};

  if (log_name == NULL)
    return 1;

  strncpy (processname, program_invocation_short_name, 10);

  snprintf (log_name, FILE_NAME_LEN, "app_%s_%d.log", processname, pid);

  return 0;
}

/*****************************************************************************
*   Prototype    : nstack_log_init_app
*   Description  : called by environment-specific log init function
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nstack_log_init_app ()
{
  char *pc_temp = NULL;
  int log_level = NSLOG_ERR;
  int i = 0;
  int file_flag = 0;
  char app_log_path[FILE_NAME_LEN] = { 0 };
  int ret = 0;
  char app_log_name[FILE_NAME_LEN] = { 0 };

  /* log already initialized, just return */
  if (LOG_PRO_INVALID != g_my_pro_type)
    {
      return;
    }
  /* Add app log hook module init */
  nstack_log_hook_init ();

  if (0 != g_nstack_logs[NSOCKET].inited)
    {
      return;
    }
  glogInit ("APP");

  pc_temp = getenv ("NSTACK_LOG_ON");
  if (pc_temp)
    {
      if (strcmp (pc_temp, "INF") == 0)
        {
          log_level = NSLOG_INF;
        }
      else if (strcmp (pc_temp, "DBG") == 0)
        {
          log_level = NSLOG_DBG;
        }
      else if (strcmp (pc_temp, "WAR") == 0)
        {
          log_level = NSLOG_WAR;
        }
      else if (strcmp (pc_temp, "ERR") == 0)
        {
          log_level = NSLOG_ERR;
        }
      else if (strcmp (pc_temp, "EMG") == 0)
        {
          log_level = NSLOG_EMG;
        }
      else
        {
          log_level = NSLOG_ERR;
        }

    }
  else
    {
      log_level = NSLOG_ERR;
    }

  /* socket interface APP called include both stack-x and nstack module! */
  nstack_setlog_level (STACKX, log_level);
  nstack_setlog_level (NSOCKET, log_level);
  nstack_setlog_level (LOGRTE, log_level);
  nstack_setlog_level (LOGDFX, log_level);
  nstack_setlog_level (LOGFW, log_level);
  nstack_setlog_level (LOGHAL, log_level);
  nstack_setlog_level (LOGSBR, log_level);

  /* package the app env handle function, Begin */
  file_flag = get_app_env_log_path (app_log_path, FILE_NAME_LEN);
  if ((1 == file_flag) && (strlen (app_log_path) > 0))
    {
      glogDir (app_log_path);
      glogBufLevelSet (GLOG_LEVEL_WARNING);
      (void) glogLevelSet (GLOG_LEVEL_DEBUG);
      for (i = 0; i < GLOG_LEVEL_BUTT; i++)
        glogSetLogSymlink (i, "");
      nstack_log_count_set (APP_LOG_COUNT);
      glogMaxLogSizeSet (APP_LOG_SIZE);
      ret = nstack_get_app_logname (app_log_name);
      if (ret == 0)
        {
          glogSetLogFilenameExtension (app_log_name);
        }
      else
        {
          glogSetLogFilenameExtension (APP_LOG_NAME);
        }
      glogFlushLogSecsSet (FLUSH_TIME);
    }
  else
    {
      glogToStderrSet (1);
    }

  for (i = 0; i < MAX_LOG_MODULE; i++)
    {
      g_nstack_logs[i].file_type = LOG_TYPE_APP;
    }
  init_log_ctrl_info ();
  g_my_pro_type = LOG_PRO_APP;
  SetGlogCtrlOpt (TRUE);
  NSPOL_LOGERR ("app_nStack_version=%s", NSTACK_VERSION);
  return;
}

void
nstack_segment_error (int s)
{

#define BACKTRACE_SIZ 20
  void *array[BACKTRACE_SIZ];
  int size;
  int i;
  char **strings = NULL;

  /*if set, flush the log immediately */
  glogFlushLogFiles (GLOG_LEVEL_DEBUG);

  size = backtrace (array, BACKTRACE_SIZ);
  NSPOL_LOGEMG
    ("------------------DUMP_BACKTRACE[%d]--------------------------------\n",
     size);

  /* easy to view signal in  separate log file */
  NSPOL_LOGEMG ("Received signal s=%d", s);

  for (i = 0; i < size; i++)
    {
      NSPOL_LOGEMG ("[%d]:%p\n", i, array[i]);
    }
  strings = backtrace_symbols (array, size);
  if (NULL == strings)
    {
      return;
    }
  for (i = 0; i < size; i++)
    {
      NSPOL_LOGEMG ("[%d]:%s\n", i, strings[i]);
    }
  NSPOL_LOGEMG
    ("-------------------------------------------------------------------\n");
  free (strings);
}

void
set_log_init_para (struct log_init_para *para)
{
  if (NULL == para)

    {
      return;
    }
  if (EOK !=
      MEMCPY_S (&g_log_init_para, sizeof (struct log_init_para), para,
                sizeof (struct log_init_para)))

    {
      return;
    }
}

/* control log printed counts */
static inline void
update_log_prt_time (struct timespec *cur_time, struct timespec *log_prt_time)
{
  log_prt_time->tv_sec = cur_time->tv_sec;
  log_prt_time->tv_nsec = cur_time->tv_nsec;
}

int
check_log_prt_time (int id)
{
  struct timespec cur_time;
  struct timespec *log_prt_time = NULL;
  if (id >= LOG_CTRL_ID_MAX || id < 0)

    {
      return 0;
    }
  (void) clock_gettime (CLOCK_MONOTONIC, &cur_time);
  log_prt_time = &g_log_ctrl_info[id].last_log_time;
  if (cur_time.tv_sec - log_prt_time->tv_sec >=
      g_log_ctrl_info[id].expire_time)
    {
      /* first log need print */
      set_log_ctrl_time (id, DEFAULT_LOG_CTR_TIME);
      update_log_prt_time (&cur_time, log_prt_time);
      return 1;
    }
  g_log_ctrl_info[id].unprint_count++;
  return 0;
}

int
get_unprt_log_count (int id)
{
  return g_log_ctrl_info[id].unprint_count;
}

void
clr_unprt_log_count (int id)
{
  g_log_ctrl_info[id].unprint_count = 0;
}

void
set_log_ctrl_time (int id, int ctrl_time)
{
  if (id >= LOG_CTRL_ID_MAX || id < 0)
    {
      return;
    }

  if (ctrl_time <= 0)
    {
      return;
    }

  g_log_ctrl_info[id].expire_time = ctrl_time;
}

void
init_log_ctrl_info ()
{
  int i = 0;
  for (; i < LOG_CTRL_ID_MAX; i++)
    {
      /* first log need print */
      g_log_ctrl_info[i].expire_time = 0;
      g_log_ctrl_info[i].unprint_count = 0;
      g_log_ctrl_info[i].last_log_time.tv_sec = 0;
      g_log_ctrl_info[i].last_log_time.tv_nsec = 0;
    }

  // for every socket api, need different log id

  // for nstack inner
}

void
set_log_proc_type (int log_proc_type)
{
  g_my_pro_type = log_proc_type;
}
