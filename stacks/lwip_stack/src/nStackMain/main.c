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

#include <signal.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <malloc.h>
#include "nstack_securec.h"
#include "nstack_log.h"
#include "types.h"
#include "nsfw_init.h"
#include "alarm_api.h"

#include "nsfw_mgr_com_api.h"
#include "nsfw_ps_mem_api.h"
#include "nsfw_ps_api.h"
#include "nsfw_recycle_api.h"
#include "nsfw_fd_timer_api.h"
#include "nsfw_maintain_api.h"
#include "nstack_dmm_adpt.h"

#include "nsfw_mem_api.h"
#include "nsfw_mt_config.h"

#define GLOBAL_Stack_ARG "stack"
#define GlOBAL_HELP "--help"
#define GLOBAL_Dpdk_ARG "dpdk"
#define GLOBAL_STACK_PORT "-port"
#define NSTACK_MAIN_MAX_PARA 19
#define NSTACK_MAIN_MIN_PARA 1

#define MAX_MASTER_OPEN_FD 1024

extern int uStackArgIndex;
extern void spl_tcpip_thread (void *arg);
extern int globalArgc;
extern char **gArgv;
extern int g_dpdk_argc;
extern char **g_dpdk_argv;
extern void print_call_stack ();
extern int adjust_mem_arg (int argc, char *argv[]);
extern struct cfg_item_info g_cfg_item_info[CFG_SEG_MAX][MAX_CFG_ITEM];
extern int get_network_json_data ();
extern int get_ip_json_data ();

#ifdef SYS_MEM_RES_STAT
extern void get_resource_stat ();
#endif

extern alarm_result ms_alarm_check_func (void *para);
int g_tcpip_thread_stat = 0;    //this variable will be used at health check feature

#if (DPDK_MODULE)
extern void pal_common_usage (void);
#endif

typedef void (*pSignalFunc) (int);
int app_init (void);

void
helpInfo ()
{
  NSPOL_LOGERR
    ("-----------------------------------------------------------------"
     "\nThe format of arg" "\nbash:./nStackMain RTE-ARGS  stack STACK-ARGS"
     "\n RTE-ARGS=-c COREMASK -n NUM [-b <domain:bus:devid.func>] [--socket-mem=MB,...] [-m MB] [-r NUM] [-v] [--file-prefix] [--proc-type <primary|secondary|auto>] [-- xen-dom0]"
     "\n STACK-ARGS=-port PORTMASK [-c COREMASK] [-thread NUM]"
     "\nUser examples:"
     "\nTo make the DPDK  run on core mask:0xf,Number of memory channels per processor socket in DPDK is 3 "
     "\nTo make the stack  run on eth coremask:f"
     "\nbash: ./nStackMain -c 0xf -n 3 stack -port f"
     "\nTo get more help info for stack:" "\n   ./nStackMain --help stack"
     "\nTo get more help info for dpdk:" "\n   ./nStackMain --help dpdk"
     "\n-----------------------------------------------------------------");

#ifndef FOR_NSTACK_UT
  exit (0);
#endif

}

#define SIG_PTRACE __SIGRTMIN + 5

void
signal_handler (int s)
{
  NSPOL_LOGERR ("Received signal exiting.]s=%d", s);
  if (s == SIGSEGV)             /* add for gdb attach work */
    {
      print_call_stack ();
    }

  nstack_segment_error (s);
  if ((SIG_PTRACE != s) && (SIG_PTRACE + 2 != s))
    {
#ifndef FOR_NSTACK_UT
      exit (0);
#endif
    }

  int i;
  for (i = 0; i < MAX_THREAD; i++)
    {
      if (g_all_thread[i] != pthread_self () && 0 != g_all_thread[i])
        {
          NSFW_LOGERR ("send sig thread %d", g_all_thread[i]);
          if (0 == pthread_kill (g_all_thread[i], SIG_PTRACE + 2))
            {
              return;
            }
        }
      g_all_thread[i] = 0;
    }

  for (i = 0; i < MAX_THREAD; i++)
    {
      if (0 != g_all_thread[i])
        {
          return;
        }
    }

#ifndef FOR_NSTACK_UT
  exit (0);
#endif

  //dpdk_signal_handler();
}

