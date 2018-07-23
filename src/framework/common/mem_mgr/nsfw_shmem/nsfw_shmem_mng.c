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

#include "nstack_securec.h"
#include "nstack_log.h"
#include "nsfw_mem_desc.h"
#include "nsfw_ring_fun.h"
#include "nsfw_shmem_ring.h"
#include "nsfw_shmem_mng.h"
#include "common_mem_mempool.h"
#include "common_mem_memzone.h"
#include "common_mem_buf.h"
#include "common_mem_mbuf.h"
#include "nsfw_rshmem_mng.h"
#include "common_mem_api.h"
#include "common_sys_config.h"
#include "nsfw_maintain_api.h"
#include "common_pal_bitwide_adjust.h"

#include "common_mem_pal.h"

#include "common_func.h"

#define NSFW_SHMEM_PID    (get_sys_pid())
#define NSFW_SHMEM_FLAG   (g_shmem_localdata->enflag)

extern u8 app_mode;
nsfw_mem_localdata *g_shmem_localdata = NULL;

/*check g_mem_localdata*/
#define NSFW_INIT_CHK_RET() \
    if (!g_shmem_localdata) \
    { \
        return NSFW_MEM_ERR; \
    }

#define NSFW_INIT_CHK_RET_NULL() \
    if (!g_shmem_localdata) \
    { \
        return NULL; \
    }

/*
 *share memory mng module init
 *
 */
i32
nsfw_shmem_init (nsfw_mem_para * para)
{
  common_mem_pal_module_info rteinfo = { 0 };
  i32 iret = NSFW_MEM_ERR;
  int flag = 0;
  if (!para)
    {
      return NSFW_MEM_ERR;
    }

  NSCOMM_LOGINF ("nsfw shmem init begin");

  if (NSFW_PROC_MASTER == para->enflag)
    {
      iret = common_mem_pal_init (para->iargsnum, para->pargs);
    }
  else if (NSFW_PROC_MAIN == para->enflag)
    {
      iret = common_pal_module_init (NULL, app_mode);
    }
  else
    {
      LCORE_MASK_SET (rteinfo.ilcoremask, 1);
      rteinfo.ucproctype = DMM_PROC_T_SECONDARY;
      iret = common_pal_module_init (&rteinfo, app_mode);
    }

  if (NSFW_MEM_OK != iret)
    {
      NSCOMM_LOGERR ("rte init fail] ret=0x%x", iret);
      return NSFW_MEM_ERR;
    }

  flag = dmm_pal_addr_align ();
  if ((0 == flag) && (NSFW_PROC_MAIN == para->enflag))
    {
      dmm_addr_print ();
      NSCOMM_LOGERR
        ("rte init addr is not the same with primary] nstackmain flag=%d",
         flag);
      return NSFW_MEM_ERR;
    }

  g_shmem_localdata =
    (nsfw_mem_localdata *) malloc (sizeof (nsfw_mem_localdata));

  if (NULL == g_shmem_localdata)
    {
      NSCOMM_LOGERR ("g_shmem_localdata malloc fail");
      return NSFW_MEM_ERR;
    }

  iret =
    MEMSET_S (g_shmem_localdata, sizeof (nsfw_mem_localdata), 0,
              sizeof (nsfw_mem_localdata));
  if (EOK != iret)
    {
      NSCOMM_LOGERR ("memset fail] g_shmem_localdata=%p ", g_shmem_localdata);
      free (g_shmem_localdata);
      g_shmem_localdata = NULL;
      return NSFW_MEM_ERR;
    }

  g_shmem_localdata->enflag = para->enflag;

  NSCOMM_LOGINF ("nsfw shmem init end] enflag=%d", para->enflag);
  return NSFW_MEM_OK;

}

/*
 *module destroy
 */
void
nsfw_shmem_destroy (void)
{
  if (g_shmem_localdata)
    {
      free (g_shmem_localdata);
      g_shmem_localdata = NULL;
    }

  return;
}

