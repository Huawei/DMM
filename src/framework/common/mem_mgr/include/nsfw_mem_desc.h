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

#ifndef _NSFW_MEM_DESC_H
#define _NSFW_MEM_DESC_H
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "nsfw_mem_api.h"
#include "nsfw_mgr_com_api.h"
#include "nsfw_ring_data.h"

#define NSFW_MEM_NOT_INIT  (0)
#define NSFW_MEM_INIT_ERR  (1)
#define NSFW_MEM_INIT_OK   (2)

#define NSFW_NAME_LENCHECK_RET(name, desc) \
    { \
        i32 inamelen = strlen(name); \
        if (inamelen >= NSFW_MEM_APPNAME_LENGTH) \
        { \
            NSCOMM_LOGERR("name length check fail] desc=%s, name len=%d, expected max=%d", \
                         #desc, inamelen, NSFW_MEM_APPNAME_LENGTH); \
            return NSFW_MEM_ERR; \
        } \
    }

#define NSFW_NAME_LENCHECK_RET_NULL(name, desc) \
    { \
        i32 inamelen = strlen(name); \
        if (inamelen >= NSFW_MEM_APPNAME_LENGTH) \
        { \
            NSCOMM_LOGERR("name length check fail] desc=%s, name len=%d, expected max=%d", \
                         #desc, inamelen, NSFW_MEM_APPNAME_LENGTH); \
            return NULL; \
        } \
    }

#define NSFW_MEM_PARA_CHECK_RET(handle, pdata, desc, num)  {\
        if ((NULL == (handle)) || (NULL == (pdata)) || (num <= 0)\
            || (((struct  nsfw_mem_ring*)(handle))->memtype >= NSFW_MEM_TYPEMAX)) \
        {   \
            NSCOMM_LOGERR("input para error] desc=%s,mhandle=%p, pdata=%p, inum=%d", desc, (handle), (pdata), num); \
            return 0;  \
        }  \
    }

#define NSFW_MEM_ENQ_PARA_CHECK_RET(handle, desc)  {\
        if ((NULL == (handle)) \
            || (((struct  nsfw_mem_ring*)(handle))->memtype >= NSFW_MEM_TYPEMAX)) \
        {   \
            NSCOMM_LOGERR("input para error] desc=%s,mhandle=%p", desc, (handle)); \
            return 0;  \
        }  \
    }

#define NSFW_MEM_NAME_CHECK_RET_ERR(pname, desc)  {\
        if ((NULL == (pname)) || ((pname)->entype >= NSFW_MEM_TYPEMAX)) \
        { \
            NSCOMM_LOGERR("input para error] desc=%s, pname=%p, mtype=%d", desc, pname, (pname) ? (pname)->entype:-1); \
            return NSFW_MEM_ERR;  \
        }  \
    }

#define NSFW_MEM_NAME_CHECK_RET_NULL(pname, desc)  {\
        if ((NULL == (pname)) || ((pname)->entype >= NSFW_MEM_TYPEMAX)) \
        { \
            NSCOMM_LOGERR("input para error] desc=%s, pname=%p, mtype=%d", desc, pname, (pname) ? (pname)->entype:-1); \
            return NULL;  \
        }  \
    }

#define NSFW_MEM_RING_CHECK_RET(pringinfo, pringhandle_array, iringnum)  {\
        if ((NULL == pringinfo) || (NULL == pringhandle_array) || (pringinfo[0].stname.entype >= NSFW_MEM_TYPEMAX)) \
        {  \
            NSCOMM_LOGERR("input para error] pringinfo=%p, iringnum=%d, pringhandle_array=%p, mtype=%d",  \
                         pringinfo, iringnum, pringhandle_array, pringinfo ? pringinfo[0].stname.entype : (-1));  \
            return NSFW_MEM_ERR;  \
        }  \
    }

#define NSFW_MEM_RINGV_CHECK_RET(pmpinfo, inum, pringhandle_array, iarray_num)  { \
        if ((NULL == pmpinfo) || (NULL == pringhandle_array)  \
            || (inum != iarray_num) || (inum <= 0) || (pmpinfo[0].stname.entype >= NSFW_MEM_TYPEMAX)) \
        {   \
            NSCOMM_LOGERR("input para error] pmpinfo=%p, inum=%d, pringhandle_array=%p, iarray_num=%d", \
                         pmpinfo, inum, pringhandle_array, iarray_num, pmpinfo ? pmpinfo[0].stname.entype : (-1));  \
            return NSFW_MEM_ERR;  \
        }  \
    }

