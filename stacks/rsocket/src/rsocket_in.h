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

#ifndef _RSOCKET_IN_H_
#define _RSOCKET_IN_H_

#include "rsocket_rdma.h"

inline static void rr_rs_init (struct rsocket *rs);
inline static void rr_rs_dest (struct rsocket *rs);

static inline void rr_rs_notify_tcp (struct rsocket *rs);
static inline void rr_rs_notify_udp (struct rsocket *rs);

inline static void rr_rs_handle_tcp (struct rsocket *rs);
inline static int rr_rs_evfd (struct rsocket *rs);
inline static void rr_rs_connected (struct rsocket *rs);

#endif /* #ifndef _RSOCKET_IN_H_ */
