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
#ifndef _DMM_CONFIG_H_
#define _DMM_CONFIG_H_

#ifndef DMM_VAR_DIR
#define DMM_VAR_DIR "/var/run"
#endif

#ifndef DMM_MAIN_SHARE_TYPE
#define DMM_MAIN_SHARE_TYPE DMM_SHARE_FSHM      /* 1 */
#endif

#ifndef DMM_MAIN_SHARE_SIZE
#define DMM_MAIN_SHARE_SIZE 1024        /* Megabyte */
#endif

#ifndef DMM_SHARE_TYPE
#define DMM_SHARE_TYPE DMM_SHARE_FSHM   /* 1 */
#endif

#ifndef DMM_SHARE_SIZE
#define DMM_SHARE_SIZE 16       /* Megabyte */
#endif

#ifndef DMM_HUGE_DIR
#define DMM_HUGE_DIR "/mnt/dmm-huge"
#endif

#endif /* _DMM_CONFIG_H_ */
