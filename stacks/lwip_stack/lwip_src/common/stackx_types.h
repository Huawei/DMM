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

#ifndef STACKX_TYPES_H
#define STACKX_TYPES_H

#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if 1
#ifndef STACKX_MAX_S16_NUM
#define STACKX_MAX_S16_NUM ((s16_t)0x7fff)
#endif

#ifndef STACKX_MAX_U16_NUM
#define STACKX_MAX_U16_NUM ((u16_t)0xFfff)
#endif

#ifndef STACKX_MAX_U32_NUM
#define STACKX_MAX_U32_NUM ((u32_t)0xffffffff)
#endif

#ifndef STACKX_MAX_S32_NUM
#define STACKX_MAX_S32_NUM ((s32_t)0x7fffffff)
#endif

#ifndef STACKX_MAX_U64_NUM
#define STACKX_MAX_U64_NUM ((u64_t)0xffffffffffffffff)
#endif

#ifndef STACKX_MAX_S64_NUM
#define STACKX_MAX_S64_NUM ((s64_t)0x7fffffffffffffff)
#endif
#endif

#if 1
typedef uint64_t u64_t;
typedef int64_t s64_t;

typedef uint32_t u32_t;
typedef int32_t s32_t;

typedef uint16_t u16_t;
typedef int16_t s16_t;

typedef uint8_t u8_t;
typedef int8_t s8_t;

typedef uintptr_t mem_ptr_t;
#endif

#endif
