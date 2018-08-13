#include <sys/queue.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <linux/sched.h>

#include <asm/unistd.h>
#include <linux/unistd.h>
#include <syscall.h>

#include <lwip/sys.h>
#include <cc.h>
#include <arch/sys_arch.h>
#include <arch/queue.h>
#include "nstack_log.h"
#include "lwip/sockets.h"
#include "spl_timers.h"
#include "nstack_securec.h"
#include "nstack_share_res.h"
#include "nstack_log.h"

#include "stackx_spl_share.h"
#include "lwip/api.h"
#include "lwip/ip.h"
#include <netif/sc_dpdk.h>
#include <netif/sharedmemory.h>

#include "common_mem_common.h"
#include "common_func.h"
#include "common_mem_api.h"

#include "mgr_com.h"
#ifdef HAL_LIB
#else
#include "rte_ring.h"
#endif

#include "timeouts.h"

sys_sem_st g_global_semaphore;

/** Check if an mbox is valid/allocated: return 1 for valid, 0 for invalid */
int
sys_mbox_valid (sys_mbox_t * mbox)
{
  return ((*mbox == SYS_MBOX_NULL) ? 0 : 1);
}

struct lwip_thread
{
  void (*func) (void *arg);
  void *arg;
};

static void *
lwip_thread_entry (void *arg)
{
  struct lwip_thread *lt = arg;

  lt->func (lt->arg);

  free (lt);
  return 0;
}

/** The only thread function:
 * Creates a new thread
 * @param name human-readable name for the thread (used for debugging purposes)
 * @param thread thread-function
 * @param arg parameter passed to 'thread'
 * @param stacksize stack size in bytes for the new thread (may be ignored by ports)
 * @param prio priority of the new thread (may be ignored by ports) */
sys_thread_t
sys_thread_new2 (const char *name, lwip_thread_fn thread, void *arg,
                 int stacksize, int prio, int policy)
{
  pthread_attr_t attr;
  set_thread_attr (&attr, stacksize, prio, policy);

  struct lwip_thread *lt = malloc (sizeof (*lt));
  if (lt == NULL)
    {
      NSPOL_LOGERR ("process abort:cannot allocate thread struct");
      abort ();
    }

  lt->func = thread;
  lt->arg = arg;

  pthread_t t;
  int r = pthread_create (&t, &attr, lwip_thread_entry, lt);

  if (r != 0)
    {
      NSPOL_LOGERR ("process abort:lwip:annot create]errno_string=%s",
                    strerror (r));
      abort ();
    }
  else
    {
      NSPOL_LOGINF (SC_DPDK_INFO, "]thread_name=%s.", name);
    }

  (void) pthread_setname_np (t, name);

  return t;
}

/** The only thread function:
 * Creates a new thread
 * @param name human-readable name for the thread (used for debugging purposes)
 * @param thread thread-function
 * @param arg parameter passed to 'thread'
 * @param stacksize stack size in bytes for the new thread (may be ignored by ports)
 * @param prio priority of the new thread (may be ignored by ports) */
sys_thread_t
sys_thread_new (const char *name, lwip_thread_fn thread, void *arg,
                int stacksize, int prio)
{
  pthread_attr_t attr;
  set_thread_attr (&attr, stacksize, prio, 0);

  struct lwip_thread *lt = malloc (sizeof (*lt));
  if (lt == NULL)
    {
      NSPOL_LOGERR ("process abort:cannot allocate thread struct");
      abort ();
    }

  lt->func = thread;
  lt->arg = arg;

  pthread_t t;
  int r = pthread_create (&t, &attr, lwip_thread_entry, lt);

  if (r != 0)
    {
      NSPOL_LOGERR ("process abort:lwip:annot create]errno_string=%s",
                    strerror (r));
      abort ();
    }
  else
    {
      NSPOL_LOGINF (SC_DPDK_INFO, "]thread_name=%s.", name);
    }

  (void) pthread_setname_np (t, name);

  return t;
}

void
stackx_global_lock (void)
{
  sys_arch_lock_with_pid (&g_global_semaphore);
}

void
stackx_global_unlock (void)
{
  sys_sem_s_signal (&g_global_semaphore);
}

void
sys_init (void)
{
  return;
}

void
sys_timeouts_mbox_fetch (sys_mbox_t * mbox, void **msg)
{
  return;
}

void
sys_timeout (u32_t msecs, sys_timeout_handler handler, void *arg)
{
  struct ptimer_node *tmo = arg;

  tmo = malloc (sizeof (struct ptimer_node));   /*lint !e586 */
  if (NULL == tmo)
    {
      NSPOL_LOGERR ("malloc ptimer node failed!");
      return;
    }

  int ret = MEMSET_S (tmo, sizeof (struct ptimer_node), 0,
                      sizeof (struct ptimer_node));
  if (EOK != ret)
    {
      NSPOL_LOGERR ("MEMSET_S failed]ret=%d", ret);
      free (tmo);
      return;
    }

  NSPOL_LOGDBG (TIMERS_DEBUG, "alloc]ptimer_node=%p", tmo);

  tmo->info.msec = msecs;
  tmo->info._phandle = handler;
  tmo->info.ctx = arg;
  tmo->info.flags = PTIMER_ONESHOT;
  tmo->index = 0;
  regedit_ptimer (SYS_PTIMEROUT_MSG, handler, tmo);
}

void
sys_untimeout (sys_timeout_handler handler, void *arg)
{
  regedit_ptimer (SYS_UNPTIMEROUT_MSG, handler, arg);
  return;
}

/**
 * Timer callback function that calls mld6_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
void
cyclic_timer (void *arg)
{
  const struct lwip_cyclic_timer *cyclic =
    (const struct lwip_cyclic_timer *) arg;
#if LWIP_DEBUG_TIMERNAMES
  LWIP_DEBUGF (TIMERS_DEBUG, ("tcpip: %s()\n", cyclic->handler_name));
#endif
  cyclic->handler ();
  sys_timeout (cyclic->interval_ms, cyclic_timer, arg);
}
