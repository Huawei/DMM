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

#ifndef  __NSTACK_CUSTOM_API_H__
#define __NSTACK_CUSTOM_API_H__

/*****************************************************************************
*   Prototype    : create a custom socket ,when using this type .
*****************************************************************************/
#ifndef SOCK_NS_CUSTOM
#define SOCK_NS_CUSTOM  0xf001
#endif

/*****************************************************************************
*   Prototype    :  setsockopt level type
*****************************************************************************/
enum
{
  NSTACK_SOCKOPT = 0xff02
};

/*****************************************************************************
*   Prototype    :  setsockopt optname type
*****************************************************************************/
enum
{
  NSTACK_SEM_SLEEP = 0X0001
};
#endif
