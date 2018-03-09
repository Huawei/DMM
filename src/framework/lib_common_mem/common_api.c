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

#include <string.h>
#include "common_mem_api.h"
#include "common_mem_pal.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "common_func.h"

void
sys_sem_init_v2 (sys_sem_t_v2 sem)
{
  sem->locked = 1;
}

/** Returns the current time in milliseconds,
 * may be the same as sys_jiffies or at least based on it. */
long
sys_now (void)
{
  struct timespec now;

  if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &now)))
    {
      NSCOMM_LOGERR ("Failed to get time, errno = %d", errno);
    }

  return 1000 * now.tv_sec + now.tv_nsec / 1000000;
}

long
sys_jiffies (void)
{
  return sys_now ();
}

err_t
sys_sem_new_v2 (sys_sem_t_v2 * sem, u8_t isUnLockd)
{
  int retVal;
  if (!sem)
    {
      return -1;
    }
  *sem = malloc (sizeof (common_mem_spinlock_t));

  if (NULL == *sem)
    {
      return -1;
    }
  else
    {
      retVal =
        MEMSET_S (*sem, sizeof (common_mem_spinlock_t), 0,
                  sizeof (common_mem_spinlock_t));
      if (EOK != retVal)
        {
          NSCOMM_LOGERR ("MEMSET_S failed]ret=%d", retVal);
          free (*sem);
          *sem = NULL;
          return -1;
        }
      common_mem_spinlock_init (*sem);
    }

  if (!isUnLockd)
    {
      common_mem_spinlock_lock (*sem);
    }

  return 0;
}

void
sys_sem_free_v2 (sys_sem_t_v2 * sem)
{
  if ((sem != NULL) && (*sem != NULL))
    {
      free (*sem);
      *sem = NULL;
    }
  else
    {
    }
}

void
sys_sem_signal_v2 (sys_sem_t_v2 * sem)
{
  common_mem_spinlock_unlock (*sem);
}

void
sys_sem_signal_s_v2 (sys_sem_t_v2 sem)
{
  common_mem_spinlock_unlock (sem);
}

u32_t
sys_arch_sem_trywait_v2 (sys_sem_t_v2 * sem)
{
  return (u32_t) common_mem_spinlock_trylock (*sem);
}

u32_t
sys_arch_sem_wait_v2 (sys_sem_t_v2 * pstsem)
{
  common_mem_spinlock_lock (*pstsem);
  return 0;
}

u32_t
sys_arch_sem_wait_s_v2 (sys_sem_t_v2 sem)
{
  common_mem_spinlock_lock (sem);
  return 0;
}

volatile pid_t g_sys_host_pid = SYS_HOST_INITIAL_PID;

/*****************************************************************************
*   Prototype    : get_exec_name_by_pid
*   Description  : get exec name by pid
*   Input        : pid_t pid
*                  char *task_name
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
get_exec_name_by_pid (pid_t pid, char *task_name, int task_name_len)
{
  int retVal;
  char path[READ_FILE_BUFLEN] = { 0 };
  char buf[READ_FILE_BUFLEN] = { 0 };

  retVal = SPRINTF_S (path, sizeof (path), "/proc/%d/status", pid);
  if (-1 == retVal)
    {
      NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
      return;
    }

  FILE *fp = fopen (path, "r");
  if (NULL != fp)
    {
      if (fgets (buf, READ_FILE_BUFLEN - 1, fp) == NULL)
        {
          fclose (fp);
          return;
        }
      fclose (fp);

#ifdef SECUREC_LIB
      retVal = SSCANF_S (buf, "%*s %s", task_name, task_name_len);
#else
      retVal = SSCANF_S (buf, "%*s %s", task_name);
#endif

      if (1 != retVal)
        {
          NSSOC_LOGERR ("SSCANF_S failed]ret=%d", retVal);
          return;
        }
    }
}

pid_t
get_hostpid_from_file_one_time (u32_t pid)
{
  int retVal;
  char path[READ_FILE_BUFLEN] = { 0 };
  char buf[READ_FILE_BUFLEN] = { 0 };
  char fmt[READ_FILE_BUFLEN] = { 0 };
  char out[READ_FILE_BUFLEN] = { 0 };
  char task_name[BUF_SIZE_FILEPATH] = { 0 };
  pid_t hostpid = SYS_HOST_INITIAL_PID;
  get_exec_name_by_pid (pid, task_name, BUF_SIZE_FILEPATH);

  retVal = SPRINTF_S (fmt, sizeof (fmt), "%s%s", task_name, " (%s");
  if (-1 == retVal)
    {
      NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
      return hostpid;
    }
  retVal = SPRINTF_S (path, sizeof (path), "/proc/%d/sched", pid);
  if (-1 == retVal)
    {
      NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
      return hostpid;
    }
  FILE *fp = fopen (path, "r");
  if (NULL != fp)
    {
      if (fgets (buf, READ_FILE_BUFLEN - 1, fp) == NULL)
        {
          fclose (fp);
          return hostpid;
        }
      fclose (fp);
      /* Compiler needs "fmt" to be like "%s%s" to
         understand. But we have "fmt" already prepared and used here. It can
         be suppressed, not an issue
       */
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
      retVal = SSCANF_S (buf, fmt, out, READ_FILE_BUFLEN);
      if (-1 == retVal)
        {
          NSPOL_LOGERR ("SSCANF_S failed]ret=%d", retVal);
          return hostpid;
        }
    }

  hostpid = (pid_t) strtol (out, NULL, 0);
  if (hostpid == 0)
    {
      hostpid = 1;
    }

  return hostpid;
}

