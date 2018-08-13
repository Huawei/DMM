#ifndef STACKX_ARCH_SYS_ARCH_H
#define STACKX_ARCH_SYS_ARCH_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <semaphore.h>

#include "cc.h"

/*add by daifen 2012.7.27*/
  typedef uint64_t sys_thread_t;

  extern u64_t hz_per_ms;

  void stackx_global_lock (void);
  void stackx_global_unlock (void);

#define SYS_ARCH_PROTECT(lev) stackx_global_lock()
#define SYS_ARCH_UNPROTECT(lev) stackx_global_unlock()
#define SYS_ARCH_DECL_PROTECT(lev)

#ifdef __cplusplus

}
#endif
#endif
