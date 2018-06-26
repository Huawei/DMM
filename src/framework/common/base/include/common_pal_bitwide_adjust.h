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

#ifndef _COMMON_PAL_BITWIDE_ADJUST_H_
#define _COMMON_PAL_BITWIDE_ADJUST_H_

#ifdef HAL_LIB
#include "pal_bitwide_adjust.h"
#else
#define MODULE(name)   (1)

#include "common_mem_common.h"

#include "common_func.h"

#define ALIGN_SIZET(size)  ((uint64_t)(size))
#define ALIGN_PTR(PTR)    ((uint64_t)(PTR))

extern struct common_mem_memseg *g_PMemSegArry;
extern void **g_LMegAddrArry;

/*get Local Seg addr by segIdx*/
#define HMEM_SEG_LVADDR(segid)  (g_LMegAddrArry[segid])
/*get SegIDX by PrimSegAddr, just get the array Idx of g_PMemSegArry*/
#define HMEM_SEGID(segaddr)   ((struct common_mem_memseg*)segaddr - &(g_PMemSegArry[0]))

/*****************************************************************
Parameters    : LMegAddrArry[]    Local common_mem_memseg addr Array
                     SegNum              common_mem_memseg Num.
Return        :
Description   :  init  g_PrimAddr2LocalMap g_LocalAddr2PrimMap while the process start
*****************************************************************/
void *pal_shddr_to_laddr (uint64_t shaddr);
uint64_t pal_laddr_to_shddr (void *laddr);
int dmm_pal_addr_align ();

extern int g_PrimSameFlg;

/* if __NSTACK_MAIN__ is defined, no need do addr trans*/
#ifndef __NSTACK_MAIN__
/* g_PrimSameFlg check should be done before calling cast functions */

/*share memory address to local virtual address*/
#define ADDR_SHTOL(addr)  (g_PrimSameFlg ? ((void*) (addr)) : pal_shddr_to_laddr((uint64_t)(addr)))

/*local virtual address to share memory address according to memseg*/
#define ADDR_LTOSH(addr)  (g_PrimSameFlg ? ((uint64_t)(addr)) : pal_laddr_to_shddr((void*)(addr)))

#define PTR_SHTOL(type, addr) ((type)ADDR_SHTOL(addr))

/*local virtual address to share memory address; for compatible,  not delete ADDR_LTOSH_EXT*/
#define ADDR_LTOSH_EXT(addr)  ADDR_LTOSH(addr)
#else
/*share memory address to local virtual address*/
#define ADDR_SHTOL(addr)  ((void*)(addr))

/*local virtual address to share memory address according to memseg*/
#define ADDR_LTOSH(addr)  ((uint64_t)(addr))

#define PTR_SHTOL(type, addr) ((type)(addr))

/*local virtual address to share memory address; for compatible,  not delete ADDR_LTOSH_EXT*/
#define ADDR_LTOSH_EXT(addr)  ADDR_LTOSH(addr)
#endif

#if MODULE("list")
#define COMMON_LIST_INSERT_HEAD(lhead, lelm, field) do {               \
    if (((lelm)->field.le_next_align = (lhead)->lh_first_align) != ((typeof((lhead)->lh_first_align))(long)NULL))       \
         ((typeof((lhead)->lh_first))ADDR_SHTOL((lhead)->lh_first_align))->field.le_prev_align = \
            ADDR_LTOSH(&(lelm)->field.le_next); \
    (lhead)->lh_first_align = ADDR_LTOSH(lelm);                 \
    (lelm)->field.le_prev_align = ADDR_LTOSH(&(lhead)->lh_first);           \
} while (/*CONSTCOND*/0)

#define COMMON_LIST_REMOVE(lelm, field) do {                   \
    if ((lelm)->field.le_next_align != ((typeof((lelm)->field.le_next_align))ALIGN_PTR(NULL)))              \
        ((typeof((lelm)->field.le_next))ADDR_SHTOL((lelm)->field.le_next_align))->field.le_prev_align =             \
            (lelm)->field.le_prev_align; \
     if (EOK != (MEMCPY_S((typeof((lelm)->field.le_prev))ADDR_SHTOL((lelm)->field.le_prev_align), \
            sizeof((lelm)->field.le_next_align), \
             &((lelm)->field.le_next_align), \
            sizeof((lelm)->field.le_next_align)))) \
     {\
            NSCOMM_LOGERR("MEMCPY_S failed.");\
            return;\
     }\
} while (/*CONSTCOND*/0)

#define COMMON_LIST_EMPTY(lhead)       ((typeof((lhead)->lh_first))ADDR_SHTOL((lhead)->lh_first_align) == NULL)
#define COMMON_LIST_FIRST(lhead)       ((typeof((lhead)->lh_first))ADDR_SHTOL((lhead)->lh_first_align))
#define COMMON_LIST_NEXT(lelm, field)      ((typeof((lelm)->field.le_next))ADDR_SHTOL((lelm)->field.le_next_align))

#endif

#if MODULE("tailq")

