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

#include "opt.h"
#include "nsfw_mem_api.h"
#include "def.h"
#include "sc_dpdk.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "nstack_share_res.h"

#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <syscall.h>
#include "nsfw_ps_api.h"
#include "spl_timers.h"
#include "common_mem_api.h"

extern sys_thread_t g_timerThread_id;

/*Since the range of the dpdk read TSC value is u64_t, it changes*/

#ifndef typecheck
#define typecheck(type, x) \
({  type __dummy; \
    typeof(x)__dummy2; \
    (void)(&__dummy == &__dummy2); \
    1; \
 })
#endif

#ifndef time_after_eq
#define time_after_eq(a, b) \
(typecheck(unsigned long, a)    \
   && typecheck(unsigned long, b)    \
   && ((long)(a) - (long)(b) >= 0))
#endif

#define MIN_TIME_PICE    50
#define MICRO_PER_SECOND 1000
#define NANO_PER_MICROSECOND 1000000
#define NANO_PER_SECOND 1000000000

#define PTIMER_MSG_BUFFER_SIZE 4096

#define PTIMER_STATE_INACTIVE 0x00
#define PTIMER_STATE_ENQUEUED 0x01
#define gettid() syscall(__NR_gettid)

#define TIMER_CONTEXT_NAME "TIMER_CONTEXT_NAME"

static timer_t lazy_tim;
static timer_t lazy_tim_retry;
static struct ptimer_base ptimer;
static sys_sem_t ptimer_lock;

extern void recycle_tmr (struct ptimer_node *tmo);

extern nstack_tick_info_t g_nstack_timer_tick;

/*
 ***************************
 * rb_tree wrapper function*
 ***************************
 */
NSTACK_STATIC void
remove_ptimer (struct ptimer_node *tmo, struct ptimer_base *base)
{
  if (tmo == NULL)
    {
      NSPOL_LOGERR ("input parameter tmo is null");
      return;
    }
  if (!(tmo->state & PTIMER_STATE_ENQUEUED))
    {
      NSPOL_LOGWAR (TIMERS_DEBUG | LWIP_DBG_LEVEL_WARNING,
                    "the timer state is: PTIMER_STATE_INACTIVE, no need to remove!");
      return;
    }

  if (base->first == &tmo->node)
    {
      base->first = rb_next (&tmo->node);
    }

  rb_erase (&tmo->node, &base->active);

  tmo->state = PTIMER_STATE_INACTIVE;
}

NSTACK_STATIC struct ptimer_node *
search_ptimer (struct ptimer_msg *msg)
{
  if (msg == NULL)
    {
      NSPOL_LOGERR ("input parameter msg is null");
      return NULL;
    }
  if (msg->node)
    {
      return msg->node;
    }

  return NULL;

}

err_t
del_ptimer (struct ptimer_msg * msg)
{
  struct ptimer_node *tmo;

  tmo = search_ptimer (msg);
  if (tmo)
    {
      remove_ptimer (tmo, &ptimer);
      NSPOL_LOGDBG (TIMERS_DEBUG, "del]thread=%u,ptimer_node=%p", tmo->index,
                    tmo);
      return ERR_OK;
    }

  NSPOL_LOGERR ("ptime_node not found!!");
  return ERR_VAL;
}

/*
 * Ptimer inserted into rb_tree
 * @param node: the ptimer node pointer
 * @param base: the ptimer root_base pointer
 */
NSTACK_STATIC void
enqueue_ptimer (struct ptimer_node *tmo, struct ptimer_base *base)
{
  struct rb_node **linknode;
  struct rb_node *parent = NULL;
  struct ptimer_node *entry;
  int leftmost = 1;
  if (tmo == NULL || base == NULL)
    {
      NSPOL_LOGERR ("input parameter is null");
      return;
    }

  if ((tmo->state & PTIMER_STATE_ENQUEUED))
    {
      NSPOL_LOGWAR (TIMERS_DEBUG | LWIP_DBG_LEVEL_WARNING,
                    "the timer state is: PTIMER_STATE_ENQUEUED");
      return;
    }

  linknode = &base->active.rb_node;
  while (*linknode)
    {
      parent = *linknode;
      entry = rb_entry (parent, struct ptimer_node, node);
      //XXX: do by the equal case is all of price of the large case
      if (time_after_eq (entry->abs_nsec, tmo->abs_nsec))
        {
          linknode = &(*linknode)->rb_left;
        }
      else
        {
          linknode = &(*linknode)->rb_right;
          leftmost = 0;
        }
    }

  /*
   * Insert the timer to the rb_tree and check whether it
   * replaces the first pending timer
   */
  if (leftmost)
    {
      base->first = &tmo->node;
    }

  rb_link_node (&tmo->node, parent, linknode);
  rb_insert_color (&tmo->node, &base->active);
  NSPOL_LOGDBG (TIMERS_DEBUG, "enqueue]thread=%u,ptimer_node=%p", tmo->index,
                tmo);
  tmo->state |= PTIMER_STATE_ENQUEUED;
}