void
register_signal_handler ()
{
/* donot catch SIGHUP SIGTERM */
  static const int s_need_handle_signals[] = {
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGILL,
    SIGIOT,
    SIGQUIT,
    SIGSEGV,
    //SIGTRAP,
    SIGXCPU,
    SIGXFSZ,
    //SIGALRM,
    //SIGHUP,
    SIGINT,
    //SIGKILL,
    SIGPIPE,
    SIGPROF,
    SIGSYS,
    //SIGTERM,
    SIGUSR1,
    SIGUSR2,
    //SIGVTALRM,
    //SIGSTOP,
    //SIGTSTP,
    //SIGTTIN,
    //SIGTTOU
  };

  /* here mask signal that will use in  sigwait() */
  sigset_t waitset, oset;

  if (0 != sigemptyset (&waitset))
    {
      NSPOL_LOGERR ("sigemptyset failed.");
    }
  sigaddset (&waitset, SIGRTMIN);       /* for timer */
  sigaddset (&waitset, SIGRTMIN + 2);
  pthread_sigmask (SIG_BLOCK, &waitset, &oset);
  unsigned int i = 0;

  struct sigaction s;
  s.sa_handler = signal_handler;
  if (0 != sigemptyset (&s.sa_mask))
    {
      NSPOL_LOGERR ("sigemptyset failed.");
    }

  s.sa_flags = (int) SA_RESETHAND;

  /* register sig handler for more signals */
  for (i = 0; i < sizeof (s_need_handle_signals) / sizeof (int); i++)
    {
      if (sigaction (s_need_handle_signals[i], &s, NULL) != 0)
        {
          NSPOL_LOGERR ("Could not register %d signal handler.",
                        s_need_handle_signals[i]);
        }

    }

}

void
checkArgs (int argc, char **argv)
{
  int uStackArg = 0;            //mark the status whether need helpinfo and return
  int i;
  const unsigned int global_para_length = 5;    //GLOBAL_Stack_ARG "stack" and  GLOBAL_STACK_PORT "-port" string length, both are 5
  const unsigned int global_help_length = 6;    //GlOBAL_HELP "--help" string length is 6
  const unsigned int global_dpdk_length = 4;    //GLOBAL_Dpdk_ARG "dpdk" string length is 4

  for (i = 0; i < argc; i++)
    {
      if (argc > 1)
        {
          if (strncmp (argv[i], GLOBAL_Stack_ARG, global_para_length) == 0)
            {
              if (i < 5)
                {
                  NSPOL_LOGERR ("Too less args");
                  helpInfo ();
                  return;
                }

              uStackArg = 1;
              uStackArgIndex = i;
              continue;
            }

          if (strncmp (argv[i], GLOBAL_STACK_PORT, global_para_length) == 0)
            {
              continue;
            }

          if (strncmp (argv[i], GlOBAL_HELP, global_help_length) == 0)
            {
              if (i == (argc - 1))
                {
                  helpInfo ();
                  return;
                }
              else if ((++i < argc)
                       &&
                       (strncmp (argv[i], GLOBAL_Dpdk_ARG, global_dpdk_length)
                        == 0))
                {
#if (DPDK_MODULE != 1)
                  //eal_common_usage();
#else
                  pal_common_usage ();
#endif
                  return;
                }
              else
                {
                  helpInfo ();
                  return;
                }
            }
        }
      else
        {
          NSPOL_LOGERR ("Too less args");
          helpInfo ();
          return;
        }
    }

  if (!uStackArg)
    {
      helpInfo ();
      return;
    }
}

extern u64 timer_get_threadid ();

