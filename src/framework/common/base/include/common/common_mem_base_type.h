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

#ifndef _COMMON_MEM_BASE_TYPE_H_
#define _COMMON_MEM_BASE_TYPE_H_

#ifdef HAL_LIB

#else

#define ALIGN_TYPE  uint64_t
#define PTR_ALIGN_TYPE  uint64_t

/*alignment define*/
#define ALIGNMENT_DEF(name, type, aligntype) \
    union { \
        type name; \
        aligntype name##_align; \
    }

#define PTR_ALIGNMENT_DEF(name, type) ALIGNMENT_DEF(name, type, PTR_ALIGN_TYPE)

#define OTHER_ALIGNMENT_DEF(name, type) ALIGNMENT_DEF(name, type, ALIGN_TYPE)

/*
 *  * List definitions.
 *   */
#define DMM_LIST_HEAD(name, type)                       \
struct name {                               \
    PTR_ALIGNMENT_DEF(lh_first, struct type *);     /* first element */ \
}

#define DMM_LIST_ENTRY(type)                        \
struct {                                \
    PTR_ALIGNMENT_DEF(le_next, struct type *); /* next element */\
    PTR_ALIGNMENT_DEF(le_prev, struct type **); /* address of previous next element */ \
}

/*
 *  * Tail queue definitions.
 *   */
#define _DMM_TAILQ_HEAD(name, type, qual)                   \
struct name {                               \
    PTR_ALIGNMENT_DEF(tqh_first, qual type *);        /* first element */               \
    PTR_ALIGNMENT_DEF(tqh_last, qual type * qual *);        /* addr of last next element */             \
}

#define DMM_TAILQ_HEAD(name, type)  _DMM_TAILQ_HEAD(name, struct type,)

#define _DMM_TAILQ_ENTRY(type, qual)                    \
struct {                                \
    PTR_ALIGNMENT_DEF(tqe_next, qual type *); /* next element */\
    PTR_ALIGNMENT_DEF(tqe_prev, qual type * qual*); /* address of previous next element */\
}
#define DMM_TAILQ_ENTRY(type)   _DMM_TAILQ_ENTRY(struct type,)

/*
 * * Singly-linked Tail queue declarations.
 * */
#define DMM_STAILQ_HEAD(name, type)                 \
    struct name {                               \
       PTR_ALIGNMENT_DEF(stqh_first, struct type *); /* first element */         \
       PTR_ALIGNMENT_DEF(stqh_last, struct type **); /* addr of last next element */ \
    }

#define DMM_STAILQ_ENTRY(type)                      \
    struct {                               \
        PTR_ALIGNMENT_DEF(stqe_next, struct type *); /* next element */          \
    }
#endif

#endif
