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

#include <pthread.h>
#include "nstack.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "common_mem_buf.h"
#include "common_mem_api.h"
#include "nstack_eventpoll.h"
#include "nstack_socket.h"
#include "nstack_securec.h"
#include "nsfw_init.h"
#include "nstack_share_res.h"
#include "nsfw_mgr_com_api.h"
#include "nsfw_ps_mem_api.h"
#include "nsfw_ps_api.h"
#include "nsfw_recycle_api.h"
#include "nstack_fd_mng.h"
#include "nstack_info_parse.h"
#include "nstack_dmm_adpt.h"
#include "nstack_rd_mng.h"
#include "nstack_rd.h"
#include "nstack_module.h"
#include "nstack_select.h"
#include "common_func.h"

#define NSTACK_EVENT_MEN_MAXLEN  (sizeof(struct eventpoll_pool) + NSTACK_MAX_EPOLL_NUM * sizeof(struct eventpoll_entry))
#define NSTACK_EPITEM_MEN_MAXLEN  (sizeof(struct epitem_pool) + NSTACK_MAX_EPITEM_NUM *sizeof(struct epitem_entry))
/* *INDENT-OFF* */
nStack_info_t g_nStackInfo = {
                                .hasInited = NSTACK_MODULE_INIT,
                                .fwInited = NSTACK_MODULE_INIT,
                                .moduleload = NSTACK_MODULE_INIT,
                                .load_mutex = PTHREAD_MUTEX_INITIALIZER,
                                .lk_sockPoll = NULL,
                                .pid = 0,
                                .fork_lock = {0},
                                .ikernelfdmax = NSTACK_MAX_SOCK_NUM,
                                };

/*if this flag was set, maybe all socket interface called during initializing*/
__thread int g_tloadflag = 0;

/*check init stack*/
#define NSTACK_INIT_STATE_CHECK_RET(state)  do {\
    if ((state) == NSTACK_MODULE_SUCCESS)  \
    {  \
        return 0;  \
    } \
    if ((state) == NSTACK_MODULE_FAIL) \
    { \
        return -1;  \
    }  \
}while(0);



int
nstack_timeval2msec (struct timeval *pTime, u64_t * msec)
{
  if (pTime->tv_sec < 0 || pTime->tv_usec < 0)
    {
      NSSOC_LOGERR ("time->tv_sec is negative");
      return -1;
    }

  if (NSTACK_MAX_U64_NUM / 1000 < (u64_t) pTime->tv_sec)
    {
      NSSOC_LOGERR ("tout.tv_sec is too large]tout.tv_sec=%lu",
            pTime->tv_sec);
      return -1;
    }
  u64_t sec2msec = 1000 * pTime->tv_sec;
  u64_t usec2msec = (u64_t) pTime->tv_usec / 1000;

  if (NSTACK_MAX_U64_NUM - sec2msec < usec2msec)
    {
      NSSOC_LOGERR
    ("nsec2msec plus sec2usec is too large]usec2msec=%lu,usec2msec=%lu",
     usec2msec, sec2msec);
      return -1;
    }

  *msec = sec2msec + usec2msec;
  return 0;
}

int
nstack_current_time2msec (u64_t * msec)
{
  struct timespec tout;
  if (unlikely (0 != clock_gettime (CLOCK_MONOTONIC, &tout)))
    {
      NSSOC_LOGERR ("Failed to get time, errno = %d", errno);
    }

  if (NSTACK_MAX_U64_NUM / 1000 < (u64_t) tout.tv_sec)
    {
      NSSOC_LOGERR ("tout.tv_sec is too large]tout.tv_sec=%lu", tout.tv_sec);
      return -1;
    }
  u64_t sec2msec = 1000 * tout.tv_sec;
  u64_t nsec2msec = (u64_t) tout.tv_nsec / 1000000;

  if (NSTACK_MAX_U64_NUM - sec2msec < nsec2msec)
    {
      NSSOC_LOGERR
    ("nsec2msec plus sec2usec is too large]nsec2msec=%lu,usec2msec=%lu",
     nsec2msec, sec2msec);
      return -1;
    }

  *msec = sec2msec + nsec2msec;

  return 0;
}


