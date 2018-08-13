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

#ifndef _NSFW_MEM_API_H
#define _NSFW_MEM_API_H
#include <stdint.h>
#include <sys/types.h>

#include "types.h"
#include "nsfw_mgr_com_api.h"
#include "nstack_log.h"

#define NSFW_MEM_MGR_MODULE "nsfw_mem_mgr"

/*
 *the max len of memory name is 32bytes, but app just can use max 22bytes, left 10bytes to memory manager module
 */
#define NSFW_MEM_NAME_LENGTH     (32)
#define NSFW_MEM_APPNAME_LENGTH  (22)

#define NSFW_SOCKET_ANY     (-1)
#define NSFW_MEM_OK   (0)
#define NSFW_MEM_ERR  (-1)

/*
 *type of memory:
 *NSFW_SHMEM:shared memory
 *NSFW_NSHMEM:allocated by calling malloc
 */
typedef enum
{
  NSFW_SHMEM,
  NSFW_NSHMEM,
  NSFW_MEM_TYPEMAX,
} nsfw_mem_type;

/*type of ring operation*/
typedef enum
{
  NSFW_MRING_SPSC,              /*single producer single consumer ring */
  NSFW_MRING_MPSC,              /*multi producer single consumer ring */
  NSFW_MRING_SPMC,              /*single producer multi consumer ring */
  NSFW_MRING_MPMC,              /*multi producer multi consumer ring */
  NSFW_MRING_SPSC_ST,           /*single producer single consumer and belong to one thread ring */
  NSFW_MPOOL_TYPEMAX,
} nsfw_mpool_type;

typedef void *mpool_handle;
typedef void *mzone_handle;
typedef void *mbuf_handle;
typedef void *mring_handle;

/*initial of param*/
typedef struct
{
  i32 iargsnum;
  i8 **pargs;
  fw_poc_type enflag;           /*app, nStackMain, Master */
} nsfw_mem_para;

typedef struct
{
  nsfw_mem_type entype;
  fw_poc_type enowner;          /*notes: 1. when calling any shared memory create inferface, the name of memory end with _0 created by nStackMain,
                                 *              end with null created by nStackMaster, and end with _<pid> created by other.
                                 *           2. pname->enowner is available only when call look up shared memory.
                                 *           3. if the roles of process is NSFW_PROC_MASTER but the memory was created by others, or pname->enowner is NSFW_PROC_NULL,
                                 *              the name must be full name.
                                 *              for examles if the memory was created by nStackMain and pname->enowner is NSFW_PROC_NULL,
                                 *              must add '_0' at the end of name, if the memory was created by app and the role of process is NSFW_PROC_MASTER, must add
                                 *              _(pid) at the end of name, nstack_123.
                                 */
  i8 aname[NSFW_MEM_NAME_LENGTH];       /*the length of name must be less than NSFW_MEM_APPNAME_LENGTH. */
} nsfw_mem_name;

typedef struct
{
  nsfw_mem_name stname;
  size_t length;
  i32 isocket_id;
  i32 ireserv;
} nsfw_mem_zone;

typedef struct
{
  nsfw_mem_name stname;
  unsigned usnum;               /*the really created mbfpool num is (num+1) power of 2 */
  unsigned uscash_size;
  unsigned uspriv_size;
  unsigned usdata_room;
  i32 isocket_id;
  nsfw_mpool_type enmptype;
} nsfw_mem_mbfpool;

typedef struct
{
  nsfw_mem_name stname;
  u32 usnum;                    /*the really created sppool num is (num+1) power of 2 */
  u32 useltsize;
  i32 isocket_id;
  nsfw_mpool_type enmptype;
} nsfw_mem_sppool;

typedef struct
{
  nsfw_mem_name stname;
  u32 usnum;                    /*the really created ring num is (num+1) power of 2 */
  i32 isocket_id;
  nsfw_mpool_type enmptype;
} nsfw_mem_mring;

typedef enum
{
  NSFW_MEM_ALLOC_SUCC = 1,
  NSFW_MEM_ALLOC_FAIL = 2,
} nsfw_mem_alloc_state;

typedef enum
{
  NSFW_MEM_MZONE,
  NSFW_MEM_MBUF,
  NSFW_MEM_SPOOL,
  NSFW_MEM_RING
} nsfw_mem_struct_type;

