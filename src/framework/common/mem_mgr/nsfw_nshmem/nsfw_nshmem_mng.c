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

#include <stdlib.h>
#include "nstack_log.h"
#include "nstack_securec.h"
#include "nsfw_mem_desc.h"
#include "nsfw_ring_fun.h"
#include "nsfw_nshmem_ring.h"
#include "nsfw_nshmem_mng.h"

#include "common_func.h"

#define nsfw_get_glb_lock()      (&g_nshmem_internal_cfg->mlock)

#define NSFW_NSHMEM_INIT_CHK_RET_NULL()  \
    if ((!g_nshmem_internal_cfg) || (!g_nshmem_localdata)) \
    { \
        NSCOMM_LOGDBG("Error] g_nshmem_internal_cfg=%p, g_nshmem_localdata=%p", g_nshmem_internal_cfg, g_nshmem_localdata); \
        return NULL;  \
    }

#define NSFW_NSHMEM_INIT_CHK_RET()  \
    if ((!g_nshmem_internal_cfg) || (!g_nshmem_localdata)) \
    { \
        NSCOMM_LOGDBG("Error] g_nshmem_internal_cfg=%p, g_nshmem_localdata=%p", g_nshmem_internal_cfg, g_nshmem_localdata); \
        return NSFW_MEM_ERR;  \
    }

nsfw_mem_localdata *g_nshmem_localdata = NULL;
nsfw_nshmem_cfg *g_nshmem_internal_cfg = NULL;

/*look up a mem zone*/
NSTACK_STATIC inline nsfw_nshmem_mzone *
nsfw_nshmem_get_free_zone (void)
{
  int icnt = 0;

  /*g_nshmem_internal_cfg must not be null if come here */
  for (icnt = 0; icnt < COMMON_MEM_MAX_MEMZONE; icnt++)
    {
      if (g_nshmem_internal_cfg->amemzone[icnt].addr == NULL)
        {
          return &g_nshmem_internal_cfg->amemzone[icnt];
        }
    }

  return NULL;
}