/*
 * create a shared memory
 * nsfw_mem_zone::stname memory name
 * nsfw_mem_zone::isize
 */
mzone_handle
nsfw_shmem_create (nsfw_mem_zone * pinfo)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };

  NSFW_INIT_CHK_RET_NULL ()if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return common_memzone_data_reserve_name (pinfo->stname.aname,
                                               pinfo->length,
                                               pinfo->isocket_id);
    }
  else
    {
      /*app must less than NSFW_MEM_APPNAME_LENGTH */
      NSFW_NAME_LENCHECK_RET_NULL (pinfo->stname.aname, "shmem create")
        if (-1 ==
            SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x",
                       pinfo->stname.aname, NSFW_SHMEM_PID))
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]");
          return NULL;
        }
    }

  return nsfw_memzone_remote_reserv ((char *) &aname[0], pinfo->length,
                                     SOCKET_ID_ANY);
}

/*
 *create some memory
 *inum must be equal iarray_num
 */
i32
nsfw_shmem_createv (nsfw_mem_zone * pmeminfo, i32 inum,
                    mzone_handle * paddr_array, i32 iarray_num)
{
  NSFW_INIT_CHK_RET ();

  if (NSFW_PROC_MASTER == NSFW_SHMEM_FLAG)
    {
      return NSFW_MEM_ERR;
    }
  else if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return nsfw_memzone_remote_reserv_v (pmeminfo, paddr_array, iarray_num,
                                           0);
    }
  else
    {
      return nsfw_memzone_remote_reserv_v (pmeminfo, paddr_array, iarray_num,
                                           NSFW_SHMEM_PID);
    }
  return NSFW_MEM_ERR;
}

mzone_handle
nsfw_shmem_lookup (nsfw_mem_name * pname)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };
  NSFW_INIT_CHK_RET_NULL ();

  if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return common_memzone_data_lookup_name (pname->aname);
    }

  if ((NSFW_PROC_NULL == pname->enowner)
      || (NSFW_PROC_MAIN == pname->enowner))
    {
      int retVal =
        SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s", pname->aname);
      if (-1 == retVal)
        {
          NSCOMM_LOGERR ("SPRINTF_S failed");
          return NULL;
        }
    }
  else
    {
      /*app must less than NSFW_MEM_APPNAME_LENGTH */
      NSFW_NAME_LENCHECK_RET_NULL (pname->aname, "shmem lookup")
        int retVal =
        SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x", pname->aname,
                   NSFW_SHMEM_PID);
      if (-1 == retVal)
        {
          NSCOMM_LOGERR ("SPRINTF_S failed");
          return NULL;
        }
    }

  return nsfw_remote_shmem_lookup (aname, NSFW_MEM_MZONE);
}

i32
nsfw_shmem_release (nsfw_mem_name * pname)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };
  const struct common_mem_memzone *pmzone = NULL;
  NSFW_INIT_CHK_RET ();

  if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      pmzone = common_mem_memzone_lookup (pname->aname);

      if (pmzone)
        {
          common_mem_memzone_free (pmzone);
        }
      return NSFW_MEM_OK;
    }
  else
    {
      NSFW_NAME_LENCHECK_RET (pname->aname, "shmem free")
        if (-1 ==
            SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x",
                       pname->aname, NSFW_SHMEM_PID))
        {
          NSCOMM_LOGERR ("SPRINTF_S failed");
          return NSFW_MEM_ERR;
        }
    }

  return nsfw_remote_free (aname, NSFW_MEM_MZONE);
}

mpool_handle
nsfw_shmem_mbfmpcreate (nsfw_mem_mbfpool * pbufinfo)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };

  NSFW_INIT_CHK_RET_NULL ();

  if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return common_mem_pktmbuf_pool_create (pbufinfo->stname.aname,
                                             pbufinfo->usnum,
                                             pbufinfo->uscash_size,
                                             pbufinfo->uspriv_size,
                                             pbufinfo->usdata_room,
                                             pbufinfo->isocket_id);
    }
  else
    {
      /*app must less than NSFW_MEM_APPNAME_LENGTH */
      NSFW_NAME_LENCHECK_RET_NULL (pbufinfo->stname.aname, "mbufpool create")
        if (-1 ==
            SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x",
                       pbufinfo->stname.aname, NSFW_SHMEM_PID))
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]");
        }
    }

  return nsfw_remote_shmem_mbf_create (aname, pbufinfo->usnum,
                                       pbufinfo->uscash_size,
                                       pbufinfo->uspriv_size,
                                       pbufinfo->usdata_room, SOCKET_ID_ANY,
                                       pbufinfo->enmptype);
}