typedef enum
{
  NSFW_RESERV_REQ_MSG,
  NSFW_RESERV_ACK_MSG,
  NSFW_MBUF_REQ_MSG,
  NSFW_MBUF_ACK_MSG,
  NSFW_SPPOOL_REQ_MSG,
  NSFW_SPPOOL_ACK_MSG,
  NSFW_RING_REQ_MSG,
  NSFW_RING_ACK_MSG,
  NSFW_RELEASE_REQ_MSG,
  NSFW_RELEASE_ACK_MSG,
  NSFW_MEM_LOOKUP_REQ_MSG,
  NSFW_MEM_LOOKUP_ACK_MSG,
  NSFW_MEM_MAX_MSG
} nsfw_remote_msg;

typedef struct __nsfw_shmem_msg_head
{
  unsigned usmsg_type;
  unsigned uslength;
  i32 aidata[0];
} nsfw_shmem_msg_head;

typedef struct __nsfw_shmem_ack
{
  void *pbase_addr;
  u16 usseq;
  i8 cstate;
  i8 creserv;
  i32 ireserv;
} nsfw_shmem_ack;

typedef struct __nsfw_shmem_reserv_req
{
  i8 aname[NSFW_MEM_NAME_LENGTH];
  u16 usseq;
  u16 usreserv;
  i32 isocket_id;
  size_t length;
  i32 ireserv;
} nsfw_shmem_reserv_req;

typedef struct __nsfw_shmem_mbuf_req
{
  i8 aname[NSFW_MEM_NAME_LENGTH];
  u16 usseq;
  u16 enmptype;
  unsigned usnum;
  unsigned uscash_size;
  unsigned uspriv_size;
  unsigned usdata_room;
  i32 isocket_id;
  i32 ireserv;
} nsfw_shmem_mbuf_req;

typedef struct __nsfw_shmem_sppool_req
{
  i8 aname[NSFW_MEM_NAME_LENGTH];
  u16 usseq;
  u16 enmptype;
  u32 usnum;
  u32 useltsize;
  i32 isocket_id;
  i32 ireserv;
} nsfw_shmem_sppool_req;

typedef struct __nsfw_shmem_ring_req
{
  i8 aname[NSFW_MEM_NAME_LENGTH];
  u16 usseq;
  u16 enmptype;
  u32 usnum;
  i32 isocket_id;
  i32 ireserv;
} nsfw_shmem_ring_req;

typedef struct __nsfw_shmem_free_req
{
  i8 aname[NSFW_MEM_NAME_LENGTH];
  u16 usseq;
  u16 ustype;                   /*structure of memory(memzone,mbuf,mpool,ring) */
  i32 ireserv;
} nsfw_shmem_free_req;

typedef struct __nsfw_shmem_lookup_req
{
  i8 aname[NSFW_MEM_NAME_LENGTH];
  u16 usseq;
  u16 ustype;                   /*structure of memory(memzone,mbuf,mpool,ring) */
  i32 ireserv;
} nsfw_shmem_lookup_req;

typedef int (*nsfw_mem_ring_enqueue_fun) (mring_handle ring, void *box);
typedef int (*nsfw_mem_ring_dequeue_fun) (mring_handle ring, void **box);
typedef int (*nsfw_mem_ring_dequeuev_fun) (mring_handle ring, void **box,
                                           unsigned int n);

typedef struct
{
  nsfw_mem_ring_enqueue_fun ring_ops_enqueue;
  nsfw_mem_ring_dequeue_fun ring_ops_dequeue;
  nsfw_mem_ring_dequeuev_fun ring_ops_dequeuev;
} nsfw_ring_ops;

/*
 * memory module init
 * para:point to nstack_fwmem_para
 */
i32 nsfw_mem_init (void *para);

/*
 * create a block memory with name
 * nsfw_mem_zone::stname
 * nsfw_mem_zone::isize
 * note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 */
mzone_handle nsfw_mem_zone_create (nsfw_mem_zone * pinfo);

/*
 *create some memory blocks
 * note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 */
i32 nsfw_mem_zone_createv (nsfw_mem_zone * pmeminfo, i32 inum,
                           mzone_handle * paddr_array, i32 iarray_num);

/*
 *look up a memory
 * note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 *       2. if the memory is shared, pname->enowner indicate that who create this memory,
 *           note:
 *           1. when calling any shared memory create inferface, the name of memory end with _0 created by nStackMain,
 *              end with none created by nStackMaster, and end with _<pid> created by other.
 *           2. pname->enowner is available only when call look up shared memory.
 *           3. if the roles of process is NSFW_PROC_MASTER but the memory was created by others, or pname->enowner is NSFW_PROC_NULL,
 *              the name must be full name.
 *              for examles if the memory was created by nStackMain and pname->enowner is NSFW_PROC_NULL,
 *              must add '_0' at the end of name, if the memory was created by app and the role of process is NSFW_PROC_MASTER, must add
 *              _(pid) at the end of name, nstack_123.
 */