void
launch_tmr (timer_t * tim, u64_t nsec)
{
  struct itimerspec vtim;

  vtim.it_value.tv_sec = (nsec) / NANO_PER_SECOND;
  vtim.it_value.tv_nsec = (nsec) % NANO_PER_SECOND;
  vtim.it_interval.tv_sec = 0;
  vtim.it_interval.tv_nsec = 0;

  if (timer_settime (*tim, 0, &vtim, NULL) < 0)
    {
      NSPOL_LOGERR ("add_ptimer:timer_settime failed");
    }

}

/*
 * add ptimer to rb_tree
 * @param tmo: the ptimer node pointer
 */
void
add_ptimer (struct ptimer_node *tmo)
{
  if (tmo == NULL)
    {
      NSPOL_LOGERR ("input parameter is null");
      return;
    }
  enqueue_ptimer (tmo, &ptimer);

  if (ptimer.first == &tmo->node)
    {
      launch_tmr (&lazy_tim, tmo->info.msec * NANO_PER_MICROSECOND);
    }
}

#if 1
/*
 * cond signal ,send a ptimer message
 * @param type: the message type
 * @param handler: timeout handler
 * @param node: the ptimer node pointer
 */
void
regedit_ptimer (enum msg_type Type, sys_timeout_handler handler,
                struct ptimer_node *node)
{
  (void) handler;
  (void) pthread_mutex_lock (&ptimer.lock);
  if (ptimer.tail == NULL)
    {
      (void) pthread_mutex_unlock (&ptimer.lock);
      NSPOL_LOGERR ("ptimer.tail is null");
      free (node);
      node = NULL;
      return;
    }
  ptimer.tail->msg_type = Type;
  ptimer.tail->node = node;
  ptimer.tail = ptimer.tail->next;
  if (ptimer.head == ptimer.tail)
    {
      (void) pthread_mutex_unlock (&ptimer.lock);
      NSPOL_LOGERR ("ptimer.head equal to ptimer.tail");
      return;
    }

  (void) pthread_kill (g_timerThread_id, SIGRTMIN + 2);

  (void) pthread_mutex_unlock (&ptimer.lock);
  return;
}
#endif
/*
 * deal with the timeout signal
 *
 */
void
deal_timeout_sig (void)
{
  struct ptimer_node *tmo;
  struct timespec now;
  unsigned long abs_nsec;
  err_t err;
  int retval;
  if (ptimer.first == NULL)
    {
      return;
    }
  tmo = rb_entry (ptimer.first, struct ptimer_node, node);
  retval = clock_gettime (CLOCK_MONOTONIC, &now);
  if (0 != retval)
    {
      NSPOL_LOGERR ("clock_gettime failed]retval=%d,errno=%d", retval, errno);
    }
  abs_nsec = now.tv_sec * NANO_PER_SECOND + now.tv_nsec;

  // deal with all timeout point
  while (time_after_eq (abs_nsec, tmo->abs_nsec))
    {
      remove_ptimer (tmo, &ptimer);

      if (tmo->info.flags & PTIMER_ONESHOT)
        {
        }
      else
        {
          //re-entry periodic ptimer
          tmo->abs_nsec = now.tv_sec * NANO_PER_SECOND + now.tv_nsec
            + tmo->info.msec * NANO_PER_MICROSECOND;
          add_ptimer (tmo);
        }

      //send timeout message
      NSPOL_LOGDBG (INTERRUPT_DEBUG,
                    "ptimer happened to thread_index]index=%u", tmo->index);
      if ((err = ltt_apimsg (tmo->info._phandle, (void *) tmo)))
        {
          NSPOL_LOGWAR (TIMERS_DEBUG | LWIP_DBG_LEVEL_WARNING,
                        "send timeout message failed!]errno=%d", err);
        }

      if (ptimer.first == NULL)
        {
          NSPOL_LOGERR ("deal_timeout_sig: ptimer.first == NULL!!");
          return;
        }
      tmo = rb_entry (ptimer.first, struct ptimer_node, node);
    }

  if (tmo->abs_nsec <= abs_nsec)
    {
      NSPOL_LOGERR ("tmo->abs_nsec is smaller than abs_nsec");
      return;
    }

  NSPOL_LOGDBG (TIMERS_DEBUG, "TIMERSIGNAL : Launch timer for]time=%ul",
                tmo->abs_nsec - abs_nsec);
  launch_tmr (&lazy_tim, tmo->abs_nsec - abs_nsec);     //timer interupted eveny 50ms instand by tmo->abs_nsec
}

