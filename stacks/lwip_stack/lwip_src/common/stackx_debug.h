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

#ifndef __STACKX_DEBUG_H__
#define __STACKX_DEBUG_H__

//#include "lwip/arch.h"
#include <pthread.h>

#include <sys/syscall.h>

#include "nstack_log.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/** lower two bits indicate dbug level
 * - 0 all
 * - 1 warning
 * - 2 serious
 * - 3 severe
 */
#define STACKX_DBG_LEVEL_ALL 0x00
#define STACKX_DBG_LEVEL_OFF STACKX_DBG_LEVEL_ALL       /* compatibility define only */
#define STACKX_DBG_LEVEL_WARNING 0x01   /* bad checksums, dropped packets, ... */
#define STACKX_DBG_LEVEL_SERIOUS 0x02   /* memory allocation failures, ... */
#define STACKX_DBG_LEVEL_SEVERE 0x03
#define STACKX_DBG_MASK_LEVEL 0x00

/** flag  to enable that dbug message */
#define STACKX_DBG_ON NS_LOG_STACKX_ON

/** flag  to disable that dbug message */
#define STACKX_DBG_OFF NS_LOG_STACKX_OFF

/** flag for  indicating a tracing_message (to follow program flow) */
#define STACKX_DBG_TRACE NS_LOG_STACKX_TRACE

/** flag for  indicating a state dbug message (to follow module states) */
#define STACKX_DBG_STATE NS_LOG_STACKX_STATE

/** flag for  indicating newly added code, not thoroughly tested yet */
#define STACKX_DBG_FRESH NS_LOG_STACKX_FRESH

/** flag for  to halt after printing this dbug message */
#define STACKX_DBG_HALT NS_LOG_STACKX_HALT

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __STACKX_DEBUG_H__ */
