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

#ifndef _FW_SNAPSHOT_H
#define _FW_SNAPSHOT_H

#include <stdlib.h>
#include "types.h"
#include "../snapshot/fw_ss_tlv.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

// object type(7 bits) | member type (7 bits) | is object(1 bit) | is array(1 bit)
// member type is the type in object structure , it only effects the own object
/**
 *      type - object , type object of array, type base item
 */

#define NSFW_SS_TYPE_OBJ(_objType) (0xfe00 & ((_objType) << 9))
#define NSFW_SS_TYPE_GETOBJ(_objType) ((0xfe00 & (_objType)) >> 9)

#define NSFW_SS_TYPE_MEMBER(_memType) (0x01fc & ((_memType) << 2))
#define NSFW_SS_TYPE_GET_MEMBER(_memType) ((0x01fc & (_memType)) >> 2)

#define NSFW_SS_TYPE_SET_MEMBER_ITEM(_memType) (NSFW_SS_TYPE_MEMBER((_memType)))
#define NSFW_SS_TYPE_SET_MEMBER_OBJ(_objType, _memType) \
    (NSFW_SS_TYPE_OBJ((_objType)) | NSFW_SS_TYPE_SET_MEMBER_ITEM((_memType)) | 0x02)
#define NSFW_SS_TYPE_SET_MEMBER_OBJ_ARRAY(_objType, _memType) \
    (NSFW_SS_TYPE_SET_MEMBER_OBJ((_objType), (_memType)) | 0x01)

#define NSFW_SS_TYPE_IS_MEMBER_OBJ(_memType) ((_memType) & 0x02)
#define NSFW_SS_TYPE_IS_MEMBER_ARRAY(_memType) ((_memType) & 0x01)

/*
 *  This structure tells how to describe one object and its members
 */
typedef struct _nsfw_ss_objMemDesc
{
  u16 type;                     // object type(7 bits) | member type (7 bits) | is object(1 bit) | is array(1 bit)
  u32 length;
  u8 offset;
} nsfw_ss_objMemDesc_t;

typedef struct _nsfw_ss_objDesc
{
  u16 objType;                  /* Type number of object */
  u8 memNum;                    /* Nubmer of object members */
  u32 objSize;
  nsfw_ss_objMemDesc_t *memDesc;        /* Member descript */
} nsfw_ss_objDesc_t;

#define NSFW_SS_MAX_OBJDESC_NUM 256

/* Traversal, you can save the current value to start the search, do not need to start from the beginning of the search, because the value is generally a sequential */

typedef struct _nsfw_ss_objDescManager
{
  nsfw_ss_objDesc_t *g_nsfw_ss_objDescs[NSFW_SS_MAX_OBJDESC_NUM];
  int g_nsfw_ss_objDesNum;
} nsfw_ss_objDescManager_t;

extern nsfw_ss_objDescManager_t g_nsfw_ss_objDescManager;

#define nsfw_ss_getObjDescManagerInst() (&g_nsfw_ss_objDescManager)

/**
 * @Function        nsfw_ss_register_ObjDesc
 * @Description     Register object description to snapshot
 * @param           objDesc - description of object
 * @return          void
 */
extern void nsfw_ss_register_ObjDesc (nsfw_ss_objDesc_t * objDesc);

/**
 * @Function    nsfw_ss_store
 * @Description store object to memory
 * @param (in)  objType - type of object
 * @param (in)  obj - address of object memory
 * @param (in)  storeMem - address of memory to store object data
 * @param (in)  storeMemLen - maximal length of storage memory
 * @return positive integer means length of memory cost on success. return -1 if error
 */
extern int nsfw_ss_store (u16 objType, void *obj, void *storeMem,
                          u32 storeMemLen);

/**
 * @Function    nsfw_ss_restore
 * @Description restore object from memory
 * @param (in)  objMem - memory of object
 * @param (in)  mem - memory of storage
 * @param (in)  storeMemLen - maximal length of storage memory
 * @return  positive integer stands on object type, -1 on error
 */
extern int nsfw_ss_restore (void *objMem, void *mem, u32 storeMemLen);

/**
 * @Function        nsfw_ss_getObjStoreMemLen
 * @Description     Get the maximal memory it needs
 * @param (in)  objType - type of object
 * @return  length of memory needs, -1 if error
 */
extern int nsfw_ss_getObjStoreMemLen (int objType);

#define _NSFW_SNAPSHOT_OBJDESC_REGISTER_SURFIX(_value, _surfix) \
    static __attribute__((__constructor__(121))) void nsfw_snapshot_objdesc_register_constructor##_surfix(void){\
        nsfw_ss_register_ObjDesc((_value));\
    }

#define NSFW_SNAPSHOT_OBJDESC_REGISTER_SURFIX(_value, _surfix) \
    _NSFW_SNAPSHOT_OBJDESC_REGISTER_SURFIX(_value, _surfix)

#define NSFW_SNAPSHOT_OBJDESC_REGISTER_UNIQUE(_value) \
    NSFW_SNAPSHOT_OBJDESC_REGISTER_SURFIX(_value, __LINE__)

/**
 *  Using this macro to register object description
 */
#define NSFW_SS_REGSITER_OBJDESC(_value) \
        NSFW_SNAPSHOT_OBJDESC_REGISTER_UNIQUE(_value)

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _FW_SNAPSHOT_H */