int
nstack_sem_timedwait (sem_t * pSem, u64_t abs_timeout /*ms */ )
{
  int retVal;

  u64_t starttime, endtime;

#define FAST_SLEEP_TIME 10000
#define SLOW_SLEEP_TIME 500000
#define FAST_RETRY_COUNT 100
  unsigned int retry_count = 0;

  if (nstack_current_time2msec (&starttime))
    {
      errno = ETIMEDOUT;
      return -1;
    }

  while (1)
    {
      retVal = sem_trywait (pSem);

      if (nstack_current_time2msec (&endtime))
    {
      errno = ETIMEDOUT;
      return -1;
    }

      /*when get event we return the time cost */
      if (retVal == 0)
    {
      return (endtime - starttime);
    }
      /*when time out it return 0 */
      if (endtime < starttime || (endtime - starttime) > abs_timeout)
    {
      errno = ETIMEDOUT;
      return abs_timeout;
    }

      /* app calling setsockopt to set  time */
      if (retry_count < FAST_RETRY_COUNT)
    {
      sys_sleep_ns (0, FAST_SLEEP_TIME);
      retry_count++;
    }
      else
    {
      sys_sleep_ns (0, SLOW_SLEEP_TIME);
    }
    }

}

/*epoll and select shouldnot get affected by system time change*/
int
nstack_epoll_sem_timedwait (sem_t * pSem, u64_t abs_timeout /*ms */ ,
                long wait_time /*us */ )
{
  int retVal;

  /* clock_gettime() get second variable is long, so here should use long */
  u64_t starttime, endtime;

#define FAST_SLEEP_TIME 10000
#define SLOW_SLEEP_TIME 500000
#define FAST_RETRY_COUNT 100
  unsigned int retry_count = 0;

  if (nstack_current_time2msec (&starttime))
    {
      errno = ETIMEDOUT;
      return -1;
    }

  while (1)
    {
      retVal = sem_trywait (pSem);

      if (retVal == 0)
    {
      return retVal;
    }

      if (nstack_current_time2msec (&endtime))
    {
      errno = ETIMEDOUT;
      return -1;
    }

      if (endtime < starttime || (endtime - starttime) > abs_timeout)
    {
      errno = ETIMEDOUT;
      return -1;
    }

      /*app calling setsockopt to set  time   */
      if (wait_time > 0)
    {
      long wait_sec;
      long wait_nsec;
      wait_sec = wait_time / 1000000;
      wait_nsec = 1000 * (wait_time % 1000000);
      sys_sleep_ns (wait_sec, wait_nsec);    //g_sem_sleep_time
    }
      else if (retry_count < FAST_RETRY_COUNT)
    {
      sys_sleep_ns (0, FAST_SLEEP_TIME);
      retry_count++;
    }
      else
    {
      sys_sleep_ns (0, SLOW_SLEEP_TIME);
    }
    }

}

NSTACK_STATIC inline char *
get_ver_head (char *version)
{
  const char *split = " ";
  char *tmp = NULL;
  char *next_pos = NULL;

  tmp = STRTOK_S (version, split, &next_pos);
  if (NULL == tmp || NULL == next_pos)
    {
      return NULL;
    }

  // version
  tmp = STRTOK_S (next_pos, split, &next_pos);
  if (NULL == tmp)
    {
      return NULL;
    }

  return tmp;
}

