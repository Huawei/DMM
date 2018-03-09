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

#ifndef _FW_SS_TLV_H
#define _FW_SS_TLV_H

#include <stdlib.h>
#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

typedef struct _nsfw_ss_tlv
{
  u16 type;
  u32 length;
  void *value;
} nsfw_ss_tlv_t;

#define tlv_header_length() ((size_t)(&((nsfw_ss_tlv_t*)0)->value))

#define tlv_mem_forward(mem, step) ((mem) = (void*) ((char*)(mem) + (step)))
#define tlv_header(mem) tlv_mem_forward((mem), tlv_header_length())     // move header

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _FW_SS_TLV_H */