#define MAX_GET_PID_TIME 10
pid_t
get_hostpid_from_file (u32_t pid)
{
  pid_t ret_pid = SYS_HOST_INITIAL_PID;
  int i = 0;
  ret_pid = get_hostpid_from_file_one_time (pid);
  while (0 == ret_pid || ret_pid == SYS_HOST_INITIAL_PID)
    {
      i++;
      if (i > MAX_GET_PID_TIME)
        {
          NSFW_LOGERR ("get pid failed]pid=%u,hostpid=%d", pid, ret_pid);
          break;
        }
      sys_sleep_ns (0, 5000000);
      ret_pid = get_hostpid_from_file_one_time (pid);
    }

  return ret_pid;
}

/*****************************************************************************
*   Prototype    : get_hostpid_from_file
*   Description  : get host pid by sub namespace pid in docker
*   Input        : uint32_t pid
*   Output       : None
*   Return Value : uint32_t
*   Calls        :
*   Called By    :
*****************************************************************************/
pid_t
sys_get_hostpid_from_file (pid_t pid)
{
  g_sys_host_pid = get_hostpid_from_file (pid);
  NSCOMM_LOGDBG ("ok]cur pid=%d, input pid=%d", g_sys_host_pid, pid);
  return g_sys_host_pid;
}

pid_t
updata_sys_pid ()
{
  g_sys_host_pid = SYS_HOST_INITIAL_PID;
  return get_sys_pid ();
}

/*****************************************************************************
*   Prototype    : get_pid_namespace
*   Description  : Get namespace of pid
*   Input        : uint32_t pid
*   Output       : None
*   Return Value : uint64_t
*   Calls        :
*   Called By    :
*****************************************************************************/
uint64_t
get_pid_namespace (pid_t pid)
{
  char buf[BUF_SIZE_FILEPATH] = { 0 };
  char path[BUF_SIZE_FILEPATH] = { 0 };
  const char *pstart;
  int ret = 0;
  if (!pid)
    {
      NSSOC_LOGERR ("Pid is zero!");
      return 0;
    }

  ret =
    SNPRINTF_S (path, sizeof (path), sizeof (path) - 1, "/proc/%d/ns/pid",
                pid);
  if (-1 == ret)
    {
      NSSOC_LOGERR ("SNPRINTF_S failed]ret=%d", ret);
      return 0;
    }

  ret = readlink (path, buf, sizeof (buf) - 1);
  if (ret <= (int) sizeof (STR_PID))
    {
      NSSOC_LOGERR ("readlink pid ns file error]ret=%d", ret);
      return 0;
    }

  buf[ret - 1] = 0;
  pstart = &(buf[sizeof (STR_PID)]);
  return strtoull (pstart, NULL, 10);
}
