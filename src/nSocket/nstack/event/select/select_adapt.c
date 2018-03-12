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
#include "select_adapt.h"

/*==============================================*
 *      constants or macros define              *
 *----------------------------------------------*/

/*==============================================*
 *      project-wide global variables           *
 *----------------------------------------------*/
i32 nstack_mod_fd[NSTACK_MAX_MODULE_NUM][NSTACK_SELECT_MAX_FD];
struct select_fd_map_inf g_select_fd_map;

/*==============================================*
 *      routines' or functions' implementations *
 *----------------------------------------------*/

/*****************************************************************************
*   Prototype    : select_alloc
*   Description  : select_alloc
*   Input        : int size
*   Output       : None
*   Return Value : void *
*   Calls        :
*   Called By    :
*****************************************************************************/
void *
select_alloc (int size)
{

  char *p;
  if (size <= 0)
    {
      return NULL;
    }

  p = malloc (size);
  if (!p)
    {
      return NULL;
    }
  if (EOK != MEMSET_S (p, size, 0, size))
    {
      free (p);
      p = NULL;
    }

  return p;
}

/*****************************************************************************
*   Prototype    : select_free
*   Description  : select_free
*   Input        : char *p
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
select_free (char *p)
{

  if (p)
    {
      free (p);
      p = NULL;
    }
}

/*****************************************************************************
*   Prototype    : get_select_fdinf
*   Description  : get_select_fdinf
*   Input        : i32 fd
*   Output       : None
*   Return Value : struct select_comm_fd_map *
*   Calls        :
*   Called By    :
*****************************************************************************/
struct select_comm_fd_map *
get_select_fdinf (i32 fd)
{
  if ((fd < 0) || (fd >= NSTACK_SELECT_MAX_FD))
    {
      return NULL;
    }
  return (&g_select_fd_map.fdinf[fd]);
}

/*****************************************************************************
*   Prototype    : reset_select_fdinf
*   Description  : reset_select_fdinf
*   Input        : i32 fd
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
reset_select_fdinf (i32 fd)
{
  i32 i;
  struct select_comm_fd_map *fdinf = get_select_fdinf (fd);
  if (NULL == fdinf)
    {
      return;
    }
  fdinf->index = -1;
  for (i = 0; i < NSTACK_MAX_MODULE_NUM; i++)
    {
      fdinf->mod_fd[i] = -1;
    }
}

/*****************************************************************************
*   Prototype    : select_get_modfd
*   Description  : select_get_modfd
*   Input        : i32 fd
*                  i32 inx
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_get_modfd (i32 fd, i32 inx)
{
  if ((fd < 0) || (fd >= NSTACK_SELECT_MAX_FD))
    {
      return -1;
    }
  if ((inx < 0))
    {
      return -1;
    }
  if (!g_select_fd_map.fdinf)
    {
      return FALSE;
    }
  return (g_select_fd_map.fdinf[fd].mod_fd[inx]);

}

/*****************************************************************************
*   Prototype    : select_set_modfd
*   Description  : select_set_modfd
*   Input        : i32 fd
*                  i32 inx
*                  i32 modfd
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_set_modfd (i32 fd, i32 inx, i32 modfd)
{
  if ((fd < 0) || (fd >= NSTACK_SELECT_MAX_FD))
    {
      return -1;
    }
  if (!g_select_fd_map.fdinf)
    {
      return FALSE;
    }
  g_select_fd_map.fdinf[fd].mod_fd[inx] = modfd;

  return TRUE;
}

/*****************************************************************************
*   Prototype    : select_get_modindex
*   Description  : select_get_modindex
*   Input        : i32 fd
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_get_modindex (i32 fd)
{
  if ((fd < 0) || (fd >= NSTACK_SELECT_MAX_FD))
    {
      return -1;
    }
  return g_select_fd_map.fdinf[fd].index;
}

/*****************************************************************************
*   Prototype    : select_get_commfd
*   Description  : select_get_commfd
*   Input        : i32 modfd
*                  i32 inx
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_get_commfd (i32 modfd, i32 inx)
{

  if ((modfd < 0) || (modfd >= NSTACK_SELECT_MAX_FD))
    {
      return -1;
    }
  return g_select_fd_map.modinf[inx].comm_fd[modfd];
}

/*****************************************************************************
*   Prototype    : select_set_commfd
*   Description  : select_set_commfd
*   Input        : i32 modfd
*                  i32 inx
*                  i32 fd
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_set_commfd (i32 modfd, i32 inx, i32 fd)
{
  if ((modfd < 0) || (modfd >= NSTACK_SELECT_MAX_FD))
    {
      return -1;
    }
  if (!g_select_fd_map.modinf[inx].comm_fd)
    {
      return FALSE;
    }
  g_select_fd_map.modinf[inx].comm_fd[modfd] = fd;

  return TRUE;
}

/*****************************************************************************
*   Prototype    : select_set_index
*   Description  : select_set_index
*   Input        : i32 fd
*                  i32 inx
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
select_set_index (i32 fd, i32 inx)
{
  if ((fd < 0) || (fd >= NSTACK_SELECT_MAX_FD))
    {
      return -1;
    }
  if (!g_select_fd_map.fdinf)
    {
      return FALSE;
    }
  g_select_fd_map.fdinf[fd].index = inx;
  return TRUE;
}

/*****************************************************************************
*   Prototype    : fdmapping_init
*   Description  : fdmapping_init
*   Input        : void
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
fdmapping_init (void)
{
  int ret = FALSE;
  int i, inx;

  g_select_fd_map.fdinf =
    (struct select_comm_fd_map *)
    select_alloc (sizeof (struct select_comm_fd_map) * NSTACK_SELECT_MAX_FD);
  if (NULL == g_select_fd_map.fdinf)
    {
      goto err_return;
    }

  for (i = 0; i < get_mode_num (); i++)
    {
      g_select_fd_map.modinf[i].comm_fd =
        (i32 *) select_alloc (sizeof (i32) * NSTACK_SELECT_MAX_FD);
      if (NULL == g_select_fd_map.modinf[i].comm_fd)
        {
          goto err_return;
        }
    }

  for (i = 0; i < NSTACK_SELECT_MAX_FD; i++)
    {
      reset_select_fdinf (i);
    }

  for (inx = 0; inx < get_mode_num (); inx++)
    {
      for (i = 0; i < NSTACK_SELECT_MAX_FD; i++)
        {
          select_set_commfd (i, inx, -1);

        }
    }

  ret = TRUE;
  return ret;
err_return:

  select_free ((char *) g_select_fd_map.fdinf);
  for (i = 0; i < get_mode_num (); i++)
    {
      select_free ((char *) g_select_fd_map.modinf[i].comm_fd);
    }

  return ret;
}
