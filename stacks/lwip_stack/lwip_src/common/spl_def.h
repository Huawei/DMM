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

#ifndef __STACKX_DEF_H__
#define __STACKX_DEF_H__

/* arch.h might define NULL already */
//#include "lwip/arch.h"
#include "spl_opt.h"
#include "stackx_types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define STACKX_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define STACKX_MIN(x, y) (((x) < (y)) ? (x) : (y))

/* Endianess-optimized shifting of two u8_t to create one u16_t */
#if BYTE_ORDER == LITTLE_ENDIAN
#define STACKX_MAKE_U16(a, b) ((a << 8) | b)
#else
#define STACKX_MAKE_U16(a, b) ((b << 8) | a)
#endif

#ifndef STACKX_PLATFORM_BYTESWAP
#define STACKX_PLATFORM_BYTESWAP 0
#endif

#ifndef STACKX_PREFIX_BYTEORDER_FUNCS

/* workaround for naming collisions on some platforms */

#ifdef spl_htons
#undef spl_htons
#endif /* htons */
#ifdef spl_htonl
#undef spl_htonl
#endif /* spl_htonl */
#ifdef spl_ntohs
#undef spl_ntohs
#endif /* spl_ntohs */
#ifdef spl_ntohl
#undef spl_ntohl
#endif /* spl_ntohl */

#define spl_htons(x) stackx_htons(x)
#define spl_ntohs(x) stackx_ntohs(x)
#define spl_htonl(x) stackx_htonl(x)
#define spl_ntohl(x) stackx_ntohl(x)
#endif /* STACKX_PREFIX_BYTEORDER_FUNCS */

#if BYTE_ORDER == BIG_ENDIAN
#define stackx_htons(x) (x)
#define stackx_ntohs(x) (x)
#define stackx_htonl(x) (x)
#define stackx_ntohl(x) (x)
#define SPL_PP_HTONS(x) (x)
#define SPL_PP_NTOHS(x) (x)
#define SPL_PP_HTONL(x) (x)
#define SPL_PP_NTOHL(x) (x)
#else /* BYTE_ORDER != BIG_ENDIAN */
#if STACKX_PLATFORM_BYTESWAP
#define stackx_htons(x) STACKX_PLATFORM_HTONS(x)
#define stackx_ntohs(x) STACKX_PLATFORM_HTONS(x)
#define stackx_htonl(x) STACKX_PLATFORM_HTONL(x)
#define stackx_ntohl(x) STACKX_PLATFORM_HTONL(x)
#else /* STACKX_PLATFORM_BYTESWAP */
 /**
  * Convert an u16_t from host- to network byte order.
  *
  * @param n u16_t in host byte order
  * @return n in network byte order
  */
static inline u16_t
stackx_htons (u16_t x)
{
  return ((x & 0xff) << 8) | ((x & 0xff00) >> 8);
}

static inline u16_t
stackx_ntohs (u16_t x)
{
  return stackx_htons (x);
}

 /**
  * Convert an u32_t from host- to network byte order.
  *
  * @param n u32_t in host byte order
  * @return n in network byte order
  */
static inline u32_t
stackx_htonl (u32_t x)
{
  return ((x & 0xff) << 24) |
    ((x & 0xff00) << 8) |
    ((x & 0xff0000UL) >> 8) | ((x & 0xff000000UL) >> 24);
}

static inline u32_t
stackx_ntohl (u32_t x)
{
  return stackx_htonl (x);
}
#endif /* STACKX_PLATFORM_BYTESWAP */

/* These macros should be calculated by the preprocessor and are used
   with compile-time constants only (so that there is no little-endian
   overhead at runtime). */
#define SPL_PP_HTONS(x) ((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8))
#define SPL_PP_NTOHS(x) SPL_PP_HTONS(x)
#define SPL_PP_HTONL(x) ((((x) & 0xff) << 24) | \
					  (((x) & 0xff00) << 8) | \
					  (((x) & 0xff0000UL) >> 8) | \
					  (((x) & 0xff000000UL) >> 24))
#define SPL_PP_NTOHL(x) SPL_PP_HTONL(x)

#endif /* BYTE_ORDER == BIG_ENDIAN */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __STACKX_DEF_H__ */