#define NSFW_MEM_MBUF_CHECK_RET_ERR(mhandle, entype, desc)  {\
        if ((NULL == mhandle) || (entype >= NSFW_MEM_TYPEMAX)) \
        { \
            NSCOMM_LOGERR("input para error] desc=%s, mhandle=%p, mtype=%d", desc, mhandle, entype); \
            return NSFW_MEM_ERR; \
        } \
    }

#define NSFW_MEM_MBUF_CHECK_RET_NULL(mhandle, entype, desc)  {\
        if ((NULL == mhandle) || (entype >= NSFW_MEM_TYPEMAX)) \
        { \
            NSCOMM_LOGERR("input para error] desc=%s, mhandle=%p, mtype=%d", desc, mhandle, entype); \
            return NULL; \
        } \
    }

/*memory access inferface define*/
typedef struct
{
  i32 (*mem_ops_init) (nsfw_mem_para * para);
  void (*mem_ops_destroy) (void);
    mzone_handle (*mem_ops_zone_create) (nsfw_mem_zone * pinfo);
    i32 (*mem_ops_zone_createv) (nsfw_mem_zone * pmeminfo, i32 inum,
                                 mzone_handle * paddr_array, i32 iarray_num);
    mzone_handle (*mem_ops_zone_lookup) (nsfw_mem_name * pname);
    i32 (*mem_ops_mzone_release) (nsfw_mem_name * pname);
    mpool_handle (*mem_ops_mbfmp_create) (nsfw_mem_mbfpool * pbufinfo);
    i32 (*mem_ops_mbfmp_createv) (nsfw_mem_mbfpool * pmbfname, i32 inum,
                                  mpool_handle * phandle_array,
                                  i32 iarray_num);
    mbuf_handle (*mem_ops_mbf_alloc) (mpool_handle mhandle);
    i32 (*mem_ops_mbf_free) (mbuf_handle mhandle);
    mpool_handle (*mem_ops_mbfmp_lookup) (nsfw_mem_name * pmbfname);
    i32 (*mem_ops_mbfmp_release) (nsfw_mem_name * pname);
    mring_handle (*mem_ops_sp_create) (nsfw_mem_sppool * pmpinfo);
    i32 (*mem_ops_sp_createv) (nsfw_mem_sppool * pmpinfo, i32 inum,
                               mring_handle * pringhandle_array,
                               i32 iarray_num);
    i32 (*mem_ops_spring_create) (nsfw_mem_mring * prpoolinfo,
                                  mring_handle * pringhandle_array,
                                  i32 iringnum);
    i32 (*mem_ops_sp_release) (nsfw_mem_name * pname);
    mring_handle (*mem_ops_sp_lookup) (nsfw_mem_name * pname);
    mring_handle (*mem_ops_ring_create) (nsfw_mem_mring * pringinfo);
    mring_handle (*mem_ops_ring_lookup) (nsfw_mem_name * pname);
    i32 (*mem_ops_ring_release) (nsfw_mem_name * pname);
    ssize_t (*mem_ops_mem_statics) (void *handle, nsfw_mem_struct_type type);
    i32 (*mem_ops_mbuf_recycle) (mpool_handle handle);
    i32 (*mem_ops_sp_iterator) (mpool_handle handle, u32 start, u32 end,
                                nsfw_mem_item_fun fun, void *argv);
    i32 (*mem_ops_mbuf_iterator) (mpool_handle handle, u32 start, u32 end,
                                  nsfw_mem_item_fun fun, void *argv);
} nsfw_mem_ops;

typedef struct
{
  nsfw_mem_type entype;
  nsfw_mem_ops *stmemop;
} nsfw_mem_attr;

typedef struct
{
  fw_poc_type enflag;           /*app, nStackMain, Master */
} nsfw_mem_localdata;

extern nsfw_mem_attr g_nsfw_mem_ops[];
extern i32 g_mem_type_num;
#endif