/*
 *create some mbuf pools
 */
i32
nsfw_shmem_mbfmpcreatev (nsfw_mem_mbfpool * pmbfname, i32 inum,
                         mpool_handle * phandle_array, i32 iarray_num)
{
  NSFW_INIT_CHK_RET ();

  if (NSFW_PROC_MASTER == NSFW_SHMEM_FLAG)
    {
      return NSFW_MEM_ERR;
    }
  else if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return nsfw_remote_shmem_mbf_createv (pmbfname, phandle_array,
                                            iarray_num, 0);
    }
  else
    {
      return nsfw_remote_shmem_mbf_createv (pmbfname, phandle_array,
                                            iarray_num, NSFW_SHMEM_PID);
    }

  return NSFW_MEM_ERR;
}

mbuf_handle
nsfw_shmem_mbfalloc (mpool_handle mhandle)
{
  return (mbuf_handle) common_mem_pktmbuf_alloc ((struct common_mem_mempool *)
                                                 mhandle);
}

i32
nsfw_shmem_mbffree (mbuf_handle mhandle)
{
  common_mem_pktmbuf_free ((struct common_mem_mbuf *) mhandle);
  return NSFW_MEM_OK;
}

i32
nsfw_shmem_mbfmprelease (nsfw_mem_name * pname)
{
  return NSFW_MEM_OK;
}

mpool_handle
nsfw_shmem_mbfmplookup (nsfw_mem_name * pmbfname)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };

  NSFW_INIT_CHK_RET_NULL ();

  if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return common_mem_mempool_lookup (pmbfname->aname);
    }

  if ((NSFW_PROC_NULL == pmbfname->enowner)
      || (NSFW_PROC_MAIN == pmbfname->enowner))
    {
      if (-1 ==
          SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s",
                     pmbfname->aname))
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]");
        }
    }
  else
    {
      /*app must less than NSFW_MEM_APPNAME_LENGTH */
      NSFW_NAME_LENCHECK_RET_NULL (pmbfname->aname, "shmem lookup")
        if (-1 ==
            SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x",
                       pmbfname->aname, NSFW_SHMEM_PID))
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]");
        }
    }

  return nsfw_remote_shmem_lookup (aname, NSFW_MEM_MBUF);
}

mring_handle
nsfw_shmem_spcreate (nsfw_mem_sppool * pmpinfo)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };

  NSFW_INIT_CHK_RET_NULL ();

  if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return nsfw_shmem_pool_create (pmpinfo->stname.aname, pmpinfo->usnum,
                                     pmpinfo->useltsize, pmpinfo->isocket_id,
                                     pmpinfo->enmptype);
    }
  else
    {
      NSFW_NAME_LENCHECK_RET_NULL (pmpinfo->stname.aname, "mpool create")
        if (-1 ==
            SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x",
                       pmpinfo->stname.aname, NSFW_SHMEM_PID))
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]");
        }
    }

  return nsfw_remote_shmem_mpcreate (aname, pmpinfo->usnum,
                                     pmpinfo->useltsize, SOCKET_ID_ANY,
                                     pmpinfo->enmptype);
}

