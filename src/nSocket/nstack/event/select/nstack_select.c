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
#include "nstack_select.h"
#include "nstack_log.h"
#include "nsfw_base_linux_api.h"

/*==============================================*
 *      constants or macros define              *
 *----------------------------------------------*/
#ifdef NSTACK_SELECT_MODULE

/*==============================================*
 *      project-wide global variables           *
 *----------------------------------------------*/
extern void *nstack_select_thread (void *arg);
/*************select module***************************/
struct select_module_info g_select_module = {
  .inited = FALSE,
};

/*==============================================*
 *      routines' or functions' implementations *
 *----------------------------------------------*/
/*****************************************************************************
*   Prototype    : get_select_module
*   Description  : get_select_module
*   Input        : void
*   Output       : None
*   Return Value : struct select_module_info *
*   Calls        :
*   Called By    :
*****************************************************************************/
struct select_module_info *
get_select_module (void)
{
  return &g_select_module;
}

/*split comm seclet entry to child mod select*/
/*****************************************************************************
*   Prototype    : select_cb_split_by_mod
*   Description  : select_cb_split_by_module
*   Input        : i32 nfds
*                  fd_set *readfd
*                  fd_set *writefd
*                  fd_set *exceptfd
*                  struct select_entry *entry
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_cb_split_by_mod (i32 nfds,
                        fd_set * readfd,
                        fd_set * writefd,
                        fd_set * exceptfd, struct select_entry * entry)
{
  i32 inx;
  i32 i;
  i32 fd;

  for (i = 0; i < nfds; i++)
    {

      /*not bound to any stack */
      for (inx = 0; inx < get_mode_num (); inx++)
        {
          if (!((readfd && FD_ISSET (i, readfd)) ||
                (writefd && FD_ISSET (i, writefd)) ||
                (exceptfd && FD_ISSET (i, exceptfd))))
            {
              continue;
            }
          fd = select_get_modfd (i, inx);

          /*not create by nstack */
          if ((fd < 0) || (select_get_modindex (i) < 0))
            {

              if (inx != get_mode_linux_index ())
                {
                  continue;
                }
              fd = i;
              nssct_create (fd, fd, inx);
            }
          else
            {
              if (select_get_modindex (i) != inx)
                continue;
            }
          NSSOC_LOGDBG ("fd is  available i= %d fd = %d index = %d\n", i, fd,
                        inx);
          if ((readfd) && (FD_ISSET (i, readfd)))
            {
              NSTACK_FD_SET (fd, &(entry->cb[inx].nstack_readset));
              if (entry->cb[inx].count <= fd)
                entry->cb[inx].count = fd + 1;
            }

          if ((writefd) && (FD_ISSET (i, writefd)))
            {
              NSTACK_FD_SET (fd, &(entry->cb[inx].nstack_writeset));
              if (entry->cb[inx].count <= fd)
                entry->cb[inx].count = fd + 1;
            }

          if ((exceptfd) && (FD_ISSET (i, exceptfd)))
            {
              NSTACK_FD_SET (fd, &(entry->cb[inx].nstack_exceptset));
              if (entry->cb[inx].count <= fd)
                entry->cb[inx].count = fd + 1;
            }
        }
    }

  for (inx = 0; inx < get_mode_num (); inx++)
    {
      if (entry->cb[inx].count > 0)
        {
          entry->info.set_num++;
          entry->info.index = inx;
        }
    }
  return TRUE;
}

