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

/*==============================================*
 *      include header files                    *
 *----------------------------------------------*/

/*==============================================*
 *      constants or macros define              *
 *----------------------------------------------*/

/*==============================================*
 *      project-wide global variables           *
 *----------------------------------------------*/

/*==============================================*
 *      routines' or functions' implementations *
 *----------------------------------------------*/

#ifndef __SELECT_ADAPT_H__
#define __SELECT_ADAPT_H__

#include "types.h"
#include "nstack_module.h"
#include "common_mem_spinlock.h"
#include "nstack_securec.h"
#include "common_func.h"

#define SBR_MAX_FD_NUM MAX_SOCKET_NUM

#define NSTACK_SELECT_MAX_FD    8192
#define NSTACK_SETSIZE  8192

#define FREE_FD_SET(readfd, writefd, exceptfd)  {\
    if(readfd)\
        free(readfd);\
    if(writefd)\
        free(writefd);\
    if(exceptfd)\
        free(exceptfd);\
}

typedef sem_t select_sem_t;
typedef common_mem_spinlock_t select_spinlock_t;

#define select_spin_lock(lock)         (common_mem_spinlock_lock((lock)))
#define select_spin_unlock(lock)     (common_mem_spinlock_unlock((lock)))
#define select_spin_lock_init(lock)   (common_mem_spinlock_init((lock)))

#define  select_sem_wait(sem)                       (sem_wait((sem)))
#define  select_sem_init(sem,  share,  val)      (sem_init((sem), (share), (val)))
#define select_sem_post(sem)                        (sem_post((sem)))

/*************input form other modules***************************/
extern nstack_module_info g_nstack_modules;
#define get_mode_num()                        (g_nstack_modules.modNum)
#define get_mode_linux_index()              (g_nstack_modules.fix_mid)

struct select_comm_fd_map
{
  i32 mod_fd[NSTACK_MAX_MODULE_NUM];    //save modules fd
  i32 index;                    //-1 mean not select modules
};

struct select_mod_fd_map
{
  i32 *comm_fd;                 //the fd app used
};

struct select_fd_map_inf
{

  struct select_comm_fd_map *fdinf;     //NSTACK_MAX_SOCK_NUM
  struct select_mod_fd_map modinf[NSTACK_MAX_MODULE_NUM];

};
void *select_alloc (int size);
void select_free (char *p);
void reset_select_fdinf (i32 fd);
i32 select_get_modfd (i32 fd, i32 inx);
i32 select_set_modfd (i32 fd, i32 inx, i32 modfd);
i32 select_get_modindex (i32 fd);
i32 select_get_commfd (i32 modfd, i32 inx);
i32 select_set_commfd (i32 modfd, i32 inx, i32 fd);
i32 fdmapping_init (void);
i32 select_set_index (i32 fd, i32 inx);
i32 select_set_profd (i32 fd, i32 profd);
void nssct_close (i32 cfd, i32 inx);
void nssct_create (i32 cfd, i32 mfd, i32 inx);
void nssct_set_index (i32 fd, i32 inx);

#endif
