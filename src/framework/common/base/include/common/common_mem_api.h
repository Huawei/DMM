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

#ifndef __COMMON_MEM_API_H__
#define __COMMON_MEM_API_H__

#ifdef HAL_LIB
#else

#include "rte_atomic.h"
#include "common_mem_spinlock.h"
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#ifndef NSTACK_LINT_CODE_DISABLE
#define NSTACK_LINT_CODE_DISABLE(code)  /*lint -e#code */
#endif

#ifndef NSTACK_LINT_CODE_ENABLE
#define NSTACK_LINT_CODE_ENABLE(code)   /*lint +e#code */
#endif

#define SYS_MBOX_NULL (sys_mbox_t)0

typedef sem_t *sys_sem_t_v1;
typedef sem_t sys_sem_st_v1;
typedef struct queue *sys_mbox_t;

typedef rte_spinlock_t *sys_sem_t_v2;
typedef rte_spinlock_t sys_sem_st_v2;

#ifndef u32_t
typedef uint32_t u32_t;
#endif

#ifndef u8_t
typedef uint8_t u8_t;
#endif

#ifndef s8_t
typedef int8_t s8_t;
#endif

#ifndef err_t
typedef s8_t err_t;
#endif

/** Return code for timeouts from sys_arch_mbox_fetch and sys_arch_sem_wait */
#define SYS_ARCH_TIMEOUT 0xffffffffUL

/** sys_mbox_tryfetch() returns SYS_MBOX_EMPTY if appropriate.
 * For now we use the same magic value, but we allow this to change in future.
 */
#define SYS_MBOX_EMPTY SYS_ARCH_TIMEOUT

void sys_sem_signal_s_v2 (sys_sem_t_v2 sem);
void sys_sem_init_v2 (sys_sem_t_v2 sem);

u32_t sys_arch_sem_wait_s_v2 (sys_sem_t_v2 sem);

#define SYS_HOST_INITIAL_PID 1
extern volatile pid_t g_sys_host_pid;
pid_t sys_get_hostpid_from_file (pid_t pid);
static inline pid_t
get_sys_pid ()
{
  if (SYS_HOST_INITIAL_PID == g_sys_host_pid)
    (void) sys_get_hostpid_from_file (getpid ());
  return g_sys_host_pid;
}

pid_t updata_sys_pid ();
long sys_now (void);

#define sys_sem_t sys_sem_t_v2
#define sys_sem_st sys_sem_st_v2
#define sys_sem_new(sem, count) sys_sem_new_v2(sem, count)
#define sys_sem_free(sem) sys_sem_free_v2(sem)
#define sys_sem_signal(sem) sys_sem_signal_v2(sem)
#define sys_arch_sem_wait(sem, timeout) sys_arch_sem_wait_v2(sem)
#define sys_arch_sem_trywait(sem) sys_arch_sem_trywait_v2(sem)

#define sys_sem_init(sem) sys_sem_init_v2(sem)
#define sys_sem_s_signal(sem) sys_sem_signal_s_v2(sem)
#define sys_arch_sem_s_wait(sem, timeout) sys_arch_sem_wait_s_v2(sem)
#define sys_arch_lock_with_pid(sem) (void)sys_arch_lock_with_pid_v2(sem)

#define BUF_SIZE_FILEPATH 256
#define STR_PID "pid:"
#define READ_FILE_BUFLEN  512

extern pid_t sys_get_hostpid_from_file (pid_t pid);
extern pid_t get_hostpid_from_file (u32_t pid);
extern void get_exec_name_by_pid (pid_t pid, char *task_name,
                                  int task_name_len);

static inline u32_t
sys_arch_lock_with_pid_v2 (sys_sem_t_v2 sem)
{
  if (SYS_HOST_INITIAL_PID == g_sys_host_pid)
    (void) sys_get_hostpid_from_file (getpid ());
  dmm_spinlock_lock_with_pid (sem, g_sys_host_pid);
  return 0;
}

#define NSTACK_SEM_MALLOC(sys_sem,count) \
{ \
    rte_spinlock_init(&(sys_sem)); \
    /*lint -e506*/\
    if (!(count)) \
    /*lint +e506*/\
    { \
        rte_spinlock_lock(&(sys_sem)); \
    } \
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#endif

#endif /* __COMMON_MEM_API_H__ */