/*
 * timeout signal handle function
 * @param v: unused argument
 */
/*ptimer is already protected and used*/

NSTACK_STATIC void
timeout_sigaction (int sig)
{
  (void) sig;
  if (!sys_arch_sem_trywait (&ptimer_lock))
    {
      launch_tmr (&lazy_tim_retry,
                  (MIN_TIME_PICE / 5) * NANO_PER_MICROSECOND);
      return;
    }

  deal_timeout_sig ();

  sys_sem_signal (&ptimer_lock);

  return;
}

/*
 * initialize the ptimer buffer and timing mechanism
 */
err_t
init_ptimer (void)
{
  int i, retval;
  struct sigevent timer_event;
  struct sigevent timer_event1;
  struct ptimer_msg *ptr;

  if (pthread_mutex_init (&ptimer.lock, NULL))
    {
      NSPOL_LOGERR ("pthread_mutex_init failed");
      return ERR_MEM;
    }
  /*codex fix */
  if (ERR_MEM == sys_sem_new (&ptimer_lock, 1))
    {
      NSPOL_LOGERR ("init_ptimer: sys_sem_new failure");
      return ERR_MEM;
    }

  ptimer.active.rb_node = NULL;
  ptimer.first = NULL;

  ptr =
    (struct ptimer_msg *) malloc (PTIMER_MSG_BUFFER_SIZE *
                                  sizeof (struct ptimer_msg));
  if (!ptr)
    {
      NSPOL_LOGERR ("init_ptimer: malloc ptimer buffer failed!");
      return ERR_MEM;
    }

  int ret =
    MEMSET_S (ptr, (PTIMER_MSG_BUFFER_SIZE * sizeof (struct ptimer_msg)),
              0, (PTIMER_MSG_BUFFER_SIZE * sizeof (struct ptimer_msg)));
  if (EOK != ret)
    {
      free (ptr);
      NSPOL_LOGERR ("init_ptimer: MEMSET_S ptimer buffer failed]ret=%d", ret);
      return ERR_MEM;
    }

  for (i = 0; i < PTIMER_MSG_BUFFER_SIZE - 1; i++)
    {
      ptr[i].next = &ptr[(i + 1)];
      ptr[(i + 1)].prev = &ptr[i];
    }

  ptr[i].next = &ptr[0];
  ptr[0].prev = &ptr[i];
  ptimer.head = ptimer.tail = &ptr[0];
  retval =
    MEMSET_S (&timer_event, sizeof (struct sigevent), 0,
              sizeof (struct sigevent));
  if (EOK != retval)
    {
      free (ptr);
      ptr = NULL;
      NSPOL_LOGERR ("MEMSET_S failed]ret=%d", retval);
      return ERR_VAL;
    }
  timer_event.sigev_notify = SIGEV_SIGNAL;
  timer_event.sigev_signo = SIGRTMIN;
  timer_event.sigev_value.sival_ptr = (void *) &lazy_tim;
  timer_event1 = timer_event;
  timer_event1.sigev_value.sival_ptr = (void *) &lazy_tim_retry;

  if (timer_create (CLOCK_MONOTONIC, &timer_event, &lazy_tim) < 0)
    {
      free (ptr);
      ptr = NULL;
      NSPOL_LOGERR ("ptimer_init: timer_create failed!");
      return ERR_VAL;
    }

  if (timer_create (CLOCK_MONOTONIC, &timer_event1, &lazy_tim_retry) < 0)
    {
      free (ptr);
      ptr = NULL;
      NSPOL_LOGERR ("ptimer_init: timer_create failed!");
      return ERR_VAL;
    }

  return ERR_OK;
}