mzone_handle nsfw_mem_zone_lookup (nsfw_mem_name * pname);

/*release a memory*/
i32 nsfw_mem_zone_release (nsfw_mem_name * pname);

/*
 *create a mbuf pool
 */
mpool_handle nsfw_mem_mbfmp_create (nsfw_mem_mbfpool * pbufinfo);

/*
 *create some mbuf pools
 * note: 1. the name of length must be less than NSFW_MEM_APPNAME_LENGTH.
 */
i32 nsfw_mem_mbfmp_createv (nsfw_mem_mbfpool * pmbfname, i32 inum,
                            mpool_handle * phandle_array, i32 iarray_num);

/*
 *alloc a mbuf from mbuf pool
 */
mbuf_handle nsfw_mem_mbf_alloc (mpool_handle mhandle, nsfw_mem_type entype);

/*
 *put a mbuf backintp mbuf pool
 */
i32 nsfw_mem_mbf_free (mbuf_handle mhandle, nsfw_mem_type entype);

/*
 *look up mbuf mpool
 * note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
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
 */
mpool_handle nsfw_mem_mbfmp_lookup (nsfw_mem_name * pmbfname);

/*
 *release mbuf pool
 * note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 */
i32 nsfw_mem_mbfmp_release (nsfw_mem_name * pname);

/*
 *create a simple pool
 *note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 */
mring_handle nsfw_mem_sp_create (nsfw_mem_sppool * pmpinfo);

/*
 *create some simple pools one time
 *note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 */
i32 nsfw_mem_sp_createv (nsfw_mem_sppool * pmpinfo, i32 inum,
                         mring_handle * pringhandle_array, i32 iarray_num);

/*
 *create a simple pool with many rings
 *note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 */
i32 nsfw_mem_sp_ring_create (nsfw_mem_mring * prpoolinfo,
                             mring_handle * pringhandle_array, i32 iringnum);

/*
 *release a simple mempool
 *note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 */
i32 nsfw_mem_sp_release (nsfw_mem_name * pname);

/*
 *look up a simpile ring
 * note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 *       2. if the memory is shared, pname->enowner indicate that who create this memory,
 *           note:
 *           1. when calling any shared memory create inferface, the name of memory end with _0 created by nStackMain,
 *              end with none created by nStackMaster, and end with _<pid> created by other.
 *           2. pname->enowner is available only when call look up shared memory.
 *           3. if the roles of process is NSFW_PROC_MASTER but the memory was created by others, or pname->enowner is NSFW_PROC_NULL,
 *              the name must be full name.
 *              for examles if the memory was created by nStackMain and pname->enowner is NSFW_PROC_NULL,
 *              must add '_0' at the end of name, if the memory was created by app and the role of process is NSFW_PROC_MASTER, must add
 *              _(pid) at the end of name, nstack_123.
 */
mring_handle nsfw_mem_sp_lookup (nsfw_mem_name * pname);

/*
 *create a ring
 *note: 1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 *      2. shared memory ring (NSFW_SHMEM) just can put a pointer into the queue, the queue also point to a shared block memory.
 *         no shared memory ring(NSFW_NSHMEM) is other wise.
 */
mring_handle nsfw_mem_ring_create (nsfw_mem_mring * pringinfo);

/*
 *look up a ring by name
 * note:1. the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 *       2. if the memory is shared, pname->enowner indicate that who create this memory,
 *           note:
 *           1. when calling any shared memory create inferface, the name of memory end with _0 created by nStackMain,
 *              end with none created by nStackMaster, and end with _<pid> created by other.
 *           2. pname->enowner is available only when call look up shared memory.
 *           3. if the roles of process is NSFW_PROC_MASTER but the memory was created by others, or pname->enowner is NSFW_PROC_NULL,
 *              the name must be full name.
 *              for examles if the memory was created by nStackMain and pname->enowner is NSFW_PROC_NULL,
 *              must add '_0' at the end of name, if the memory was created by app and the role of process is NSFW_PROC_MASTER, must add
 *              _(pid) at the end of name, nstack_123.
 */
mring_handle nsfw_mem_ring_lookup (nsfw_mem_name * pname);

/*
 * reset the number of producer and consumer, also, the state of ring reset to empty
 * notes: must be called before doing any operations base on the ring
 */
void nsfw_mem_ring_reset (mring_handle mhandle, nsfw_mpool_type entype);

