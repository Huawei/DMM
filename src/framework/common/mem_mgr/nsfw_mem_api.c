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

#include <sys/types.h>
#include <unistd.h>
#include "nsfw_mem_desc.h"
#include "nstack_securec.h"

#ifdef SYS_MEM_RES_STAT
#include "common_mem_ring.h"
#include "common_mem_mempool.h"
#endif

#define MEM_OP_CALL_OK_RET(mtype, fun, para) { \
        if (g_nsfw_mem_ops[mtype].stmemop->fun) \
        { \
            return g_nsfw_mem_ops[mtype].stmemop->fun para; \
        } \
    }

/*****************************************************************************
*   Prototype    : nsfw_mem_init
*   Description  : memory mgr module init
*   Input        : point to nstack_fwmem_para
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_init (void *para)
{
  nsfw_mem_para *ptempara = NULL;
  i32 iret = NSFW_MEM_OK;
  i32 icount = 0;
  i32 iindex = 0;

  if (NULL == para)
    {
      NSCOMM_LOGERR ("ns mem init input error");
      return NSFW_MEM_ERR;
    }

  ptempara = (nsfw_mem_para *) para;

  if (ptempara->enflag >= NSFW_PROC_MAX)
    {
      NSCOMM_LOGERR ("ns mem init input enflag invalid] enflag=%d",
                     ptempara->enflag);
      return NSFW_MEM_ERR;
    }

  NSCOMM_LOGINF ("ns mem init begin] enflag=%d, iargsnum=%d",
                 ptempara->enflag, ptempara->iargsnum);

  for (iindex = 0; iindex < ptempara->iargsnum; iindex++)
    {
      NSCOMM_LOGINF ("%s", ptempara->pargs[iindex]);
    }

  for (icount = 0; icount < g_mem_type_num; icount++)
    {
      if ((NULL != g_nsfw_mem_ops[icount].stmemop)
          && (NULL != g_nsfw_mem_ops[icount].stmemop->mem_ops_init))
        {
          iret = g_nsfw_mem_ops[icount].stmemop->mem_ops_init (ptempara);

          if (NSFW_MEM_OK != iret)
            {
              NSCOMM_LOGERR ("mem init failed]index=%d, memtype=%d", icount,
                             g_nsfw_mem_ops[icount].entype);
              break;
            }
        }
    }

  /*if some module init fail, destory the modules that success */
  if (icount < g_mem_type_num)
    {
      for (iindex = 0; iindex < icount; iindex++)
        {
          if (g_nsfw_mem_ops[icount].stmemop->mem_ops_destroy)
            {
              g_nsfw_mem_ops[icount].stmemop->mem_ops_destroy ();
            }
        }

      return NSFW_MEM_ERR;
    }

  NSCOMM_LOGINF ("ns mem init end");
  return NSFW_MEM_OK;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_zone_create