NSTACK_STATIC int
match_version (char *nstack_ver, char *my_ver)
{
  if ((NULL == nstack_ver || 0 == nstack_ver[0]) ||
      (NULL == my_ver || 0 == my_ver[0]))
    {
      NSSOC_LOGERR ("invalid input]");
      return 0;
    }

  char *nstack_ver_head = NULL;
  char *my_ver_head = NULL;

  char nstack_version[NSTACK_VERSION_LEN] = { 0 };
  char my_version[NSTACK_VERSION_LEN] = { 0 };

  // !!!STRTOK_S will modify the original string, so use use temp for parameter
  /* use STRCPY_S instead of MEMCPY_S to avoid invalid memory visit */
  if (EOK != STRCPY_S (nstack_version, sizeof (nstack_version), nstack_ver))
    {
      return 0;
    }

  nstack_ver_head = get_ver_head (nstack_version);
  if (NULL == nstack_ver_head)
    {
      return 0;
    }

  /*use STRCPY_S instead of MEMCPY_S to avoid invalid memory visit */
  if (EOK != STRCPY_S (my_version, sizeof (my_version), my_ver))
    {
      return 0;
    }

  my_ver_head = get_ver_head (my_version);
  if (NULL == my_ver_head)
    {
      return 0;
    }

  if (strlen (my_ver_head) != strlen (nstack_ver_head))
    {
      return 0;
    }


  if (0 != strncmp (nstack_ver_head, my_ver_head, strlen (nstack_ver_head)))
    {
      return 0;
    }

  return 1;
}

NSTACK_STATIC inline void
set_unmatch_version (char *version, unmatch_ver_info_t * app_ver_info)
{
  int i = 0;
  if (version == NULL || app_ver_info == NULL)
    {
      return;
    }

  for (; i < MAX_UNMATCH_VER_CNT; i++)
    {
      if (app_ver_info[i].unmatch_count != 0)
    {
      if (0 ==
          strncmp (version, app_ver_info[i].lib_version,
               NSTACK_VERSION_LEN - 1))
        {
          app_ver_info[i].unmatch_count++;
          return;
        }
    }
      else
    {
      if (__sync_bool_compare_and_swap (&app_ver_info[i].unmatch_count, 0, 1))
        {
          int retval =
        STRNCPY_S (app_ver_info[i].lib_version, NSTACK_VERSION_LEN,
               version, NSTACK_VERSION_LEN - 1);
          if (EOK != retval)
        {
          NSSOC_LOGERR ("STRNCPY_S failed]ret=%d", retval);
          return;
        }

          get_current_time (app_ver_info[i].first_time_stamp,
                LOG_TIME_STAMP_LEN);
          return;
        }
    }
    }
}

NSTACK_STATIC inline void
check_main_version ()
{
  char my_version[NSTACK_VERSION_LEN] = { 0 };
  nsfw_mem_name stname = { NSFW_SHMEM, NSFW_PROC_MAIN, {NSTACK_VERSION_SHM} };
  g_nStackInfo.nstack_version = nsfw_mem_zone_lookup (&stname);

  if (NULL == g_nStackInfo.nstack_version)
    {
      NSSOC_LOGERR ("can not get nstack version.");
      return;
    }

  if (EOK != STRCPY_S (my_version, sizeof (my_version), NSTACK_VERSION))
    {
      NSSOC_LOGERR ("STRCPY_S failed");
      return;
    }

  if (match_version (g_nStackInfo.nstack_version, my_version))
    {
      return;
    }

  NSSOC_LOGERR ("version not match]my version=%s, nStackMain_version=%s",
        my_version, g_nStackInfo.nstack_version);

  /* record unmatched app version in snapshot */
  char *unmatch_app_version =
    g_nStackInfo.nstack_version + NSTACK_VERSION_LEN;

  set_unmatch_version (my_version,
               (unmatch_ver_info_t *) unmatch_app_version);
}

int
nstack_init_shmem ()
{

  int deploytype = nstack_get_deploy_type ();

  if (deploytype > NSTACK_MODEL_TYPE1)
    {
      if (-1 == nsep_attach_memory ())
    {
      return -1;
    }
      if (nstack_attach_share_res () != 0)
    {
      return -1;
    }
    }
  else
    {
      if (nsep_create_memory () != 0)
    {
      return -1;
    }
      if (nstack_init_share_res () != 0)
    {
      return -1;
    }
    }
  return 0;
}


/**
 *  This should be called only once
 */
