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
#ifndef _DMM_PAUSE_H_
#define _DMM_PAUSE_H_

#include <emmintrin.h>

inline static void
dmm_pause (void)
{
  _mm_pause ();
}

#define DMM_PAUSE_WHILE(cond) do { dmm_pause(); } while (!!(cond))
#define DMM_WHILE_PAUSE(cond) do { while (!!(cond)) dmm_pause(); } while (0)

#endif /* #ifndef _DMM_PAUSE_H_ */
