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

#ifndef NSFW_UPGRADE_H
#define NSFW_UPGRADE_H
#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define ENUM_STRUCT(struct) enum enum_upgrade_ ## struct

#define ENUM_MEM(struct, mem) struct ## _ ## mem

#define GET_MEM_BIT(mem_bit, struct, mem) (mem_bit & ((i64)1 << struct ## _ ## mem))

#define USE_MEM_BIT(mem_bit, struct, mem) (mem_bit |= ((i64)1 << struct ## _ ## mem))

#define UPGRADE_MEM_VAL(mem_bit, struct, mem, obj, defalut) {     \
        if (!GET_MEM_BIT(mem_bit, struct, mem)){                  \
            obj->mem = defalut;                                   \
        }                                                         \
    }

#define CONSTRUCT_UPGRADE_FUN(struct, obj) void upgrade_ ## struct (struct* obj)

#define CALL_UPGRADE_FUN(struct, obj) upgrade_ ## struct (obj)

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