/*****************************************************************************
*   Prototype    : select_add_cb
*   Description  : add cb to global list
*   Input        : struct select_entry *entry
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_add_cb (struct select_entry * entry)
{

  if ((!entry))
    {

      return FALSE;
    }
  select_spin_lock (&g_select_module.lock);

  if (!g_select_module.entry_head)
    {
      g_select_module.entry_head = entry;
      g_select_module.entry_tail = entry;
      entry->next = NULL;
      entry->prev = NULL;
    }
  else
    {
      g_select_module.entry_tail->next = entry;
      entry->prev = g_select_module.entry_tail;
      g_select_module.entry_tail = entry;
      entry->next = NULL;
    }

  select_spin_unlock (&g_select_module.lock);
  select_sem_post (&g_select_module.sem);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : select_rm_cb
*   Description  : rm the cb from global list
*   Input        : struct select_entry *entry
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_rm_cb (struct select_entry * entry)
{

  if (!entry)
    {

      return FALSE;
    }

  select_spin_lock (&g_select_module.lock);

  if (g_select_module.entry_head == entry)
    {
      g_select_module.entry_head = entry->next;

    }
  else if (entry->prev)
    {
      entry->prev->next = entry->next;
    }

  if (g_select_module.entry_tail == entry)
    {
      g_select_module.entry_tail = entry->prev;
    }
  else if (entry->next)
    {
      entry->next->prev = entry->prev;
    }

  entry->next = NULL;
  entry->prev = NULL;

  select_spin_unlock (&g_select_module.lock);
  return TRUE;
}

/*get fd set from entry*/
/*****************************************************************************
*   Prototype    : select_thread_get_fdset
*   Description  : get module listening  fd form global list
*   Input        : nstack_fd_set *readfd
*                  nstack_fd_set *writefd
*                  nstack_fd_set *exceptfd
*                  struct select_module_info *module
*                  i32 inx
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_thread_get_fdset (nstack_fd_set * readfd,
                         nstack_fd_set * writefd,
                         nstack_fd_set * exceptfd,
                         struct select_module_info * module, i32 inx)
{

  struct select_entry *tmp;
  i32 nfds = 0;
  int retVal;

  if (!module)
    {
      return FALSE;
    }

  retVal = NSTACK_FD_ZERO (readfd);
  retVal |= NSTACK_FD_ZERO (writefd);
  retVal |= NSTACK_FD_ZERO (exceptfd);
  if (EOK != retVal)
    {
      NSSOC_LOGERR ("NSTACK_FD_ZERO MEMSET_S failed]ret=%d", retVal);
      return FALSE;
    }

  select_spin_lock (&module->lock);
  for (tmp = module->entry_head; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->cb[inx].count <= 0)
        {
          continue;
        }

      NSTACK_FD_OR (readfd, &tmp->cb[inx].nstack_readset);
      NSTACK_FD_OR (writefd, &tmp->cb[inx].nstack_writeset);
      NSTACK_FD_OR (exceptfd, &tmp->cb[inx].nstack_exceptset);
      if (nfds < tmp->cb[inx].count)
        {
          nfds = tmp->cb[inx].count;
        }
    }
  select_spin_unlock (&module->lock);

  return nfds;
}

/*****************************************************************************
*   Prototype    : select_thread_set_fdset
*   Description  : set ready event to global list
*   Input        : i32 nfds
*                  nstack_fd_set *readfd
*                  nstack_fd_set *writefd
*                  nstack_fd_set *exceptfd
*                  struct select_module_info *module
*                  i32 inx
*                  i32 err
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_thread_set_fdset (i32 nfds,
                         nstack_fd_set * readfd,
                         nstack_fd_set * writefd,
                         nstack_fd_set * exceptfd,
                         struct select_module_info * module, i32 inx, i32 err)
{

  struct select_entry *tmp;

  if (!module)
    {
      return FALSE;
    }

  select_spin_lock (&module->lock);
  for (tmp = module->entry_head; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->cb[inx].count <= 0)
        {
          continue;
        }

      if (nfds < 0)
        {
          tmp->ready.readyset = nfds;
          tmp->ready.select_errno = err;
          continue;
        }
      NSSOC_LOGDBG ("readyset=%d,index=%d", tmp->ready.readyset, inx);
      entry_module_fdset (tmp, nfds, readfd, writefd, exceptfd, inx);
    }
  select_spin_unlock (&module->lock);
  return TRUE;

}

/*****************************************************************************
*   Prototype    : select_event_post
*   Description  : when event ready post sem to awake nstack_select
*   Input        : struct select_module_info *module
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
select_event_post (struct select_module_info *module)
{
  struct select_entry *tmp;
  int inx;
  select_spin_lock (&module->lock);
  for (tmp = module->entry_head; NULL != tmp; tmp = tmp->next)
    {

      if ((tmp->ready.readyset != 0))
        {
          for (inx = 0; inx < get_mode_num (); inx++)
            {
              tmp->cb[inx].count = 0;
            }
          NSSOC_LOGDBG ("readyset=%d", tmp->ready.readyset);
          select_sem_post (&tmp->sem);
        }
    }
  select_spin_unlock (&module->lock);
}

/*set select_event  function*/
/*****************************************************************************
*   Prototype    : select_module_init
*   Description  : init select module
*   Input        : None
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_module_init ()
{
  i32 i;
  pthread_t select_thread_id;
  i32 retval;

  if (fdmapping_init () < 0)
    {
      goto ERR_RET;
    }

  g_select_module.default_mod = get_mode_linux_index ();
  g_select_module.default_fun = nsfw_base_select;

  /*regist select fun */
  for (i = 0; i < get_mode_num (); i++)
    {
      g_select_module.get_select_fun_nonblock[i] =
        nstack_module_ops (i)->pfselect;
    }

  select_sem_init (&g_select_module.sem, 0, 0);
  select_spin_lock_init (&g_select_module.lock);

  if (pthread_create (&select_thread_id, NULL, nstack_select_thread, NULL))
    {

      goto ERR_RET;
    }

  retval = pthread_setname_np (select_thread_id, "nstack_select");
  if (retval)
    {
      /*set thread name failed */
    }

  g_select_module.inited = TRUE;
  g_select_module.entry_head = g_select_module.entry_tail = NULL;
  return TRUE;