NSTACK_STATIC err_t
process_timeout_msg (struct ptimer_msg * msg)
{
  struct ptimer_node *tmo = msg->node;
  struct timespec now;

  switch (msg->msg_type)
    {
    case SYS_PTIMEROUT_MSG:
      if (tmo)
        {
          int retval = clock_gettime (CLOCK_MONOTONIC, &now);
          if (0 != retval)
            {
              NSPOL_LOGERR ("clock_gettime failed]retval=%d,errno=%d", retval,
                            errno);
            }
          tmo->abs_nsec = now.tv_sec * NANO_PER_SECOND + now.tv_nsec
            + tmo->info.msec * NANO_PER_MICROSECOND;
          add_ptimer (tmo);
        }
      else
        {
          NSPOL_LOGERR ("process_timeout_msg: rb_node is NULL!");
          return ERR_MEM;
        }

      break;
    case SYS_UNPTIMEROUT_MSG:
      if (del_ptimer (msg) != ERR_OK)
        {
          NSPOL_LOGERR
            ("process_timeout_msg: can not find the timeout event(oops)!");
        }

      break;
    default:
      NSPOL_LOGERR
        ("process_timeout_msg: oops! lwip timeout_thread receive unacquainted message!");
      break;
    }

  return ERR_OK;
}

void
ptimer_thread (void *arg)
{
  LWIP_UNUSED_ARG (arg);
  int sig, ret = -1;
  sigset_t waitset;
  sigset_t oset;
  struct ptimer_msg *msg;

  if (0 != sigemptyset (&waitset))
    {
      NSTCP_LOGERR ("sigemptyset function call error");
      return;
    }

  if (0 != sigaddset (&waitset, SIGRTMIN))
    {
      NSTCP_LOGERR ("sigaddset function call error");
      return;
    }

  if (0 != sigaddset (&waitset, SIGRTMIN + 2))
    {
      NSTCP_LOGERR ("sigaddset function call error");
      return;
    }

  (void) pthread_sigmask (SIG_BLOCK, &waitset, &oset);

  (void) pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
  (void) pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  NSPOL_LOGDBG (TIMERS_DEBUG, "ptimer: init done !");

  if (!g_nstack_timer_tick.tick_ptr)
    {
      NSTCP_LOGERR ("g_nstack_timer_tick not inited");
      return;
    }
  (void) gettimeofday (&g_nstack_timer_tick.ref_time, NULL);
  g_nstack_timer_tick.interval = 100;
  *g_nstack_timer_tick.tick_ptr = 0;
  g_nstack_timer_tick.ref_tick = 0;
  while (1)
    {
      ret = sigwait (&waitset, &sig);
      (void) nsfw_thread_chk ();        // will only return TRUE, no need to check return value
      if (ret != -1)
        {
          /* for timer expirt handle */
          if (SIGRTMIN == sig)
            {
              timeout_sigaction (sig);
              continue;
            }
        }
      else
        {
          continue;
        }

      while (1)
        {
          (void) pthread_mutex_lock (&ptimer.lock);
          if (ptimer.head == ptimer.tail)
            {
              (void) pthread_mutex_unlock (&ptimer.lock);
              break;
            }

          msg = ptimer.head;
          ptimer.head = ptimer.head->next;
          (void) pthread_mutex_unlock (&ptimer.lock);

          sys_arch_sem_wait (&ptimer_lock, 0);

          if (process_timeout_msg (msg) != ERR_OK)
            {
              NSPOL_LOGERR ("oops: ptimer_thread panic!");
            }

          sys_sem_signal (&ptimer_lock);
        }
    }
}

void
timeout_phandler (void *act, void *arg)
{
  struct ptimer_node *tmo = arg;
  if (act == NULL)
    {
      NSPOL_LOGERR ("input parameter arg is null");
      return;
    }

  NSPOL_LOGINF (TIMERS_DEBUG, "Timer expire @timeout_phandler Handle %p",
                tmo->info._phandle);
  if (tmo->info.flags & PTIMER_ONESHOT)
    {
      sys_timeout_handler handle = act;
      handle (tmo->info.ctx);
      return;
    }
  else
    {
      NSPOL_LOGINF (TIMERS_DEBUG,
                    "Timer expire error @timeout_phandler Handle %p",
                    tmo->info._phandle);
    }
}