i32
nsfw_shmem_spcreatev (nsfw_mem_sppool * pmpinfo, i32 inum,
                      mring_handle * pringhandle_array, i32 iarray_num)
{
  NSFW_INIT_CHK_RET ();

  if (NSFW_PROC_MASTER == NSFW_SHMEM_FLAG)
    {
      return NSFW_MEM_ERR;
    }
  else if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return nsfw_remote_shmem_mpcreatev (pmpinfo, pringhandle_array, inum,
                                          0);
    }
  else
    {
      return nsfw_remote_shmem_mpcreatev (pmpinfo, pringhandle_array, inum,
                                          NSFW_SHMEM_PID);
    }
  return NSFW_MEM_ERR;
}

i32
nsfw_lshmem_ringcreatev (const char *name, i32 ieltnum,
                         mring_handle * pringhandle_array, i32 iringnum,
                         i32 socket_id, nsfw_mpool_type entype)
{
  i32 useltsize = 0;
  mring_handle nhandle = NULL;
  i32 icount = 0;
  i32 n = 0;
  uint64_t baseaddr = 0;
  uint64_t endaddr = 0;
  i32 usnum = common_mem_align32pow2 (ieltnum + 1);

  useltsize =
    sizeof (struct nsfw_mem_ring) + usnum * sizeof (union RingData_U);
  nhandle =
    nsfw_shmem_pool_create (name, iringnum, useltsize, socket_id,
                            NSFW_MRING_SPSC);
  if (NULL == (nhandle))
    {
      return NSFW_MEM_ERR;
    }

  n =
    nsfw_shmem_ring_sc_dequeuev (nhandle, (void **) pringhandle_array,
                                 iringnum);

  if (n != iringnum)
    {
      NSCOMM_LOGERR
        ("ring dequeuev failed] ring=%p, dequeue num=%d, expect num=%d",
         nhandle, n, iringnum);
      return NSFW_MEM_ERR;
    }

  nsfw_shmem_ring_baseaddr_query (&baseaddr, &endaddr);

  for (icount = 0; icount < iringnum; icount++)
    {
      nsfw_mem_ring_init (pringhandle_array[icount], usnum, (void *) baseaddr,
                          NSFW_SHMEM, entype);
    }

  return NSFW_MEM_OK;
}

i32
nswf_shmem_sp_ringcreate (nsfw_mem_mring * prpoolinfo,
                          mring_handle * pringhandle_array, i32 iringnum)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };

  NSFW_INIT_CHK_RET ();

  if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return nsfw_lshmem_ringcreatev (prpoolinfo->stname.aname,
                                      prpoolinfo->usnum, pringhandle_array,
                                      iringnum, SOCKET_ID_ANY,
                                      prpoolinfo->enmptype);
    }
  else
    {
      NSFW_NAME_LENCHECK_RET (prpoolinfo->stname.aname, "ring pool")
        int retVal = SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x",
                                prpoolinfo->stname.aname, NSFW_SHMEM_PID);
      if (-1 == retVal)
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]");
        }
    }

  return nsfw_remote_shmem_ringcreatev (aname, prpoolinfo->usnum,
                                        pringhandle_array, iringnum,
                                        SOCKET_ID_ANY, prpoolinfo->enmptype);
}

i32
nsfw_shmem_sprelease (nsfw_mem_name * pname)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };
  void *mz_mem = NULL;
  struct nsfw_mem_ring *ring_ptr = NULL;
  NSFW_INIT_CHK_RET ();

  if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      mz_mem = common_memzone_data_lookup_name (pname->aname);

      if (mz_mem)
        {
          ring_ptr =
            (struct nsfw_mem_ring *) ((char *) mz_mem +
                                      sizeof (struct nsfw_shmem_ring_head));
          nsfw_shmem_pool_free (ring_ptr);
        }
      return NSFW_MEM_OK;
    }
  else
    {
      NSFW_NAME_LENCHECK_RET (pname->aname, "shmem free")
        if (-1 ==
            SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x",
                       pname->aname, NSFW_SHMEM_PID))
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]");
        }
    }

  return nsfw_remote_free (aname, NSFW_MEM_SPOOL);
}

