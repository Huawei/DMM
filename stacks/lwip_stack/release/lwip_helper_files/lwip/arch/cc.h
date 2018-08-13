#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LWIP_MAX_S16_NUM
#define LWIP_MAX_S16_NUM ((s16_t)0x7fff)
#endif

#ifndef LWIP_MAX_U16_NUM
#define LWIP_MAX_U16_NUM ((u16_t)0xFfff)
#endif

#ifndef LWIP_MAX_U32_NUM
#define LWIP_MAX_U32_NUM ((u32_t)0xffffffff)
#endif

#ifndef LWIP_MAX_S32_NUM
#define LWIP_MAX_S32_NUM ((s32_t)0x7fffffff)
#endif

#ifndef LWIP_MAX_U64_NUM
#define LWIP_MAX_U64_NUM ((u64_t)0xffffffffffffffff)
#endif

#ifndef LWIP_MAX_S64_NUM
#define LWIP_MAX_S64_NUM ((s64_t)0x7fffffffffffffff)
#endif

#define NONBLOCK_MODE_FOR_ALG 2 /* it is possible to get fail */
#define BLOCK_MODE_FOR_ALG 1    /* will block till success */
#define NONBLOCK_MODE_NOT_FOR_ALG 0

typedef uint64_t u64_t;
typedef int64_t s64_t;

typedef uint32_t u32_t;
typedef int32_t s32_t;

typedef uint16_t u16_t;
typedef int16_t s16_t;

typedef uint8_t u8_t;
typedef int8_t s8_t;

typedef uintptr_t mem_ptr_t;

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define S16_F "d"
#define U16_F "u"
#define X16_F "x"

#define S32_F "d"
#define U32_F "u"
#define X32_F "x"

#define S64_F "ld"
#define U64_F "lu"
#define X64_F "x"

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

//#define LWIP_PROVIDE_ERRNO

#ifndef LWIP_PLATFORM_DIAG
#define LWIP_PLATFORM_DIAG(x) do {printf x;} while(0)
#endif
#ifndef LWIP_DEBUG
#define LWIP_DEBUG  1
#endif
#endif
