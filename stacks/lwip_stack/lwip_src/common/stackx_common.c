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

#include <stdarg.h>
#include <stddef.h>
#include "stackx_common.h"
#include "nstack_securec.h"
#include "nstack_log.h"
#include "types.h"
#include "stackx_types.h"

int
spl_snprintf (char *buffer, int buflen, const char *format, ...)
{
  int len;
  va_list ap;

  if ((NULL == buffer) || (0 >= buflen))
    {
      return -1;
    }

  if (format == NULL)
    {
      buffer[0] = '\0';
      return -1;
    }

  (void) va_start (ap, format);
  len = VSNPRINTF_S (buffer, buflen, buflen - 1, format, ap);
  if (-1 == len)
    {
      va_end (ap);
      return -1;
    }

  va_end (ap);
  if ((len >= buflen) && (buflen > 0) && (buffer != NULL))
    {
      buffer[buflen - 1] = '\0';
    }

  return len;
}

/*****************************************************************************
*   Prototype    : sbr_create_mzone
*   Description  : create mzone
*   Input        : const char* name
*                  size_t size
*   Output       : None
*   Return Value : mzone_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mzone_handle
sbr_create_mzone (const char *name, size_t size)
{
  if (!name)
    {
      NSFW_LOGERR ("name is NULL");
      return NULL;
    }

  mzone_handle zone;
  nsfw_mem_zone param;

  param.isocket_id = -1;
  param.length = size;
  param.stname.entype = NSFW_SHMEM;

  if (STRCPY_S (param.stname.aname, NSFW_MEM_NAME_LENTH, name) != 0)
    {
      NSFW_LOGERR ("STRCPY_S failed]name=%s", name);
      return NULL;
    }

  zone = nsfw_mem_zone_create (&param);
  if (!zone)
    {
      NSFW_LOGERR ("nsfw_mem_zone_create failed]name=%s, size:%zu", name,
                   size);
      return NULL;
    }

  return zone;
}

/*****************************************************************************
*   Prototype    : sbr_lookup_mzone
*   Description  : lookup mzone
*   Input        : const char* name
*   Output       : None
*   Return Value : mzone_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mzone_handle
sbr_lookup_mzone (const char *name)
{
  if (!name)
    {
      NSFW_LOGERR ("name is NULL");
      return NULL;
    }

  mzone_handle zone;
  nsfw_mem_name param;

  param.entype = NSFW_SHMEM;
  param.enowner = NSFW_PROC_MAIN;
  if (STRCPY_S (param.aname, NSFW_MEM_NAME_LENTH, name) != 0)
    {
      NSFW_LOGERR ("STRCPY_S failed]name=%s", name);
      return NULL;
    }

  zone = nsfw_mem_zone_lookup (&param);
  if (!zone)
    {
      NSFW_LOGERR ("nsfw_mem_zone_lookup failed]name=%s", name);
      return NULL;
    }

  return zone;
}

/*****************************************************************************
*   Prototype    : sbr_create_pool
*   Description  : create pool
*   Input        : const char* name
*                  i32 num
*                  u16 size
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mring_handle
sbr_create_pool (const char *name, i32 num, u16 size)
{
  if (!name)
    {
      NSFW_LOGERR ("name is NULL");
      return NULL;
    }

  nsfw_mem_sppool param;
  if (EOK != MEMSET_S (&param, sizeof (param), 0, sizeof (param)))
    {
      NSFW_LOGERR ("memset error]name=%s", name);
      return NULL;
    }

  param.enmptype = NSFW_MRING_MPMC;
  param.useltsize = size;
  param.usnum = num;
  param.stname.entype = NSFW_SHMEM;
  param.isocket_id = -1;
  if (STRCPY_S (param.stname.aname, NSFW_MEM_NAME_LENTH, name) != 0)
    {
      NSFW_LOGERR ("STRCPY_S failed]name=%s", name);
      return NULL;
    }

  mring_handle ring = nsfw_mem_sp_create (&param);
  if (!ring)
    {
      NSFW_LOGERR ("Create pool failed]name=%s, num=%d, size=%u", name, num,
                   size);
    }

  return ring;
}

/*****************************************************************************
*   Prototype    : sbr_create_ring
*   Description  : create ring
*   Input        : const char* name
*                  i32 num
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mring_handle
sbr_create_ring (const char *name, i32 num)
{
  if (!name)
    {
      NSFW_LOGERR ("name is NULL");
      return NULL;
    }

  nsfw_mem_mring param;
  if (EOK != MEMSET_S (&param, sizeof (param), 0, sizeof (param)))
    {
      NSFW_LOGERR ("memset error]name=%s", name);
      return NULL;
    }

  param.enmptype = NSFW_MRING_MPMC;
  param.isocket_id = -1;
  param.usnum = num;
  param.stname.entype = NSFW_SHMEM;

  if (STRCPY_S (param.stname.aname, NSFW_MEM_NAME_LENTH, name) != 0)
    {
      NSFW_LOGERR ("STRCPY_S failed]name=%s", name);
      return NULL;
    }

  mring_handle ring = nsfw_mem_ring_create (&param);
  if (!ring)
    {
      NSFW_LOGERR ("Create ring failed]name=%s, num=%d", name, num);
    }

  return ring;
}

/*****************************************************************************
*   Prototype    : sbr_create_multi_ring
*   Description  : create multi ring
*   Input        : const char* name
*                  u32 ring_size
*                  i32 ring_num
*                  mring_handle* array
*                  nsfw_mpool_type type
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_create_multi_ring (const char *name, u32 ring_size, i32 ring_num,
                       mring_handle * array, nsfw_mpool_type type)
{
  if (!name)
    {
      NSFW_LOGERR ("name is NULL");
      return -1;
    }

  if (!array)
    {
      NSFW_LOGERR ("array is NULL");
      return -1;
    }

  nsfw_mem_mring param;

  if (EOK != MEMSET_S (&param, sizeof (param), 0, sizeof (param)))
    {
      NSSBR_LOGERR ("Error to memset]name=%s", name);
      return -1;
    }

  param.enmptype = type;
  param.stname.entype = NSFW_SHMEM;
  if (STRCPY_S (param.stname.aname, NSFW_MEM_NAME_LENTH, name) != 0)
    {
      NSSBR_LOGERR ("STRCPY_S failed]name=%s", name);
      return -1;
    }

  param.usnum = ring_size;
  param.isocket_id = -1;
  if (nsfw_mem_sp_ring_create (&param, array, ring_num) != 0)
    {
      NSSBR_LOGERR ("Create ring pool failed]name=%s, ring_num=%d", name,
                    ring_num);
      return -1;
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_lookup_ring
*   Description  : lookup ring
*   Input        : const char* name
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mring_handle
sbr_lookup_ring (const char *name)
{
  nsfw_mem_name param;

  param.entype = NSFW_SHMEM;
  param.enowner = NSFW_PROC_MAIN;
  if (STRCPY_S (param.aname, NSFW_MEM_NAME_LENTH, name) != 0)
    {
      NSFW_LOGERR ("STRCPY_S failed]name=%s", name);
      return NULL;
    }

  mring_handle ring = nsfw_mem_ring_lookup (&param);
  if (!ring)
    {
      NSFW_LOGERR ("lookup ring failed]name=%s", name);
    }

  return ring;
}

int
sbr_timeval2msec (struct timeval *pTime, u64 * msec)
{
  if ((pTime->tv_sec < 0) || (pTime->tv_usec < 0))
    {
      NSFW_LOGERR ("time->tv_sec is nagative");
      return -1;
    }

  if (STACKX_MAX_U64_NUM / 1000 < (u64_t) pTime->tv_sec)
    {
      NSFW_LOGERR ("tout.tv_sec is too large]tout.tv_sec=%lu", pTime->tv_sec);
      return -1;
    }

  u64 sec2msec = 1000 * pTime->tv_sec;
  u64 usec2msec = pTime->tv_usec / 1000;

  if (STACKX_MAX_U64_NUM - sec2msec < usec2msec)
    {
      NSFW_LOGERR
        ("nsec2msec plus sec2usec is too large]usec2msec=%lu,usec2msec=%lu",
         usec2msec, sec2msec);
      return -1;
    }

  *msec = sec2msec + usec2msec;
  return 0;
}