#define COMMON_TAILQ_INSERT_TAIL(lhead, lelm, field) do {          \
    (lelm)->field.tqe_next_align = (typeof((lelm)->field.tqe_next_align))NULL;                  \
    (lelm)->field.tqe_prev_align = (lhead)->tqh_last_align;         \
    typeof((lhead)->tqh_last_align) tempelm = ADDR_LTOSH(lelm);\
    if (EOK != (MEMCPY_S(ADDR_SHTOL((lhead)->tqh_last_align), sizeof(tempelm), &tempelm, sizeof(tempelm)))) \
    {\
            NSCOMM_LOGERR("MEMCPY_S failed.");\
    }\
    (lhead)->tqh_last_align = ADDR_LTOSH(&(lelm)->field.tqe_next);          \
} while (/*CONSTCOND*/0)

#define COMMON_TAILQ_FOREACH(lvar, lhead, field)                   \
    for ((lvar) = (typeof(lvar))ADDR_SHTOL((lhead)->tqh_first_align);               \
        (lvar);                         \
        (lvar) = (typeof(lvar))ADDR_SHTOL((lvar)->field.tqe_next_align))

#define COMMON_TAILQ_REMOVE(lhead, lelm, field) do {               \
        if (((lelm)->field.tqe_next_align) != (typeof((lelm)->field.tqe_next_align))NULL)                \
            ((typeof((lelm)->field.tqe_next))ADDR_SHTOL((lelm)->field.tqe_next_align))->field.tqe_prev_align =         \
                (lelm)->field.tqe_prev_align;              \
        else                                \
            (lhead)->tqh_last_align = (lelm)->field.tqe_prev_align;       \
        if (EOK != (MEMCPY_S(ADDR_SHTOL((lelm)->field.tqe_prev_align), \
            sizeof((lelm)->field.tqe_next_align), \
            &((lelm)->field.tqe_next_align), \
            sizeof((lelm)->field.tqe_next_align))))         \
        {\
            NSCOMM_LOGERR("MEMCPY_S failed.");\
        }\
    } while (/*CONSTCOND*/0)

/*
 * Tail queue functions.
 */
#define COMMON_TAILQ_INIT(head) do {                       \
            (head)->tqh_first_align = (typeof((head)->tqh_first_align))NULL;                    \
            (head)->tqh_last_align = ADDR_LTOSH(&(head)->tqh_first);              \
        } while (/*CONSTCOND*/0)

/*
 * Tail queue access methods.
 */
#define COMMON_TAILQ_EMPTY(head)       ((head)->tqh_first_align == (typeof((head)->tqh_first_align))NULL)
#define COMMON_TAILQ_FIRST(head)       ((typeof((head)->tqh_first))ADDR_SHTOL((head)->tqh_first_align))
#define COMMON_TAILQ_NEXT(elm, field)      ((typeof((elm)->field.tqe_next))ADDR_SHTOL((elm)->field.tqe_next_align))

#endif

#if MODULE("stailq")
/*
* Singly-linked Tail queue functions.
*/
#define    COMMON_STAILQ_INIT(head) do {                      \
    (head)->stqh_first_align = ALIGN_PTR(NULL);                  \
    (head)->stqh_last_align = ADDR_LTOSH(&(head)->stqh_first);                \
} while (/*CONSTCOND*/0)

#define    COMMON_STAILQ_INSERT_TAIL(head, elm, field) do {           \
    (elm)->field.stqe_next_align = ALIGN_PTR(NULL);                  \
    typeof((head)->stqh_last_align) telm = ADDR_LTOSH(elm);\
    if (EOK != (MEMCPY_S(ADDR_SHTOL((head)->stqh_last_align), sizeof(telm), &telm, sizeof(telm))))\
    {\
        NSCOMM_LOGERR("MEMCPY_S failed.");\
    }\
    (head)->stqh_last_align =  ADDR_LTOSH(&(elm)->field.stqe_next);            \
} while (/*CONSTCOND*/0)

#define    COMMON_STAILQ_REMOVE_HEAD(head, field) do {                \
    if (((head)->stqh_first_align = \
           ((typeof((head)->stqh_first))ADDR_SHTOL((head)->stqh_first_align))->field.stqe_next_align) == \
           (PTR_ALIGN_TYPE)NULL) \
        (head)->stqh_last_align = ADDR_LTOSH(&(head)->stqh_first);            \
} while (/*CONSTCOND*/0)

#define COMMON_STAILQ_FOREACH(var, head, field)                \
    for ((var) = ADDR_SHTOL((head)->stqh_first_align);              \
        (var);                          \
        (var) = ADDR_SHTOL((var)->field.stqe_next_align))

/*
* Singly-linked Tail queue access methods.
*/

#define    COMMON_STAILQ_EMPTY(head)  ((head)->stqh_first_align == (PTR_ALIGN_TYPE)NULL)

#define    COMMON_STAILQ_FIRST(head)  (ADDR_SHTOL((head)->stqh_first_align))

#define    COMMON_STAILQ_NEXT(elm, field) (ADDR_SHTOL((elm)->field.stqe_next_align))
#endif

#endif

#endif