NSTACK_STATIC int
nstack_init_mem (void)
{

  int ret = ns_fail;
  int initflag = 0;
  int deploytype = nstack_get_deploy_type ();
    /* record unmatched app version*/
    /* check lib version match - Begin */
  if (deploytype > NSTACK_MODEL_TYPE1)
    {
      check_main_version ();
      initflag = 1;
    }

  ret = nstack_init_shmem ();
  if (ns_success != ret)
    {
      NSSOC_LOGERR ("nstack init shmem fail");
      return ns_fail;
    }

  /*stack-x rd mng init*/
  ret = nstack_rd_mng_int(initflag);
  if (ns_success != ret)
  {
     NSSOC_LOGERR("nstack_rd_mng_int fail");
     return ns_fail;
  }

  /*rd info sys*/
  ret = nstack_rd_sys();
  if (ns_success != ret)
  {
     NSSOC_LOGERR("nstack rd sys fail");
     return ns_fail;
  }

  (void)nstack_stack_module_init();

  /*init select mod*/
  if(FALSE == select_module_init()){
      return ns_fail;
  }
  return ns_success;
  /* The memory of the g_nStackInfo.lk_sockPoll  was not released in the exception*/
}

void
nstack_fork_fd_local_lock_info (nstack_fd_local_lock_info_t * local_lock)
{
  if (local_lock->fd_ref.counter > 1)    /* after fork, if fd ref > 1, need set it to 1 */
    {
      local_lock->fd_ref.counter = 1;
    }
  common_mem_spinlock_init (&local_lock->close_lock);
}

void
nstack_reset_fd_local_lock_info (nstack_fd_local_lock_info_t * local_lock)
{
  atomic_set (&local_lock->fd_ref, 0);
  common_mem_spinlock_init (&local_lock->close_lock);
  local_lock->fd_status = FD_CLOSE;
}

common_mem_rwlock_t *
get_fork_lock ()
{
  return &g_nStackInfo.fork_lock;
}

NSTACK_STATIC int
nstack_init_fd_local_info ()
{
  int iindex = 0;
  nstack_fd_Inf *fdInf;

  g_nStackInfo.lk_sockPoll = (nstack_fd_Inf *) malloc (NSTACK_KERNEL_FD_MAX * sizeof (nstack_fd_Inf));
  if (!g_nStackInfo.lk_sockPoll)
    {
      NSSOC_LOGERR ("malloc nstack_fd_lock_info failed");
      return ns_fail;
    }

  for (iindex = 0; iindex < (int) NSTACK_KERNEL_FD_MAX; iindex++)
    {
      fdInf = &g_nStackInfo.lk_sockPoll[iindex];
      nstack_reset_fdInf (fdInf);
    }

  if (-1 == nsep_init_infoSockMap ())
    {
      NSSOC_LOGERR ("malloc epInfoPool fail");
      if (g_nStackInfo.lk_sockPoll)
    {
      free (g_nStackInfo.lk_sockPoll);
      g_nStackInfo.lk_sockPoll = NULL;
    }
      return ns_fail;
    }

  return ns_success;
}



/*design ensures that g_ksInfo is not write accessed at the same time.
only read is done simultaneously with no chance of other thread writing it.
so no protection needed.*/
int
nstack_stack_init (void)
{                // Just need to create shared memory
  int ret;

  ret = nstack_init_fd_local_info ();
  if (ret != ns_success)
    {
      goto INIT_DONE;
    }

  if (ns_fail == nstack_init_mem ())
    {
      ret = ns_fail;
      goto INIT_DONE;
    }

  if (SYS_HOST_INITIAL_PID == get_sys_pid ())
    {
      ret = ns_fail;
      goto INIT_DONE;
    }

  ret = ns_success;

INIT_DONE:

  if (ns_success == ret)
    {
      NSSOC_LOGDBG ("success");
    }
  else
    {
      NSSOC_LOGERR ("fail");
    }
  return ret;
}

