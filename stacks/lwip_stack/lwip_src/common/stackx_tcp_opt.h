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

#ifndef __STACKX_TCP_OPT_H__
#define __STACKX_TCP_OPT_H__

#include "spl_opt.h"

/* TCP Options flags */
#define TF_ACK_DELAY        ((u32_t)0x01U)      /* Delayed ACK. */
#define TF_ACK_NOW          ((u32_t)0x02U)      /* Immediate ACK. */
#define TF_WINDOWSCALING    ((u32_t)0x40U)      /* Window scaling option enabled */
#define TF_INFR             ((u32_t)0x04U)      /* In fast recovery. */
#define TF_TIMESTAMP        ((u32_t)0x08U)      /* Timestamp option enabled */
#define TF_RXCLOSED         ((u32_t)0x10U)      /* rx closed by tcp_shutdown */
#define TF_FIN              ((u32_t)0x20U)      /* Connection was closed locally (FIN segment enqueued). */
#define TF_NODELAY          ((u32_t)0x100U)     /* Disable Nagle algorithm */
#define TF_NAGLEMEMERR      ((u32_t)0x80U)

/* TCP segment Options flags */
#define TF_SEG_OPTS_MSS             (u8_t)0x01U /* Include MSS option. */
#define TF_SEG_OPTS_TS              (u8_t)0x02U /* Include timestamp option. */
#define TF_SEG_OPTS_WS              (u8_t)0x08U /* Include window scaling option. */

#define STACKX_TCP_OPT_LENGTH(flags) \
    (flags & TF_SEG_OPTS_MSS ? 4  : 0) + \
    (flags & TF_SEG_OPTS_TS  ? 12 : 0) + \
    (flags & TF_SEG_OPTS_WS ? 4 : 0)
#endif