mring_handle
nsfw_shmem_sp_lookup (nsfw_mem_name * pname)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };
  void *mz_mem = NULL;
  struct nsfw_mem_ring *ring_ptr = NULL;
  NSFW_INIT_CHK_RET_NULL ();

  if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      mz_mem = common_memzone_data_lookup_name (pname->aname);

      if (mz_mem)
        {
          ring_ptr =
            (struct nsfw_mem_ring *) ((char *) mz_mem +
                                      sizeof (struct nsfw_shmem_ring_head));
          return ring_ptr;
        }
      return mz_mem;
    }

  if ((NSFW_PROC_NULL == pname->enowner)
      || (NSFW_PROC_MAIN == pname->enowner))
    {
      if (-1 ==
          SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s", pname->aname))
        {
          NSCOMM_LOGERR ("SPRINTF_S fails]");
        }
    }
  else
    {
      /*app's name can not over NSFW_MEM_APPNAME_LENGTH */
      NSFW_NAME_LENCHECK_RET_NULL (pname->aname, "shmem lookup")
        if (-1 ==
            SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x",
                       pname->aname, NSFW_SHMEM_PID))
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]");
        }
    }

  return nsfw_remote_shmem_lookup (aname, NSFW_MEM_SPOOL);
}

mring_handle
nsfw_shmem_ringcreate (nsfw_mem_mring * pringinfo)
{
  i8 aname[COMMON_MEM_MEMPOOL_NAMESIZE] = { 0 };

  NSFW_INIT_CHK_RET_NULL ();

  if (NSFW_PROC_MAIN == NSFW_SHMEM_FLAG)
    {
      return nsfw_shmem_ring_create (pringinfo->stname.aname,
                                     pringinfo->usnum, pringinfo->isocket_id,
                                     pringinfo->enmptype);
    }
  else
    {
      NSFW_NAME_LENCHECK_RET_NULL (pringinfo->stname.aname, "ring create")
        if (-1 ==
            SPRINTF_S (aname, COMMON_MEM_MEMPOOL_NAMESIZE, "%s_%x",
                       pringinfo->stname.aname, NSFW_SHMEM_PID))
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]");
        }
    }

  return nsfw_remote_shmem_ringcreate (aname, pringinfo->usnum, SOCKET_ID_ANY,
                                       pringinfo->enmptype);
}

mring_handle
nsfw_shmem_ring_lookup (nsfw_mem_name * pname)
{
  return nsfw_shmem_lookup (pname);
}

i32
nsfw_shmem_ringrelease (nsfw_mem_name * pname)
{
  return nsfw_shmem_release (pname);
}

size_t
nsfw_shmem_mbufpool_statics (mpool_handle mbufpool)
{
  struct common_mem_mempool *mp = (struct common_mem_mempool *) mbufpool;
  return (size_t) mp->size * (mp->header_size + mp->elt_size +
                              mp->trailer_size) +
    (size_t) mp->private_data_size +
    (size_t)
    common_mem_ring_get_memsize (common_mem_align32pow2 (mp->size + 1));
}

size_t
nsfw_shmem_sppool_statics (mring_handle sppool)
{
  struct nsfw_shmem_ring_head *temp = NULL;
  size_t lent = 0;
  temp =
    (struct nsfw_shmem_ring_head *) ((char *) sppool -
                                     sizeof (struct nsfw_shmem_ring_head));

  while (temp)
    {
      lent += temp->mem_zone->len;
      temp = temp->next;
    }

  return lent;
}

size_t
nsfw_shmem_ring_statics (mring_handle handle)
{
  struct nsfw_mem_ring *ring = (struct nsfw_mem_ring *) handle;
  return ring->size * sizeof (union RingData_U) +
    sizeof (struct nsfw_mem_ring);
}

ssize_t
nsfw_shmem_static (void *handle, nsfw_mem_struct_type type)
{
  switch (type)
    {
    case NSFW_MEM_MBUF:
      return nsfw_shmem_mbufpool_statics (handle);
    case NSFW_MEM_SPOOL:
      return nsfw_shmem_sppool_statics (handle);
    case NSFW_MEM_RING:
      return nsfw_shmem_ring_statics (handle);
    default:
      break;
    }
  return -1;
}