ERR_RET:

  return FALSE;
}

/*****************************************************************************
*   Prototype    : entry_module_fdset
*   Description  : set event
*   Input        : struct select_entry *entry
*                  i32 fd_size
*                  nstack_fd_set *readfd
*                  nstack_fd_set *writefd
*                  nstack_fd_set *exceptfd
*                  i32 inx
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
entry_module_fdset (struct select_entry *entry,
                    i32 fd_size,
                    nstack_fd_set * readfd,
                    nstack_fd_set * writefd,
                    nstack_fd_set * exceptfd, i32 inx)
{
  i32 i;
  i32 fd;

  for (i = 0; i < fd_size; i++)
    {
      fd = select_get_commfd (i, inx);
      if (fd < 0)
        {
          continue;
        }
      if (NSTACK_FD_ISSET (i, readfd)
          && NSTACK_FD_ISSET (i, &entry->cb[inx].nstack_readset))
        {
          FD_SET (fd, &entry->ready.readset);
          entry->ready.readyset++;
          NSSOC_LOGDBG ("readyset is %d", entry->ready.readyset);
        }

      if (NSTACK_FD_ISSET (i, writefd)
          && NSTACK_FD_ISSET (i, &entry->cb[inx].nstack_writeset))
        {
          FD_SET (fd, &entry->ready.writeset);
          entry->ready.readyset++;
          NSSOC_LOGDBG ("writeset is %d", entry->ready.readyset);
        }

      if (NSTACK_FD_ISSET (i, exceptfd)
          && NSTACK_FD_ISSET (i, &entry->cb[inx].nstack_exceptset))
        {
          FD_SET (fd, &entry->ready.exceptset);
          entry->ready.readyset++;
          NSSOC_LOGDBG ("exceptset is %d", entry->ready.readyset);
        }
    }

}

/*****************************************************************************
*   Prototype    : select_scan
*   Description  : scan all modules to check event ready or not
*   Input        : struct select_entry *entry
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_scan (struct select_entry *entry)
{
  i32 inx;
  i32 fd_size;
  i32 ready;
  nstack_fd_set *readfd;
  nstack_fd_set *writefd;
  nstack_fd_set *exceptfd;
  struct timeval timeout;

  readfd = malloc (sizeof (nstack_fd_set));
  writefd = malloc (sizeof (nstack_fd_set));
  exceptfd = malloc (sizeof (nstack_fd_set));
  if ((!readfd) || (!writefd) || (!exceptfd))
    {
      NSPOL_LOGERR ("malloc nstack_fd_set fail");
      FREE_FD_SET (readfd, writefd, exceptfd);
      return -1;
    }
  for (inx = 0; inx < get_mode_num (); inx++)
    {

      *readfd = entry->cb[inx].nstack_readset;
      *writefd = entry->cb[inx].nstack_writeset;
      *exceptfd = entry->cb[inx].nstack_exceptset;
      fd_size = entry->cb[inx].count;
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
      if ((g_select_module.get_select_fun_nonblock[inx]) && (fd_size > 0))
        {
          ready =
            g_select_module.get_select_fun_nonblock[inx] (fd_size,
                                                          (fd_set *) readfd,
                                                          (fd_set *) writefd,
                                                          (fd_set *) exceptfd,
                                                          &timeout);
        }
      else
        {
          continue;
        }

      if (ready > 0)
        {
          entry_module_fdset (entry, fd_size, readfd, writefd, exceptfd, inx);
        }
      else if (ready < 0)
        {
          entry->ready.readyset = ready;
          entry->ready.select_errno = errno;
          NSSOC_LOGERR ("select failed index = %d", inx);
          FREE_FD_SET (readfd, writefd, exceptfd);
          return FALSE;
        }

    }
  FREE_FD_SET (readfd, writefd, exceptfd);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : lint_lock
*   Description  : avoid lint error
*   Input        : None
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline void
lint_lock ()
{
  return;
}

/*****************************************************************************
*   Prototype    : lint_unlock
*   Description  : avoid lint error
*   Input        : None
*   Output       : None
*   Return Value : static inline void
*   Calls        :
*   Called By    :
*****************************************************************************/
static inline void
lint_unlock ()
{
  return;
}

