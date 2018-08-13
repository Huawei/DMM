#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define RING_CACHE_SIZE 1024
#define LISTEN_CACHE_SIZE 1024
#define RECV_MAX_POOL 4
#define MAX_TRY_GET_MEMORY_TIMES 4
#define MAX_MEMORY_USED_SIZE 80

#define IP_HLEN             20
#define TCP_HLEN            20
#define TCP_MAX_OPTION_LEN  40

#define _cache_aligned __attribute__((__aligned__(64)))
#define LWIP_NETIF_API 1

#define TCP_OVERSIZE 1
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS
#define SYS_LIGHTWEIGHT_PROT              1
#define LWIP_DISABLE_TCP_SANITY_CHECKS    1
#define LWIP_COMPAT_MUTEX_ALLOWED         1
#define LWIP_ERRNO_INCLUDE  <errno.h>
#define LWIP_SKIP_PACKING_CHECK           1
#define PBUF_POOL_FREE_OOSEQ              0

#define LWIP_DEBUG               1
#define LWIP_TIMERS              0
#define LWIP_TIMERS_CUSTOM       1
#define LWIP_TCPIP_CORE_LOCKING         1
#define MEM_LIBC_MALLOC                 1
#define MEMP_MEM_MALLOC                 1
#define LWIP_CALLBACK_API               1
#define LWIP_SOCKET                     0
#define LWIP_POSIX_SOCKETS_IO_NAMES     0
#define LWIP_TCP_KEEPALIVE              1
#define LWIP_TIMEVAL_PRIVATE            0
#define LWIP_COMPAT_MUTEX               1

#endif
