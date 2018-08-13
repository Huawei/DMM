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

#ifndef SHAREDMEMORY_H_
#define SHAREDMEMORY_H_
#include "stackxopts.h"
#include <sys/types.h>
#include <semaphore.h>
//#include "stackx/raw.h"
#include "tcp.h"
#include "udp.h"
//#include "stackx/ip.h"
#include "spl_err.h"
#include "list.h"
#include "arch/queue.h"
#include "spl_opt.h"
#include "stackx/spl_ip_addr.h"

#include "stackx/spl_api.h"
#include <arch/sys_arch.h>
#include "common_mem_api.h"
//#include "stackx/memp.h"
#include "stackx_instance.h"

#include "hal_api.h"
#ifdef HAL_LIB
#else
#include "rte_ring.h"
#endif

/** Description for a task waiting in select */
struct stackx_select_cb
{
        /** Pointer to the next waiting task */
  union
  {
    struct stackx_select_cb *next;
    PTR_ALIGN_TYPE next_a;
  };

        /** Pointer to the previous waiting task */
  union
  {
    struct stackx_select_cb *prev;
    PTR_ALIGN_TYPE prev_a;
  };

        /** semaphore to wake up a task waiting for select */
  //sys_sem_t sem;
  union
  {
    sys_sem_t_v1 sem;
    PTR_ALIGN_TYPE sem_a;
  };

        /** readset passed to select */
  fd_set readset;

        /** writeset passed to select */
  fd_set writeset;

        /** unimplemented: exceptset passed to select */
  fd_set exceptset;

        /** don't signal the same semaphore twice: set to 1 when signalled */
  volatile int sem_signalled;

  uint8_t pad_64bit[4];
};
/** From epoll.h: Definition of struct stackx_sock and stackx_select_cb ---------End*/

enum tcp_run_type
{
  TCP_MUTIPL_INSTANCE = 0,
  TCP_MASTER_WORKER,
  TCP_DISTRIBUTOR_WORKER,
  TCP_RUN_TO_COMPETE,
  TCP_PROC_TYPE_END
};

enum proc_run_type
{
  PROC_MAIN_RUN_TYPE = 0,
  PROC_BACKUP_RUN_TYPE,
  PROC_RUN_TYPE_END
};

struct linux_port_info
{
  char if_name[HAL_MAX_NIC_NAME_LEN];
  char ip_addr_linux[18];       //uint32_t ip_addr_linux;
  char mask_linux[18];          //uint32_t mask_linux;
  char bcast_linux[18];         //uint32_t bcast_linux;
  char mac_addr[20];            //struct ether_addr mac_addr;
  hal_hdl_t hdl;
};

struct stackx_port_info
{
  struct stackx_port_info *next_use_port;
  struct linux_port_info linux_ip;
};

struct stackx_port_zone
{
  unsigned int port_num;
  unsigned int bonded_port_num;
  struct stackx_port_info *stackx_one_port;
};

struct select_cb_entry
{
  struct stackx_select_cb select_cb;

  union
  {
    sem_t semForSelect;
    ALIGN_TYPE pad_64bit[4];
  };

  union
  {
    struct select_cb_entry *pre_empty_entry;
    PTR_ALIGN_TYPE pre_empty_entry_a;
  };

  union
  {
    struct select_cb_entry *next_empty_entry;
    PTR_ALIGN_TYPE next_empty_entry_a;
  };

};

#endif /* SHAREDMEMORY_H_ */