i32
nsfw_shmem_mbuf_recycle (mpool_handle handle)
{
  return NSFW_MEM_OK;
}

/*****************************************************************************
*   Prototype    : nsfw_shmem_sp_iterator
*   Description  : sp pool iterator
*   Input        : mpool_handle handle
*                  u32 start
*                  u32 end
*                  nsfw_mem_item_fun fun
*                  void *argv
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_shmem_sp_iterator (mpool_handle handle, u32 start, u32 end,
                        nsfw_mem_item_fun fun, void *argv)
{
  struct nsfw_mem_ring *perfring_ptr = (struct nsfw_mem_ring *) handle;
  if (NULL == perfring_ptr || NULL == fun)
    {
      return 0;
    }

  if (0 == perfring_ptr->eltsize)
    {
      return 0;
    }

  int num = perfring_ptr->size;
  if (start >= (u32) num || end <= start)
    {
      return 0;
    }

  struct nsfw_shmem_ring_head *ring_head =
    (struct nsfw_shmem_ring_head *) ((char *) handle -
                                     sizeof (struct nsfw_shmem_ring_head));
  void *mz =
    (void *) ((char *) perfring_ptr + sizeof (struct nsfw_mem_ring) +
              num * sizeof (union RingData_U));

  if (ring_head->mem_zone->len <
      sizeof (struct nsfw_shmem_ring_head) + sizeof (struct nsfw_mem_ring) +
      num * sizeof (union RingData_U))
    {
      return 0;
    }

  u32 mz_len =
    ring_head->mem_zone->len - sizeof (struct nsfw_shmem_ring_head) -
    sizeof (struct nsfw_mem_ring) - num * sizeof (union RingData_U);

  u32 start_idx = 0;
  u32 elm_num = 0;
  elm_num = mz_len / perfring_ptr->eltsize;
  while (start > start_idx + elm_num)
    {
      if (NULL == ring_head->next || NULL == ring_head->next->mem_zone
          || 0 == elm_num)
        {
          return 0;
        }

      ring_head =
        (struct nsfw_shmem_ring_head *) ring_head->next->mem_zone->addr_64;
      mz_len =
        ring_head->mem_zone->len - sizeof (struct nsfw_shmem_ring_head);

      elm_num = mz_len / perfring_ptr->eltsize;
      mz =
        (void *) ((char *) ring_head + sizeof (struct nsfw_shmem_ring_head));
      start_idx += elm_num;
    }

  u32 cur_idx = start - start_idx;
  char *cur_elm = NULL;
  int proc_count = 0;
  while (cur_idx + start_idx < end && cur_idx + start_idx < (u32) num)
    {
      if (cur_idx >= elm_num)
        {
          if (NULL == ring_head->next || NULL == ring_head->next->mem_zone
              || 0 == elm_num)
            {
              break;
            }

          ring_head =
            (struct nsfw_shmem_ring_head *) ring_head->next->
            mem_zone->addr_64;
          mz_len =
            ring_head->mem_zone->len - sizeof (struct nsfw_shmem_ring_head);

          elm_num = mz_len / perfring_ptr->eltsize;
          mz =
            (void *) ((char *) ring_head +
                      sizeof (struct nsfw_shmem_ring_head));
          start_idx += elm_num;

          cur_idx = 0;
          cur_elm = NULL;
          continue;
        }

      if (NULL == cur_elm)
        {
          cur_elm = ((char *) mz + cur_idx * perfring_ptr->eltsize);
        }
      else
        {
          cur_elm += perfring_ptr->eltsize;
        }

      cur_idx++;
      proc_count++;
      (void) fun (cur_elm, argv);
    }

  return proc_count;
}

i32
nsfw_shmem_mbuf_iterator (mpool_handle handle, u32 start, u32 end,
                          nsfw_mem_item_fun fun, void *argv)
{
  return dmm_pktmbuf_pool_iterator ((struct common_mem_mempool *) handle,
                                    start, end, (dmm_mbuf_item_fun) fun,
                                    argv);
}