*   Description  : create a block memory with name
*                  nsfw_mem_zone::stname
*                  nsfw_mem_zone::isize
*   note         : 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*   Input        : nsfw_mem_zone* pinfo
*   Output       : None
*   Return Value : mzone_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mzone_handle
nsfw_mem_zone_create (nsfw_mem_zone * pinfo)
{

  if ((NULL == pinfo) || (pinfo->stname.entype >= NSFW_MEM_TYPEMAX))
    {
      NSCOMM_LOGERR ("zone create input para error] pinfo=%p, mtype=%d",
                     pinfo, pinfo ? pinfo->stname.entype : (-1));
      return NULL;
    }

  MEM_OP_CALL_OK_RET (pinfo->stname.entype, mem_ops_zone_create, (pinfo));
  NSCOMM_LOGINF ("mem create fail] memtype=%d, name=%s, size=%zu",
                 pinfo->stname.entype, pinfo->stname.aname, pinfo->length);
  return NULL;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_zone_createv
*   Description  : create some memory blocks
*   note         : 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*   Input        : nsfw_mem_zone* pmeminfo
*                  i32 inum
*                  mzone_handle* paddr_array
*                  i32 iarray_num
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_zone_createv (nsfw_mem_zone * pmeminfo, i32 inum,
                       mzone_handle * paddr_array, i32 iarray_num)
{
  if ((NULL == pmeminfo) || (NULL == paddr_array)
      || (inum != iarray_num) || (inum <= 0)
      || (pmeminfo[0].stname.entype >= NSFW_MEM_TYPEMAX))
    {
      NSCOMM_LOGERR
        ("input para error] pmeminfo=%p, inum=%d, paddr_array=%p, iarray_num=%d, mtype=%d",
         pmeminfo, inum, paddr_array, iarray_num,
         pmeminfo ? pmeminfo[0].stname.entype : (-1));
      return NSFW_MEM_ERR;
    }

  MEM_OP_CALL_OK_RET (pmeminfo[0].stname.entype, mem_ops_zone_createv,
                      (pmeminfo, inum, paddr_array, iarray_num));
  NSCOMM_LOGINF ("mem create fail] memtype=%d", pmeminfo[0].stname.entype);
  return NSFW_MEM_ERR;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_zone_lookup
*   Description  : look up a memory
*                  1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*                  2. if the memory is shared, pname->enowner indicate that who create this memory.
*   note         : 1. when calling any shared memory create inferface, the name of memory end with _0 created by nStackMain,
*                     end with none created by nStackMaster, and end with _<pid> created by other.
*                  2. pname->enowner is available only when call look up shared memory.
*                  3. if the roles of process is NSFW_PROC_MASTER but the memory was created by others, or pname->enowner is NSFW_PROC_NULL,
*                     the name must be full name.
*                     for examles if the memory was created by nStackMain and pname->enowner is NSFW_PROC_NULL,
*                     must add '_0' at the end of name, if the memory was created by app and the role of process is NSFW_PROC_MASTER, must add
*                     _(pid) at the end of name, nstack_123.
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : mzone_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mzone_handle
nsfw_mem_zone_lookup (nsfw_mem_name * pname)
{
  NSFW_MEM_NAME_CHECK_RET_NULL (pname, "mem zone look up");
  MEM_OP_CALL_OK_RET (pname->entype, mem_ops_zone_lookup, (pname));
  NSCOMM_LOGERR ("mem lookup fail] memtype=%d, name=%s ", pname->entype,
                 pname->aname);
  return NULL;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_zone_release
*   Description  : release a memory
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_zone_release (nsfw_mem_name * pname)
{
  NSFW_MEM_NAME_CHECK_RET_ERR (pname, "mem zone release");
  MEM_OP_CALL_OK_RET (pname->entype, mem_ops_mzone_release, (pname));
  NSCOMM_LOGERR ("mem release fail] memtype=%d, name=%s", pname->entype,
                 pname->aname);
  return NSFW_MEM_ERR;

}

/*****************************************************************************
*   Prototype    : nsfw_mem_mbfmp_create
*   Description  : create a mbuf pool
*   Input        : nsfw_mem_mbfpool* pbufinfo
*   Output       : None
*   Return Value : mpool_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mpool_handle
nsfw_mem_mbfmp_create (nsfw_mem_mbfpool * pbufinfo)
{
  if ((NULL == pbufinfo) || (pbufinfo->stname.entype >= NSFW_MEM_TYPEMAX))
    {
      NSCOMM_LOGERR ("input para error] pbufinfo=%p, mtype=%d", pbufinfo,
                     pbufinfo ? pbufinfo->stname.entype : (-1));
      return NULL;
    }

  MEM_OP_CALL_OK_RET (pbufinfo->stname.entype, mem_ops_mbfmp_create,
                      (pbufinfo));
  NSCOMM_LOGERR ("mbufmp create fail] memtype=%d, name=%s ",
                 pbufinfo->stname.entype, pbufinfo->stname.aname);
  return NULL;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_mbfmp_createv
*   Description  : create some mbuf pools
*                  1. the name of length must be less than NSFW_MEM_APPNAME_LENGTH.
*   Input        : nsfw_mem_mbfpool* pmbfname
*                  i32 inum
*                  mpool_handle* phandle_array
*                  i32 iarray_num
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_mbfmp_createv (nsfw_mem_mbfpool * pmbfname, i32 inum,
                        mpool_handle * phandle_array, i32 iarray_num)
{
  if ((NULL == pmbfname) || (NULL == phandle_array)
      || (inum != iarray_num) || (inum <= 0)
      || (pmbfname[0].stname.entype >= NSFW_MEM_TYPEMAX))
    {
      NSCOMM_LOGERR
        ("input para error] pmbfname=%p, inum=%d, phandle_array=%p, iarray_num=%d",
         pmbfname, inum, phandle_array, iarray_num,
         pmbfname ? pmbfname[0].stname.entype : (-1));
      return NSFW_MEM_ERR;
    }

  MEM_OP_CALL_OK_RET (pmbfname[0].stname.entype, mem_ops_mbfmp_createv,
                      (pmbfname, inum, phandle_array, iarray_num));
  NSCOMM_LOGERR ("mbufmp createv fail] memtype=%d",
                 pmbfname[0].stname.entype);
  return NSFW_MEM_ERR;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_mbf_alloc
*   Description  : alloc a mbuf from mbuf pool
*   Input        : mpool_handle mhandle
*                  nsfw_mem_type entype
*   Output       : None
*   Return Value : mbuf_handle
*   Calls        :
*   Called By    :
*****************************************************************************/
mbuf_handle
nsfw_mem_mbf_alloc (mpool_handle mhandle, nsfw_mem_type entype)
{
  NSFW_MEM_MBUF_CHECK_RET_NULL (mhandle, entype, "mbf alloc");
  MEM_OP_CALL_OK_RET (entype, mem_ops_mbf_alloc, (mhandle));
  NSCOMM_LOGERR ("mbf alloc fail] handle=%p, type=%d", mhandle, entype);
  return NULL;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_mbf_free
*   Description  : put a mbuf backintp mbuf pool
*   Input        : mbuf_handle mhandle
*                  nsfw_mem_type entype
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_mbf_free (mbuf_handle mhandle, nsfw_mem_type entype)
{
  NSFW_MEM_MBUF_CHECK_RET_ERR (mhandle, entype, "mbuf free");
  MEM_OP_CALL_OK_RET (entype, mem_ops_mbf_free, (mhandle));
  NSCOMM_LOGERR ("mbf free fail] handle=%p, type=%d", mhandle, entype);
  return NSFW_MEM_ERR;

}

/*****************************************************************************
*   Prototype    : nsfw_mem_mbfmp_lookup
*   Description  : look up mbuf mpool
*                  1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*                  2. if the memory is shared, pname->enowner indicate that who create this memory.
*   note         : 1. when calling any shared memory create inferface, the name of memory end with _0 created by nStackMain,
*                     end with none created by nStackMaster, and end with _<pid> created by other.
*                  2. pname->enowner is available only when call look up shared memory.
*                  3. if the roles of process is NSFW_PROC_MASTER but the memory was created by others, or pname->enowner is NSFW_PROC_NULL,
*                     the name must be full name.
*                     for examles if the memory was created by nStackMain and pname->enowner is NSFW_PROC_NULL,
*                     must add '_0' at the end of name, if the memory was created by app and the role of process is NSFW_PROC_MASTER, must add
*                     _(pid) at the end of name, nstack_123.
*   Input        : nsfw_mem_name* pmbfname
*   Output       : None
*   Return Value : mpool_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mpool_handle
nsfw_mem_mbfmp_lookup (nsfw_mem_name * pmbfname)
{
  NSFW_MEM_NAME_CHECK_RET_NULL (pmbfname, "mbuf pool look up");
  MEM_OP_CALL_OK_RET (pmbfname->entype, mem_ops_mbfmp_lookup, (pmbfname));
  NSCOMM_LOGERR ("mbufmp lookup fail] memtype=%d, name=%s ", pmbfname->entype,
                 pmbfname->aname);
  return NULL;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_mbfmp_release
*   Description  : release mbuf pool
*   note         : 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_mbfmp_release (nsfw_mem_name * pname)
{
  NSFW_MEM_NAME_CHECK_RET_ERR (pname, "mbuf mp release");
  MEM_OP_CALL_OK_RET (pname->entype, mem_ops_mbfmp_release, (pname));
  NSCOMM_LOGERR ("mbfmp release fail] memtype=%d, name=%s", pname->entype,
                 pname->aname);
  return NSFW_MEM_ERR;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_sp_create
*   Description  : create a simple pool
*   note         : 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*   Input        : nsfw_mem_sppool* pmpinfo
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mring_handle
nsfw_mem_sp_create (nsfw_mem_sppool * pmpinfo)
{
  if ((NULL == pmpinfo) || (pmpinfo->stname.entype >= NSFW_MEM_TYPEMAX))
    {
      NSCOMM_LOGERR ("input para error] pmpinfo=%p, mtype=%d", pmpinfo,
                     pmpinfo ? pmpinfo->stname.entype : (-1));
      return NULL;
    }

  MEM_OP_CALL_OK_RET (pmpinfo->stname.entype, mem_ops_sp_create, (pmpinfo));
  NSCOMM_LOGERR ("sp create fail] memtype=%d, name=%s ",
                 pmpinfo->stname.entype, pmpinfo->stname.aname);
  return NULL;

}

/*****************************************************************************
*   Prototype    : nsfw_mem_sp_createv
*   Description  : create some simple pools one time
*   note         : 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*   Input        : nsfw_mem_sppool* pmpinfo
*                  i32 inum
*                  mring_handle* pringhandle_array
*                  i32 iarray_num
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_sp_createv (nsfw_mem_sppool * pmpinfo, i32 inum,
                     mring_handle * pringhandle_array, i32 iarray_num)
{
  NSFW_MEM_RINGV_CHECK_RET (pmpinfo, inum, pringhandle_array, iarray_num);
  MEM_OP_CALL_OK_RET (pmpinfo[0].stname.entype, mem_ops_sp_createv,
                      (pmpinfo, inum, pringhandle_array, iarray_num));
  NSCOMM_LOGERR ("sp createv fail] memtype=%d", pmpinfo[0].stname.entype);
  return NSFW_MEM_ERR;

}

/*****************************************************************************
*   Prototype    : nsfw_mem_sp_ring_create
*   Description  : create a simple pool with many rings
*   note         : 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*   Input        : nsfw_mem_mring* pringinfo
*                  mring_handle* pringhandle_array
*                  i32 iringnum
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_sp_ring_create (nsfw_mem_mring * pringinfo,
                         mring_handle * pringhandle_array, i32 iringnum)
{
  NSFW_MEM_RING_CHECK_RET (pringinfo, pringhandle_array, iringnum);
  MEM_OP_CALL_OK_RET (pringinfo[0].stname.entype, mem_ops_spring_create,
                      (pringinfo, pringhandle_array, iringnum));
  NSCOMM_LOGERR ("mppool spring creat fail] memtype=%d",
                 pringinfo[0].stname.entype);
  return NSFW_MEM_ERR;

}

/*****************************************************************************
*   Prototype    : nsfw_mem_sp_release
*   Description  : release a simple mempool
*   note         : 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_mem_sp_release (nsfw_mem_name * pname)
{
  NSFW_MEM_NAME_CHECK_RET_ERR (pname, "sp release");
  MEM_OP_CALL_OK_RET (pname->entype, mem_ops_sp_release, (pname));
  NSCOMM_LOGERR ("sp release fail] memtype=%d, name=%s ", pname->entype,
                 pname->aname);
  return NSFW_MEM_ERR;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_sp_lookup
*   Description  : look up a simpile ring
*                  1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*                  2. if the memory is shared, pname->enowner indicate that who create this memory.
*   note         : 1. when calling any shared memory create inferface, the name of memory end with _0 created by nStackMain,
*                     end with none created by nStackMaster, and end with _<pid> created by other.
*                  2. pname->enowner is available only when call look up shared memory.
*                  3. if the roles of process is NSFW_PROC_MASTER but the memory was created by others, or pname->enowner is NSFW_PROC_NULL,
*                     the name must be full name.
*                     for examles if the memory was created by nStackMain and pname->enowner is NSFW_PROC_NULL,
*                     must add '_0' at the end of name, if the memory was created by app and the role of process is NSFW_PROC_MASTER, must add
*                     _(pid) at the end of name, nstack_123.
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mring_handle
nsfw_mem_sp_lookup (nsfw_mem_name * pname)
{
  NSFW_MEM_NAME_CHECK_RET_NULL (pname, "sp look up");
  MEM_OP_CALL_OK_RET (pname->entype, mem_ops_sp_lookup, (pname));
  NSCOMM_LOGERR ("sp lookup fail] memtype=%d, name=%s", pname->entype,
                 pname->aname);
  return NULL;

}

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_create
*   Description  : create a ring
*   note         : 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*                  2. shared memory ring (NSFW_SHMEM) just can put a pointer into the queue, the queue also point to a shared block memory.
*                     no shared memory ring(NSFW_NSHMEM) is other wise.
*   Input        : nsfw_mem_mring* pringinfo
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mring_handle
nsfw_mem_ring_create (nsfw_mem_mring * pringinfo)
{
  if ((NULL == pringinfo) || (pringinfo->stname.entype >= NSFW_MEM_TYPEMAX))
    {
      NSCOMM_LOGERR ("input para error] pmpinfo=%p, mtype=%d", pringinfo,
                     pringinfo ? pringinfo->stname.entype : (-1));
      return NULL;
    }

  MEM_OP_CALL_OK_RET (pringinfo->stname.entype, mem_ops_ring_create,
                      (pringinfo));
  NSCOMM_LOGERR ("ring create fail] memtype=%d, name=%s ",
                 pringinfo->stname.entype, pringinfo->stname.aname);
  return NULL;

}

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_lookup
*   Description  : look up a ring by name
*       1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
*       2. if the memory is shared, pname->enowner indicate that who create this memory.
*           note:
*           1. when calling any shared memory create inferface, the name of memory end with _0 created by nStackMain,
*              end with none created by nStackMaster, and end with _<pid> created by other.
*           2. pname->enowner is available only when call look up shared memory.
*           3. if the roles of process is NSFW_PROC_MASTER but the memory was created by others, or pname->enowner is NSFW_PROC_NULL,
*              the name must be full name.
*              for examles if the memory was created by nStackMain and pname->enowner is NSFW_PROC_NULL,
*              must add '_0' at the end of name, if the memory was created by app and the role of process is NSFW_PROC_MASTER, must add
*              _(pid) at the end of name, nstack_123.
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*
*****************************************************************************/
mring_handle
nsfw_mem_ring_lookup (nsfw_mem_name * pname)
{
  NSFW_MEM_NAME_CHECK_RET_NULL (pname, "ring lookup");
  MEM_OP_CALL_OK_RET (pname->entype, mem_ops_ring_lookup, (pname));
  NSCOMM_LOGERR ("ring lookup fail] memtype=%d, name=%s", pname->entype,
                 pname->aname);
  return NULL;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_reset
*   Description  : reset the number of producer and consumer, also, the
*                  state of ring reset to empty
*   notes        : must be called before doing any operations base on the ring
*   Input        : mring_handle mhandle
*                  nsfw_mpool_type entype
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
nsfw_mem_ring_reset (mring_handle mhandle, nsfw_mpool_type entype)
{
  u32 loop = 0;
  struct nsfw_mem_ring *ring = (struct nsfw_mem_ring *) mhandle;

  if (!ring)
    {
      return;
    }

  ring->prod.head = 0;
  ring->cons.tail = 0;
  ring->ringflag = (u8) entype;

  /*init Ring */
  for (loop = 0; loop < ring->size; loop++)
    {
      /*
         for a empty ring, version is the mapping head val - size
         so the empty ring's ver is loop-size;
       */
      ring->ring[loop].data_s.ver = (loop - ring->size);
      ring->ring[loop].data_s.val = 0;
    }

  return;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_free_count
*   Description  : get the free number of ring
*   Input        : mring_handle mhandle
*   Output       : None
*   Return Value : u32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
u32
nsfw_mem_ring_free_count (mring_handle mhandle)
{
  struct nsfw_mem_ring *temp = NULL;
  u32 thead = 0;
  u32 ttail = 0;
  if (NULL == mhandle)
    {
      NSCOMM_LOGERR ("input para error] mhandle=%p", mhandle);
      return 0;
    }

  temp = (struct nsfw_mem_ring *) mhandle;
  thead = temp->prod.head;
  ttail = temp->cons.tail;
  return ttail + temp->size - thead;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_using_count
*   Description  : get the in using number of ring
*   Input        : mring_handle mhandle
*   Output       : None
*   Return Value : u32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
u32
nsfw_mem_ring_using_count (mring_handle mhandle)
{
  struct nsfw_mem_ring *temp = NULL;
  u32 thead = 0;
  u32 ttail = 0;
  if (NULL == mhandle)
    {
      NSCOMM_LOGERR ("input para error] mhandle=%p", mhandle);
      return 0;
    }

  temp = (struct nsfw_mem_ring *) mhandle;
  thead = temp->prod.head;
  ttail = temp->cons.tail;
  return thead - ttail;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_size
*   Description  : get size of ring
*   Input        : mring_handle mhandle
*   Output       : None
*   Return Value : u32
*   Calls        :
*   Called By    :
*****************************************************************************/
u32
nsfw_mem_ring_size (mring_handle mhandle)
{
  struct nsfw_mem_ring *temp = NULL;

  if (NULL == mhandle)
    {
      NSCOMM_LOGERR ("input para error] mhandle=%p", mhandle);
      return 0;
    }

  temp = (struct nsfw_mem_ring *) mhandle;

  return temp->size;
}

#ifdef SYS_MEM_RES_STAT
/*****************************************************************************
*   Prototype    : nsfw_mem_mbfpool_free_count
*   Description  : get the free mbuf count of a mbuf pool
*   Input        : mpool_handle mhandle
*   Output       : None
*   Return Value : u32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
u32
nsfw_mem_mbfpool_free_count (mpool_handle mhandle)
{
  if (!mhandle)
    {
      NSCOMM_LOGERR ("input para error] mhandle=%p", mhandle);
      return 0;
    }
  struct common_mem_mempool *mp = (struct common_mem_mempool *) mhandle;
  struct common_mem_ring *mp_ring =
    (struct common_mem_ring *) (mp->ring_align);
  if (!mp_ring)
    {
      NSCOMM_LOGERR ("ring is null");
      return 0;
    }
  u32 p_head = mp_ring->prod.head;
  u32 c_tail = mp_ring->cons.tail;

  return p_head - c_tail;
}
#endif /* SYS_MEM_RES_STAT */

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_release
*   Description  : release a ring memory
*   notes        : the length of name must be less than NSFW_MEM_APPNAME_LENGTH
*   Input        : nsfw_mem_name* pname
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_mem_ring_release (nsfw_mem_name * pname)
{
  NSFW_MEM_NAME_CHECK_RET_ERR (pname, "ring release");
  MEM_OP_CALL_OK_RET (pname->entype, mem_ops_ring_release, (pname));
  NSCOMM_LOGERR ("ring release fail] name=%s, type=%d", pname->aname,
                 pname->entype);
  return NSFW_MEM_ERR;

}

/*****************************************************************************
*   Prototype    : nsfw_mem_get_len
*   Description  : statics mbufpool, sppool, ring mem size.
*                  return: <=0, err happen, >0 mem size
*                  NSFW_MEM_MZONE: not surport because you already know the length when create
*   Input        : void * handle
*                  nsfw_mem_struct_type type
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*****************************************************************************/
ssize_t
nsfw_mem_get_len (void *handle, nsfw_mem_struct_type type)
{
  if (NULL == handle)
    {
      NSCOMM_LOGERR ("input para error] handle=%p", handle);
      return -1;
    }
  if ((NSFW_MEM_SPOOL == type) || (NSFW_MEM_RING == type))
    {
      struct nsfw_mem_ring *ring = (struct nsfw_mem_ring *) handle;
      if (ring->memtype >= NSFW_MEM_TYPEMAX)
        {
          NSCOMM_LOGERR ("invalid ring] ring type=%u ,handle=%p",
                         ring->memtype, handle);
          return -1;
        }
      MEM_OP_CALL_OK_RET (ring->memtype, mem_ops_mem_statics, (handle, type));
    }
  else
    {
      MEM_OP_CALL_OK_RET (NSFW_SHMEM, mem_ops_mem_statics, (handle, type));
    }
  return -1;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_mbuf_pool_recycle
*   Description  : recycle mbuf
*   Input        : mpool_handle handle
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_mem_mbuf_pool_recycle (mpool_handle handle)
{
  MEM_OP_CALL_OK_RET (NSFW_SHMEM, mem_ops_mbuf_recycle, (handle));
  return -1;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_sp_iterator
*   Description  : spool iterator
*   Input        : mpool_handle handle
*                  u32 start
*                  u32 end
*                  nsfw_mem_item_fun fun
*                  void *argv
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_sp_iterator (mpool_handle handle, u32 start, u32 end,
                      nsfw_mem_item_fun fun, void *argv)
{
  MEM_OP_CALL_OK_RET (NSFW_SHMEM, mem_ops_sp_iterator,
                      (handle, start, end, fun, argv));
  return -1;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_mbuf_iterator
*   Description  : mbuf iterator
*   Input        : mpool_handle handle
*                  u32 start
*                  u32 end
*                  nsfw_mem_item_fun fun
*                  void *argv
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32
nsfw_mem_mbuf_iterator (mpool_handle handle, u32 start, u32 end,
                        nsfw_mem_item_fun fun, void *argv)
{
  MEM_OP_CALL_OK_RET (NSFW_SHMEM, mem_ops_mbuf_iterator,
                      (handle, start, end, fun, argv));
  return -1;
}

/*****************************************************************************
*   Prototype    : nsfw_mem_dfx_ring_print
*   Description  : print ring info
*   Input        : mring_handle mhandle
*   Output       : None
*   Return Value : if no err happen, return the length of string print, 0 or -1 maybe err happen
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_mem_dfx_ring_print (mring_handle mhandle, char *pbuf, int length)
{
  struct nsfw_mem_ring *temp = (struct nsfw_mem_ring *) mhandle;
  u32 head = 0;
  u32 tail = 0;
  int ret = 0;
  if ((!temp) || (!pbuf) || (length <= 0))
    {
      return 0;
    }
  head = temp->prod.head;
  tail = temp->cons.tail;
  ret =
    SPRINTF_S (pbuf, length,
               "[.Head=%u,\n .Tail=%u,\n .(|Tail-Head|)=%u,\n .size=%u,\n .mask=%u]\n",
               head, tail, (tail >= head) ? (tail - head) : (head - tail),
               temp->size, temp->mask);
  return ret;
}