extern nsfw_ring_ops g_ring_ops_arry[NSFW_MEM_TYPEMAX][NSFW_MPOOL_TYPEMAX];

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_dequeue
*   Description  : get a member from a ring
*   note         : if NSFW_SHMEM ring, pdata returned already a local address
*   Input        : mring_handle mhandle
*                  void** pdata
*   Output       : None
*   Return Value : the num of elment get from the queue, =0: get null, <0: err happen, >0: return num.
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline i32
nsfw_mem_ring_dequeue (mring_handle mhandle, void **pdata)
{
  if (NULL == mhandle || *((u8 *) mhandle) >= NSFW_MEM_TYPEMAX
      || *((u8 *) mhandle + 1) >= NSFW_MPOOL_TYPEMAX)
    {
      NSCOMM_LOGERR ("input para error] mhandle=%p", mhandle);
      return -1;
    }

  return
    g_ring_ops_arry[*((u8 *) mhandle)][*((u8 *) mhandle + 1)].ring_ops_dequeue
    (mhandle, pdata);
}

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_dequeuev
*   Description  : get some members from a ring
*   note         : if NSFW_SHMEM ring, pdata returned already a local address
*   Input        : mring_handle mhandle
*                  void** pdata
*                  unsigned inum
*   Output       : None
*   Return Value : the num of elment get from the queue, =0: get null, <0: err happen, >0: return num.
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline i32
nsfw_mem_ring_dequeuev (mring_handle mhandle, void **pdata, unsigned int inum)
{
  if (NULL == mhandle || *((u8 *) mhandle) >= NSFW_MEM_TYPEMAX
      || *((u8 *) mhandle + 1) >= NSFW_MPOOL_TYPEMAX)
    {
      NSCOMM_LOGERR ("input para error] mhandle=%p", mhandle);
      return -1;
    }

  return
    g_ring_ops_arry[*((u8 *) mhandle)][*
                                       ((u8 *) mhandle +
                                        1)].ring_ops_dequeuev (mhandle, pdata,
                                                               inum);
}

/*****************************************************************************
*   Prototype    : nsfw_mem_ring_enqueue
*   Description  : put a member back into a ring
*   note         : pdata must point to a shared block memory when put into the NSFW_SHMEM type memory ring, and the
*                  value of pdata must be local address
*   Input        : mring_handle mhandle
*                  void* pdata
*   Output       : None
*   Return Value : the num of elment put into the queue, =0: put null, <0: err happen, >0: return num.
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline i32
nsfw_mem_ring_enqueue (mring_handle mhandle, void *pdata)
{
  if (NULL == mhandle || *((u8 *) mhandle) >= NSFW_MEM_TYPEMAX
      || *((u8 *) mhandle + 1) >= NSFW_MPOOL_TYPEMAX)
    {
      NSCOMM_LOGERR ("input para error] mhandle=%p", mhandle);
      return -1;
    }

  return
    g_ring_ops_arry[*((u8 *) mhandle)][*((u8 *) mhandle + 1)].ring_ops_enqueue
    (mhandle, pdata);
}

/*
 *get the free number of ring
 */
u32 nsfw_mem_ring_free_count (mring_handle mhandle);

/*
 *get the in using number of ring
 */
u32 nsfw_mem_ring_using_count (mring_handle mhandle);

/*
 *get size of ring
 */
u32 nsfw_mem_ring_size (mring_handle mhandle);

/*
 *release a ring memory
 *note: the length of name must be less than NSFW_MEM_APPNAME_LENGTH.
 */
i32 nsfw_mem_ring_release (nsfw_mem_name * pname);

/*
 *statics mbufpool, sppool, ring mem size
 *return: <=0, err happen, >0 mem size
 * NSFW_MEM_MZONE: not surport because you already know the length when create
 */
ssize_t nsfw_mem_get_len (void *handle, nsfw_mem_struct_type type);

/*
 *recycle mbuf
 *
 */
i32 nsfw_mem_mbuf_pool_recycle (mpool_handle handle);

typedef int (*nsfw_mem_item_fun) (void *data, void *argv);

i32 nsfw_mem_sp_iterator (mpool_handle handle, u32 start, u32 end,
                          nsfw_mem_item_fun fun, void *argv);
i32 nsfw_mem_mbuf_iterator (mpool_handle handle, u32 start, u32 end,
                            nsfw_mem_item_fun fun, void *argv);

/*****************************************************************************
*   Prototype    : nsfw_mem_dfx_ring_print
*   Description  : print ring info
*   Input        : mring_handle mhandle
*                  char *pbuf
*                  int length
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
i32 nsfw_mem_dfx_ring_print (mring_handle mhandle, char *pbuf, int length);

#ifdef SYS_MEM_RES_STAT
u32 nsfw_mem_mbfpool_free_count (mpool_handle mhandle);
#endif

#endif