#ifdef FOR_NSTACK_UT
int
nstack_main (void)
{
  int argc;

  char *argv[NSTACK_MAIN_MAX_PARA];
  argv[0] = "nStackMain";
  argv[1] = "-c";;
  argv[2] = "0xffffffff";
  argv[3] = "-n";
  argv[4] = "3";
  argv[5] = "stack";
  argv[6] = "-port";
  argv[7] = "288";
  argv[8] = "-c";
  argv[9] = "2";
  argc = 10;

#else
int
main (int argc, char *argv[])
{
#endif
  fw_poc_type proc_type = NSFW_PROC_MAIN;

  /* although nStackMaster has set close on exec, here can't be removed.
   * in upgrade senario, if Master is old which has not set close on exec, here,
   * if new version Main not close, problem will reappear. Add Begin*/
  int i;
  for (i = 4; i < MAX_MASTER_OPEN_FD; i++)
    {
      close (i);
    }

  cfg_module_param config_para;
  config_para.proc_type = proc_type;
  config_para.argc = argc;
  config_para.argv = (unsigned char **) argv;
  config_module_init (&config_para);

  set_log_proc_type (LOG_PRO_NSTACK);
  (void) nstack_log_init ();

//    nstack_tracing_enable();
  if (0 != nsfw_proc_start_with_lock (NSFW_PROC_MAIN))
    {
      NSFW_LOGERR ("another process is running!!");
      return 0;
    }

  if (1 != mallopt (M_ARENA_MAX, 1))
    {
      NSPOL_LOGERR ("Error: mallopt fail, continue init");
    }

  /* Set different mem args to PAL and EAL */
  INITPOL_LOGINF ("main_thread", "adjust_mem_arg init", NULL_STRING,
                  LOG_INVALID_VALUE, MODULE_INIT_START);
  if (adjust_mem_arg (argc, argv))
    {
      INITPOL_LOGERR ("main_thread", "adjust_mem_arg init", NULL_STRING,
                      LOG_INVALID_VALUE, MODULE_INIT_FAIL);
      return -1;
    }
  INITPOL_LOGINF ("main_thread", "adjust_mem_arg init", NULL_STRING,
                  LOG_INVALID_VALUE, MODULE_INIT_SUCCESS);

  checkArgs (globalArgc, gArgv);
  register_signal_handler ();

  (void) signal (SIG_PTRACE, signal_handler);
  (void) signal (SIG_PTRACE + 2, signal_handler);

  if (nsfw_mgr_comm_fd_init (NSFW_PROC_MAIN) < 0)
    {
      NSFW_LOGERR ("nsfw_mgr_comm_fd_init failed");
    }

  (void) nsfw_reg_trace_thread (pthread_self ());

  (void) nstack_framework_setModuleParam (NSFW_ALARM_MODULE,
                                          (void *) ((u64) proc_type));
  (void) nstack_framework_setModuleParam (TCPDUMP_MODULE,
                                          (void *) ((u64) proc_type));

  nstack_dmm_para stpara;
  stpara.argc = uStackArgIndex;
  stpara.argv = gArgv;
  stpara.deploy_type = NSTACK_MODEL_TYPE3;
  stpara.proc_type = NSFW_PROC_MAIN;
  stpara.attr.policy = get_cfg_info (CFG_SEG_PRI, CFG_SEG_THREAD_PRI_POLICY);
  stpara.attr.pri = get_cfg_info (CFG_SEG_PRI, CFG_SEG_THREAD_PRI_PRI);

  if (nstack_adpt_init (&stpara) != 0)
    {
      NSFW_LOGERR ("nstack adpt init fail");
      return -1;
    }

  ns_send_init_alarm (ALARM_EVENT_NSTACK_MAIN_ABNORMAL);

  while (!timer_get_threadid ())
    {
      (void) nsfw_thread_chk ();
      sys_sleep_ns (0, 500000000);
    }
  (void) nsfw_thread_chk_unreg ();
  NSFW_LOGINF ("tcpip thread start!");
  (void) sleep (2);

  NSFW_LOGINF ("read configuration!");
  if (0 != get_network_json_data ())
    {
      NSFW_LOGINF ("get_network_json_data error");
    }

  if (0 != get_ip_json_data ())
    {
      NSFW_LOGINF ("get_ip_json_data error");
    }

  int ep_thread = 0;
  while (1)
    {
#ifdef SYS_MEM_RES_STAT
      get_resource_stat ();
#endif
      (void) sleep (3);
      ep_thread = nsfw_mgr_com_chk_hbt (1);
      if (ep_thread > 0)
        {
          NSFW_LOGINF ("force release lock");
          (void) nsfw_recycle_rechk_lock ();
        }
#ifdef FOR_NSTACK_UT
      return 0;
#endif

    }
}
