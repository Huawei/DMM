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

#ifndef _PIDINFO_H_
#define _PIDINFO_H_

#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define NSFW_MAX_FORK_NUM 32
typedef struct
{
  u32 used_size;
  u32 apid[NSFW_MAX_FORK_NUM];
} nsfw_pidinfo;

inline i32 nsfw_pidinfo_init (nsfw_pidinfo * pidinfo);
inline int nsfw_add_pid (nsfw_pidinfo * pidinfo, u32 pid);
inline int nsfw_del_pid (nsfw_pidinfo * pidinfo, u32 pid);
inline int nsfw_del_last_pid (nsfw_pidinfo * pidinfo, u32 pid);
inline int nsfw_pid_exist (nsfw_pidinfo * pidinfo, u32 pid);
inline int nsfw_pidinfo_empty (nsfw_pidinfo * pidinfo);

#ifdef __cplusplus
/* *INDENT-OFF* */
};
/* *INDENT-ON* */
#endif

#endif /* _PIDINFO_H_ */