int
nstack_for_epoll_init ()
{
  NSSOC_LOGINF ("fork] init begin..");
  if (g_nStackInfo.pid != 0 && g_nStackInfo.pid != getpid ())
    {
      NSSOC_LOGINF ("fork]g_nStackInfo.pid=%u,getpid=%d", g_nStackInfo.pid,
            getpid ());

        nstack_stack_module_init();
    }
  return 0;
}



void
signal_handler_app (int s)
{
  NSPOL_LOGERR ("Received signal exiting.]s=%d", s);
  if (SIGHUP != s && SIGTERM != s)
    {
      nstack_segment_error (s);
    }
}

void
register_signal_handler_app ()
{
    /* handle function should comply secure coding standard
          here mask signal that will use in  sigwait() */
  sigset_t waitset, oset;
  if (0 != sigemptyset (&waitset))
    {
      NSPOL_LOGERR ("sigemptyset failed");
    }
  if (0 != sigaddset (&waitset, SIGRTMIN))    /* for timer */
    {
      NSPOL_LOGERR ("sigaddset failed");
    }
  if (0 != sigaddset (&waitset, SIGRTMIN + 2))
    {
      NSPOL_LOGERR ("sigaddset failed");
    }
  if (0 != pthread_sigmask (SIG_BLOCK, &waitset, &oset))
    {
      NSPOL_LOGERR ("pthread_sigmask failed");
    }


  struct sigaction s;
  s.sa_handler = signal_handler_app;
  if (0 != sigemptyset (&s.sa_mask))
    {
      NSPOL_LOGERR ("sigemptyset failed.");
    }

  s.sa_flags = (int) SA_RESETHAND;

    /* register sig handler for more signals */
  if (sigaction (SIGINT, &s, NULL) != 0)
    {
      NSPOL_LOGERR ("Could not register SIGINT signal handler.");
    }
  if (sigaction (SIGSEGV, &s, NULL) != 0)
    {
      NSPOL_LOGERR ("Could not register SIGSEGV signal handler.");
    }
  if (sigaction (SIGPIPE, &s, NULL) != 0)
    {
      NSPOL_LOGERR ("Could not register SIGPIPE signal handler.");
    }
  if (sigaction (SIGFPE, &s, NULL) != 0)
    {
      NSPOL_LOGERR ("Could not register SIGFPE signal handler.");
    }
  if (sigaction (SIGABRT, &s, NULL) != 0)
    {
      NSPOL_LOGERR ("Could not register SIGABRT signal handler.");
    }
  if (sigaction (SIGBUS, &s, NULL) != 0)
    {
      NSPOL_LOGERR ("Could not register SIGBUS signal handler.");
    }
    /* register sig handler for more signals */


}

int nstack_stack_module_load()
{
    /*check whether already inited*/
    NSTACK_INIT_STATE_CHECK_RET(g_nStackInfo.moduleload);

    /*lock for fork*/
    common_mem_rwlock_read_lock(get_fork_lock());

    if (pthread_mutex_lock(&g_nStackInfo.load_mutex)){
        NSSOC_LOGERR("nstack mutex lock fail");
        common_mem_rwlock_read_unlock(get_fork_lock());
        return -1;
    }

    NSTACK_INIT_STATE_CHECK_RET(g_nStackInfo.moduleload);

    NSTACK_THREAD_LOAD_SET();

    if (0 != nstack_module_parse())
    {
        NSSOC_LOGERR("nstack stack module parse fail");
        goto LOAD_FAIL;
    }

    if (0 != nstack_register_module())
    {
        NSSOC_LOGERR("nstack stack module parse fail");
        goto LOAD_FAIL;
    }

    NSTACK_THREAD_LOAD_UNSET();
    g_nStackInfo.moduleload = NSTACK_MODULE_SUCCESS;
    common_mem_rwlock_read_unlock(get_fork_lock());
    pthread_mutex_unlock(&g_nStackInfo.load_mutex);
    return 0;

LOAD_FAIL:
    g_nStackInfo.moduleload = NSTACK_MODULE_FAIL;
    NSTACK_THREAD_LOAD_UNSET();
    common_mem_rwlock_read_unlock(get_fork_lock());
    pthread_mutex_unlock(&g_nStackInfo.load_mutex);
    return -1;
}