/*****************************************************************************
*   Prototype    : nstack_select_thread
*   Description  : if global list not null scaning all modules ,need to think
                   about block mod
*   Input        : void *arg
*   Output       : None
*   Return Value : void *
*   Calls        :
*   Called By    :
*****************************************************************************/
void *
nstack_select_thread (void *arg)
{

#define  SELECT_SLEEP_TIME  800 //us

  i32 inx;
  nstack_fd_set *readfd;
  nstack_fd_set *writefd;
  nstack_fd_set *exceptfd;
  i32 fd_size;
  i32 ready;
  i32 sleep_time = SELECT_SLEEP_TIME;
  struct timeval timeout;

  lint_lock ();

  readfd = malloc (sizeof (nstack_fd_set));
  writefd = malloc (sizeof (nstack_fd_set));
  exceptfd = malloc (sizeof (nstack_fd_set));
  if ((!readfd) || (!writefd) || (!exceptfd))
    {
      NSPOL_LOGERR ("malloc nstack_fd_set fail");
      FREE_FD_SET (readfd, writefd, exceptfd);
      lint_unlock ();
      return NULL;
    }

  /*used nonblock  need add block mod later */

  for (;;)
    {
      /*wait app calling select no cong cpu */
      if (!g_select_module.entry_head)
        {
          select_sem_wait (&g_select_module.sem);
        }

      for (inx = 0; inx < get_mode_num (); inx++)
        {

          fd_size =
            select_thread_get_fdset (readfd, writefd, exceptfd,
                                     &g_select_module, inx);
          if (fd_size <= 0)
            {
              continue;
            }

          if (g_select_module.get_select_fun_nonblock[inx])
            {
              ready =
                g_select_module.get_select_fun_nonblock[inx] (fd_size,
                                                              (fd_set *)
                                                              readfd,
                                                              (fd_set *)
                                                              writefd,
                                                              (fd_set *)
                                                              exceptfd,
                                                              &timeout);
            }
          else
            {
              continue;
            }

          if (ready > 0)
            {
              select_thread_set_fdset (fd_size, readfd, writefd, exceptfd,
                                       &g_select_module, inx, 0);
            }
          else if (ready < 0)
            {
              select_thread_set_fdset (ready, readfd, writefd, exceptfd,
                                       &g_select_module, inx, errno);
              NSSOC_LOGERR ("module[%d] select failed] ret = %d errno = %d",
                            inx, ready, errno);
              lint_unlock ();
              break;
            }

        }
      select_event_post (&g_select_module);
      timeout.tv_sec = 0;
      timeout.tv_usec = sleep_time;
      lint_unlock ();
      /*use linux select for timer */
      nsfw_base_select (1, NULL, NULL, NULL, &timeout);
    }
}

/*****************************************************************************
*   Prototype    : nssct_create
*   Description  : create a select record for event fd
*   Input        : i32 cfd
*                  i32 mfd
*                  i32 inx
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nssct_create (i32 cfd, i32 mfd, i32 inx)
{
  if (g_select_module.inited != TRUE)
    {
      return;
    }
  select_set_modfd (cfd, inx, mfd);
  select_set_commfd (mfd, inx, cfd);
}

/*****************************************************************************
*   Prototype    : nssct_close
*   Description  : rm the record
*   Input        : i32 cfd
*                  i32 inx
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nssct_close (i32 cfd, i32 inx)
{
  if (g_select_module.inited != TRUE)
    {
      return;
    }
  i32 mfd = select_get_modfd (cfd, inx);
  select_set_modfd (cfd, inx, -1);
  select_set_commfd (mfd, inx, -1);
  select_set_index (cfd, -1);
}

/*****************************************************************************
*   Prototype    : nssct_set_index
*   Description  : set select fd index
*   Input        : i32 fd
*                  i32 inx
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nssct_set_index (i32 fd, i32 inx)
{
  if (g_select_module.inited != TRUE)
    {
      return;
    }
  select_set_index (fd, inx);
}

i32
select_module_init_child ()
{
  pthread_t select_thread_id;
  i32 retval;

  if (pthread_create (&select_thread_id, NULL, nstack_select_thread, NULL))
    {
      goto ERR_RET;
    }

  retval = pthread_setname_np (select_thread_id, "nstack_select_child");
  if (retval)
    {
      /*set thread name failed */
    }
  return TRUE;

ERR_RET:
  return FALSE;
}

#endif /* NSTACK_SELECT_MODULE */
