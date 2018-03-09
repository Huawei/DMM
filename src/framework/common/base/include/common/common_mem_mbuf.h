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

/*******************************************************************
                   Copyright 2017 - 2047, Huawei Tech. Co., Ltd.
                             ALL RIGHTS RESERVED

Filename      : common_mem_mbuf.h
Description   :
Version       : 1.1
********************************************************************/

#ifndef _COMMON_MEM_MBUF_H_
#define _COMMON_MEM_MBUF_H_

#ifdef HAL_LIB
#else
#include "rte_mbuf.h"
#include "common_func.h"

typedef uint32_t (*dmm_mbuf_item_fun) (void *data, void *argv);
int32_t dmm_pktmbuf_pool_iterator (struct common_mem_mempool *mp,
                                   uint32_t start, uint32_t end,
                                   dmm_mbuf_item_fun fun, void *argv);
#endif

#endif /* _COMMON_MEM_MBUF_H_ */