int
nstack_app_init (void *ppara)
{
  NSSOC_LOGINF("nstack app init begin");

  g_nStackInfo.ikernelfdmax = nstack_get_maxfd_id(nstack_get_fix_mid());

  NSSOC_LOGINF("final max fd:%d", g_nStackInfo.ikernelfdmax);

  g_nStackInfo.pid = getpid ();

  /*if init already, just return success, if init fail before, just return err */
  if (NSTACK_MODULE_INIT != g_nStackInfo.hasInited)
    {
      NSSOC_LOGINF ("nstack app already init state:%d",
            g_nStackInfo.hasInited);
      return (NSTACK_MODULE_SUCCESS ==
          g_nStackInfo.hasInited ? ns_success : ns_fail);
    }

  if (0 != nstack_stack_init ())
    {
      NSSOC_LOGERR ("nstack stack init failed");
      g_nStackInfo.hasInited = NSTACK_MODULE_FAIL;
      return ns_fail;
    }

  g_nStackInfo.hasInited = NSTACK_MODULE_SUCCESS;
  NSSOC_LOGINF ("nstack app init success end");
  return ns_success;
}

/*nsocket call framework init fun*/
int
nstack_fw_init ()
{

  int ret = ns_fail;

  if (NSTACK_MODULE_SUCCESS == g_nStackInfo.fwInited)
    {
      return ns_success;
    }

  if (NSTACK_MODULE_INIT == g_nStackInfo.fwInited)
    {
      g_nStackInfo.fwInited = NSTACK_MODULE_INITING;
      nstack_log_init_app();

      if (0 != nstack_stack_module_load())
      {
            NSSOC_LOGERR("nstack stack module load fail");
         g_nStackInfo.fwInited = NSTACK_MODULE_FAIL;
         return -1;
      }

      common_mem_rwlock_read_lock (get_fork_lock ());
      updata_sys_pid ();
      u8 proc_type = NSFW_PROC_APP;
      nsfw_mem_para stinfo = { 0 };

      int deploytype = nstack_get_deploy_type();

      if (deploytype == NSTACK_MODEL_TYPE1)
      {
         proc_type  = NSFW_PROC_MAIN;
      }

      stinfo.iargsnum = 0;
      stinfo.pargs = NULL;
      stinfo.enflag = (fw_poc_type)proc_type;
      nstack_framework_setModuleParam(NSFW_MEM_MGR_MODULE, (void*)&stinfo);
      nstack_framework_setModuleParam(NSFW_MGR_COM_MODULE, (void*) ((long long)proc_type));
      nstack_framework_setModuleParam(NSFW_PS_MODULE,     (void*) ((long long)proc_type));
      nstack_framework_setModuleParam(NSFW_PS_MEM_MODULE, (void*) ((long long)proc_type));
      nstack_framework_setModuleParam(NSFW_RECYCLE_MODULE, (void*) ((long long)proc_type));
      NSTACK_THREAD_LOAD_SET();
      ret = nstack_framework_init();

      if (ns_success == ret)
      {
          g_nStackInfo.fwInited = NSTACK_MODULE_SUCCESS;
      }
      else
      {
          g_nStackInfo.fwInited = NSTACK_MODULE_FAIL;
      }
      NSTACK_THREAD_LOAD_UNSET();
      common_mem_rwlock_read_unlock(get_fork_lock());
    }

  return ret;
}

nstack_fd_local_lock_info_t *
get_fd_local_lock_info (int fd)
{
  if (!g_nStackInfo.lk_sockPoll)
    {
      return NULL;
    }

  if (fd >= 0 && fd < (int) NSTACK_KERNEL_FD_MAX)
    {
      return &(g_nStackInfo.lk_sockPoll[fd].local_lock);
    }

  return NULL;
}
