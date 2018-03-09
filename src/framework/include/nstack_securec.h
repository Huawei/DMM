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

#ifndef __NSTACK_SECUREC_H__
#define __NSTACK_SECUREC_H__

/*if you need export the function of this library in Win32 dll, use __declspec(dllexport) */

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */

#endif /*  */
#ifndef SECUREC_LIB
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include <string.h>

#ifndef NULL
#define NULL ((void *)0)
#endif /*
        */

/*define error code*/
#ifndef errno_t
typedef int errno_t;

#endif /*
        */

/* success */
#define EOK (0)

/* invalid parameter */
#ifndef EINVAL
#define EINVAL (22)
#endif /*
        */

#define EINVAL_AND_RESET  (22 | 0X80)
#define ERANGE_AND_RESET  (34 | 0X80)

#define   SCANF_S         scanf
#define   WSCANF_S        wscanf
#define   VSCANF_S        vscanf
#define   VWSCANF_S       vwscanf
#define   FSCANF_S        fscanf
#define   FWSCANF_S       fwscanf
#define   VFSCANF_S       vfscanf
#define   VFWSCANF_S      vfwscanf
#define   SSCANF_S        sscanf
#define   SWSCANF_S       swscanf
#define   VSSCANF_S       vsscanf
#define   VSWSCANF_S      vswscanf

#define   SPRINTF_S(a, b, ...)           sprintf(a, ##__VA_ARGS__)
#define   SWPRINTF_S(a, b, c,  ...)      swprintf(a, b, c, ##__VA_ARGS__)
#define   VSPRINTF_S(a, b, c, d)         vsprintf(a, c, d)
#define   VSWPRINTF_S(a, b, c, d)        vswprintf(a, b, c, d)
#define   VSNPRINTF_S(a, b, c, d, e)     vsnprintf(a, c, d, e)
#define   SNPRINTF_S(a, b, c,  d, ...)   snprintf(a, c, d, ##__VA_ARGS__)

#define   WMEMCPY_S(a, b, c, d)       ((NULL == wmemcpy(a, c, d)) ? EINVAL : EOK)
#define   MEMMOVE_S(a, b, c, d)       ((NULL == memmove(a, c, d)) ? EINVAL : EOK)
#define   WMEMMOVE_S(a, b, c, d)      ((NULL == wmemmove(a, c, d)) ? EINVAL : EOK)
#define   WCSCPY_S(a, b, c)           ((NULL == wcscpy(a, c)) ? EINVAL : EOK)
#define   WCSNCPY_S(a, b, c, d)       ((NULL == wcsncpy(a, c, d)) ? EINVAL : EOK)
#define   WCSCAT_S(a, b, c)           ((NULL == wcscat(a, c)) ? EINVAL : EOK)
#define   WCSNCAT_S(a, b, c, d)       ((NULL == wcsncat(a, c, d)) ? EINVAL : EOK)

#define   MEMSET_S(a, b, c, d)        ((NULL == memset(a, c, d)) ? EINVAL : EOK)
#define   MEMCPY_S(a, b, c, d)        ((NULL == memcpy(a, c, d)) ? EINVAL : EOK)
#define   STRCPY_S(a, b, c)           ((NULL == strcpy(a, c )) ? EINVAL : EOK)
#define   STRNCPY_S(a, b, c, d)       ((NULL == strncpy(a, c, d)) ? EINVAL : EOK)
#define   STRCAT_S(a, b, c)           ((NULL == strcat(a, c)) ? EINVAL : EOK)
#define   STRNCAT_S(a, b, c, d)       ((NULL == strncat(a, c, d)) ? EINVAL : EOK)

#define   STRTOK_S(a, b, c)  strtok(a, b)
#define   WCSTOK_S(a, b, c)  wcstok(a, b)
#define   GETS_S(a, b)       gets(a)

#else /*  */
#include "securec.h"

#define   SCANF_S         scanf_s
#define   WSCANF_S        wscanf_s
#define   VSCANF_S        vscanf_s
#define   VWSCANF_S       vwscanf_s
#define   FSCANF_S        fscanf_s
#define   FWSCANF_S       fwscanf_s
#define   VFSCANF_S       vfscanf_s
#define   VFWSCANF_S      vfwscanf_s
#define   SSCANF_S        sscanf_s
#define   SWSCANF_S       swscanf_s
#define   VSSCANF_S       vsscanf_s
#define   VSWSCANF_S      vswscanf_s

#define   SPRINTF_S(a, b, ...)           sprintf_s (a, b, ##__VA_ARGS__)
#define   SWPRINTF_S(a, b, c,  ...)      swprintf_s(a, b, c, ##__VA_ARGS__)
#define   VSPRINTF_S(a, b, c, d)         vsprintf_s(a, b, c, d)
#define   VSWPRINTF_S(a, b, c, d)        vswprintf_s(a, b, c, d)
#define   VSNPRINTF_S(a, b, c, d, e)     vsnprintf_s(a, b, c, d, e)
#define   SNPRINTF_S(a, b, c,  d, ...)   snprintf_s(a, b, c, d, ##__VA_ARGS__)

#define   WMEMCPY_S(a, b, c, d)          wmemcpy_s(a, b, c, d)
#define   MEMMOVE_S(a, b, c, d)          memmove_s(a, b, c, d)
#define   WMEMMOVE_S(a, b, c, d)         wmemmove_s(a, b, c, d)
#define   WCSCPY_S(a, b, c)              wcscpy_s(a, b, c)
#define   WCSNCPY_S(a, b, c, d)          wcsncpy_s(a, b, c, d)
#define   WCSCAT_S(a, b, c)              wcscat_s(a, b, c)
#define   WCSNCAT_S(a, b, c, d)          wcsncat_s(a, b, c, d)

#define   MEMSET_S(a, b, c, d)           memset_s(a, b, c, d)
#define   MEMCPY_S(a, b, c, d)           memcpy_s(a, b, c, d)
#define   STRCPY_S(a, b, c)              strcpy_s(a, b, c)
#define   STRNCPY_S(a, b, c, d)          strncpy_s(a, b, c, d)
#define   STRCAT_S(a, b, c)              strcat_s(a, b, c)
#define   STRNCAT_S(a, b, c, d)          strncat_s(a, b, c, d)

#define   STRTOK_S(a, b, c)              strtok_s(a, b, c)
#define   WCSTOK_S(a, b, c)              wcstok_s(a, b, c)
#define   GETS_S(a, b)                   gets_s(a, b)
#endif /*  */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* __NSTACK_SECUREC_H__ */
