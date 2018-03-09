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

#ifndef COMPILE_CONFIG_H
#define COMPILE_CONFIG_H

#ifndef NSTACK_STATIC
#ifndef NSTACK_STATIC_CHECK
#define NSTACK_STATIC static
#else
#define NSTACK_STATIC
#endif
#endif

#include "compiling_check.h"

#endif /*compile_config.h */