NSTACK_STATIC inline void
nsfw_nshmem_free_zone (nsfw_nshmem_mzone * pzone)
{
  nsfw_nshmem_mzone *pzonebase = &g_nshmem_internal_cfg->amemzone[0];
  nsfw_nshmem_mzone *pzoneend =
    &g_nshmem_internal_cfg->amemzone[NSFW_NSHMEM_ZONE_MAX - 1];

  if ((((int) ((char *) pzone - (char *) pzonebase) < 0)
       || ((int) ((char *) pzone - (char *) pzoneend) > 0))
      && ((unsigned int) ((char *) pzone - (char *) pzonebase) %
          sizeof (nsfw_nshmem_mzone) != 0))
    {
      NSCOMM_LOGERR ("nshmem free fail] mem=%p", pzone);
      return;
    }
  if (pzone->addr)
    {
      free (pzone->addr);
    }
  pzone->addr = NULL;

  int ret = MEMSET_S ((void *) pzone, sizeof (nsfw_nshmem_mzone), 0,
                      sizeof (nsfw_nshmem_mzone));
  if (EOK != ret)
    {
      NSCOMM_LOGERR ("MEMSET_S failed] mem=%p, ret=%d", pzone, ret);
    }
  return;
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_init
*   Description  : nsh module init
*   Input        : nsfw_mem_para* para
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_nshmem_init (nsfw_mem_para * para)
{
  i32 iret = NSFW_MEM_OK;
  NSCOMM_LOGINF ("nsfw nshmem init begin");
  g_nshmem_localdata =
    (nsfw_mem_localdata *) malloc (sizeof (nsfw_mem_localdata));

  if (NULL == g_nshmem_localdata)
    {
      NSCOMM_LOGERR ("nshmem init g_nshmem_localdata malloc fail");
      return NSFW_MEM_ERR;
    }

  iret =
    MEMSET_S (g_nshmem_localdata, sizeof (nsfw_mem_localdata), 0,
              sizeof (nsfw_mem_localdata));

  if (EOK != iret)
    {
      NSCOMM_LOGERR ("nshmem init g_nshmem_localdata MEMSET_S fail");
      goto ERROR;
    }

  g_nshmem_internal_cfg =
    (nsfw_nshmem_cfg *) malloc (sizeof (nsfw_nshmem_cfg));

  if (NULL == g_nshmem_internal_cfg)
    {
      NSCOMM_LOGERR ("nshmem init g_nshmem_internal_cfg malloc fail");
      goto ERROR;
    }

  iret =
    MEMSET_S (g_nshmem_internal_cfg, sizeof (nsfw_nshmem_cfg), 0,
              sizeof (nsfw_nshmem_cfg));

  if (EOK != iret)
    {
      NSCOMM_LOGERR ("nshmem init g_nshmem_internal_cfg MEMSET_S fail");
      goto ERROR;
    }

  g_nshmem_localdata->enflag = para->enflag;
  NSCOMM_LOGINF ("nsfw nshmem init end");
  goto OK;

ERROR:
  iret = NSFW_MEM_ERR;
  nsfw_nshmem_destory ();
  return iret;
OK:
  iret = NSFW_MEM_OK;
  return iret;
}

/*
 * memory destory
 */
void
nsfw_nshmem_destory (void)
{
  if (g_nshmem_localdata)
    {
      free (g_nshmem_localdata);
      g_nshmem_localdata = NULL;
    }

  if (g_nshmem_internal_cfg)
    {
      free (g_nshmem_internal_cfg);
      g_nshmem_internal_cfg = NULL;
    }

  return;
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_reserv_safe
*   Description  : malloc a memory and save to memzone
*   Input        : const char* name
*                  size_t length
*   Output       : None
*   Return Value : mzone_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mzone_handle
nsfw_nshmem_reserv_safe (const char *name, size_t length)
{
  void *addr = NULL;
  i32 iret = NSFW_MEM_OK;
  nsfw_nshmem_mzone *pmemzone = NULL;

  if (length <= 0)
    {
      return NULL;
    }

  nsfw_write_lock (nsfw_get_glb_lock ());

  addr = malloc (length);
  if (!addr)
    {
      NSCOMM_LOGERR ("nshmem malloc addr fail] addr=%p", addr);
      nsfw_write_unlock (nsfw_get_glb_lock ());
      return NULL;
    }

  iret = MEMSET_S (addr, length, 0, length);
  if (EOK != iret)
    {
      NSCOMM_LOGERR ("nshmem malloc addr MEMSET_S fail] addr=%p", addr);
      free (addr);
      nsfw_write_unlock (nsfw_get_glb_lock ());
      return NULL;
    }

  pmemzone = nsfw_nshmem_get_free_zone ();

  if (!pmemzone)
    {
      NSCOMM_LOGERR ("nshmem get free zone fail");
      free (addr);
      nsfw_write_unlock (nsfw_get_glb_lock ());
      return NULL;
    }

  pmemzone->addr = addr;
  pmemzone->length = length;
  /*name must be less than NSFW_MEM_APPNAME_LENGTH */
  if (EOK !=
      STRCPY_S ((char *) pmemzone->aname, sizeof (pmemzone->aname), name))
    {
      NSCOMM_LOGERR ("STRCPY_S failed]name=%s", name);
      free (addr);
      nsfw_write_unlock (nsfw_get_glb_lock ());
      return NULL;
    }

  nsfw_write_unlock (nsfw_get_glb_lock ());
  return addr;
}

/*
 * create no shared memory
 * nsfw_mem_zone::stname no shared memory name
 * nsfw_mem_zone::isize memory size
 */
mzone_handle
nsfw_nshmem_create (nsfw_mem_zone * pinfo)
{

  NSFW_NAME_LENCHECK_RET_NULL (pinfo->stname.aname, "nshmem create");
  NSFW_NSHMEM_INIT_CHK_RET_NULL ();
  return nsfw_nshmem_reserv_safe (pinfo->stname.aname, pinfo->length);
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_lookup
*   Description  : find a block memory by name
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : mzone_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mzone_handle
nsfw_nshmem_lookup (nsfw_mem_name * pname)
{
  int icnt = 0;
  nsfw_nshmem_mzone *mz = NULL;

  NSFW_NAME_LENCHECK_RET_NULL (pname->aname, "nshmem lookup");
  NSFW_NSHMEM_INIT_CHK_RET_NULL ();
  nsfw_read_lock (nsfw_get_glb_lock ());

  for (icnt = 0; icnt < NSFW_NSHMEM_ZONE_MAX; icnt++)
    {
      mz = &g_nshmem_internal_cfg->amemzone[icnt];

      if (mz->addr != NULL
          && !strncmp (pname->aname, mz->aname, NSFW_MEM_NAME_LENGTH))
        {
          nsfw_read_unlock (nsfw_get_glb_lock ());
          return mz->addr;
        }
    }

  nsfw_read_unlock (nsfw_get_glb_lock ());
  return NULL;
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_release
*   Description  : free a block memory
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_nshmem_release (nsfw_mem_name * pname)
{
  int icnt = 0;
  nsfw_nshmem_mzone *mz = NULL;

  NSFW_NAME_LENCHECK_RET (pname->aname, "nshmem release");
  NSFW_NSHMEM_INIT_CHK_RET ();
  nsfw_read_lock (nsfw_get_glb_lock ());

  for (icnt = 0; icnt < NSFW_NSHMEM_ZONE_MAX; icnt++)
    {
      mz = &g_nshmem_internal_cfg->amemzone[icnt];

      if (mz->addr != NULL
          && !strncmp (pname->aname, mz->aname, NSFW_MEM_NAME_LENGTH))
        {
          nsfw_nshmem_free_zone (mz);
          nsfw_read_unlock (nsfw_get_glb_lock ());
          return NSFW_MEM_OK;
        }
    }

  nsfw_read_unlock (nsfw_get_glb_lock ());
  return NSFW_MEM_OK;

}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_spcreate
*   Description  : create a memory pool by ring
*   Input        : nsfw_mem_sppool* pmpinfo
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*****************************************************************************/
mring_handle
nsfw_nshmem_spcreate (nsfw_mem_sppool * pmpinfo)
{
  size_t len = 0;
  unsigned int usnum = common_mem_align32pow2 (pmpinfo->usnum + 1);
  unsigned int uselt_size = pmpinfo->useltsize;
  struct nsfw_mem_ring *pringhead = NULL;
  unsigned int uscnt = 0;
  char *pmz = NULL;
  NSFW_NAME_LENCHECK_RET_NULL (pmpinfo->stname.aname, "nshmem sp create");
  NSFW_NSHMEM_INIT_CHK_RET_NULL ();

  len =
    sizeof (struct nsfw_mem_ring) +
    (size_t) usnum *sizeof (union RingData_U) + (size_t) usnum *uselt_size;
  pringhead =
    (struct nsfw_mem_ring *) nsfw_nshmem_reserv_safe (pmpinfo->stname.aname,
                                                      len);

  if (!pringhead)
    {
      NSCOMM_LOGERR ("nshmem sp create mzone reserv fail");
      return NULL;
    }

  nsfw_mem_ring_init (pringhead, usnum, pringhead, NSFW_NSHMEM,
                      pmpinfo->enmptype);
  pmz =
    ((char *) pringhead + sizeof (struct nsfw_mem_ring) +
     usnum * sizeof (union RingData_U));

  for (uscnt = 0; uscnt < usnum; uscnt++)
    {
      if (0 ==
          g_ring_ops_arry[pringhead->memtype][pringhead->
                                              ringflag].ring_ops_enqueue
          (pringhead, (void *) pmz))
        {
          NSCOMM_LOGERR ("nsfw_nshmem_ringenqueue enqueue fail] uscnt=%u",
                         uscnt);
        }

      pmz = pmz + uselt_size;
    }

  return pringhead;
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_sp_lookup
*   Description  : look up a sppool memory
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*****************************************************************************/
mring_handle
nsfw_nshmem_sp_lookup (nsfw_mem_name * pname)
{
  return nsfw_nshmem_lookup (pname);
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_sprelease
*   Description  : release a sp pool
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_nshmem_sprelease (nsfw_mem_name * pname)
{
  NSFW_NAME_LENCHECK_RET (pname->aname, "nshmem sp mempool release");
  NSFW_NSHMEM_INIT_CHK_RET ();
  return nsfw_nshmem_release (pname);
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_ringcreate
*   Description  : create a ring
*   Input        : nsfw_mem_mring* pringinfo
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mring_handle
nsfw_nshmem_ringcreate (nsfw_mem_mring * pringinfo)
{
  size_t len = 0;
  unsigned int usnum = common_mem_align32pow2 (pringinfo->usnum + 1);
  struct nsfw_mem_ring *pringhead = NULL;
  NSFW_NAME_LENCHECK_RET_NULL (pringinfo->stname.aname, "nshmem ring create");
  NSFW_NSHMEM_INIT_CHK_RET_NULL ();

  len = sizeof (struct nsfw_mem_ring) + usnum * sizeof (union RingData_U);
  pringhead =
    (struct nsfw_mem_ring *) nsfw_nshmem_reserv_safe (pringinfo->stname.aname,
                                                      len);

  if (!pringhead)
    {
      NSCOMM_LOGERR ("nshmem ring create mzone reserv fail");
      return NULL;
    }

  nsfw_mem_ring_init (pringhead, usnum, (void *) pringhead, NSFW_NSHMEM,
                      pringinfo->enmptype);
  return pringhead;

}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_ringrelease
*   Description  : release a nsh ring memory
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_nshmem_ringrelease (nsfw_mem_name * pname)
{
  NSFW_NAME_LENCHECK_RET (pname->aname, "nshmem ring mempool release");
  NSFW_NSHMEM_INIT_CHK_RET ();
  return nsfw_nshmem_release (pname);
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_sppool_statics
*   Description  : static the memory size of sppool
*   Input        : mring_handle sppool
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t
nsfw_nshmem_sppool_statics (mring_handle sppool)
{
  struct nsfw_mem_ring *phead = (struct nsfw_mem_ring *) sppool;

  return sizeof (struct nsfw_mem_ring) +
    (ssize_t) phead->size * sizeof (union RingData_U) +
    (ssize_t) phead->size * phead->eltsize;
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_ring_statics
*   Description  : static the memory size of ring
*   Input        : mring_handle handle
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t
nsfw_nshmem_ring_statics (mring_handle handle)
{
  struct nsfw_mem_ring *ring = (struct nsfw_mem_ring *) handle;
  return ring->size * sizeof (union RingData_U) +
    sizeof (struct nsfw_mem_ring);
}

/*****************************************************************************
*   Prototype    : nsfw_nshmem_static
*   Description  : static the memory size according to mem type
*   Input        : void* handle
*                  nsfw_mem_struct_type type
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t
nsfw_nshmem_static (void *handle, nsfw_mem_struct_type type)
{
  switch (type)
    {
    case NSFW_MEM_MBUF:
      return -1;
    case NSFW_MEM_SPOOL:
      return nsfw_nshmem_sppool_statics (handle);
    case NSFW_MEM_RING:
      return nsfw_nshmem_ring_statics (handle);
    default:
      break;
    }
  return -1;
}
