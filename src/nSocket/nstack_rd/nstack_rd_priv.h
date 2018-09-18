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

#ifndef __NSTACK_RD_PRIV_H
#define __NSTACK_RD_PRIV_H
#include "list.h"

#define NSTACK_RD_SUCCESS    (0)
#define NSTACK_RD_FAIL       (-1)
#define NSTACK_RD_ITEM_MAX  (1024)

#define NSTACK_RD_AGETIME_MAX   (1)
#define NSTACK_SYS_FUN_MAX      (16)

#define RD_LWIP   "lwip"
#define RD_KERNEL "kernel"

#define NSTACK_RD_INDEX_BYIP(ip) (((ip) & 0xff) \
                                  + (((ip) >> 8)&0xff) \
                                  + (((ip) >> 16)&0xff) \
                                  + (((ip) >> 24)&0xff))

/*route data*/
typedef struct __rd_data_item
{
  /*route info type , for example base on ip */
  rd_data_type type;
  int stack_id;
  int agetime;
  union
  {
    rd_ip_data ipdata;
    unsigned int proto_type;
  };
} rd_data_item;

/*stack rd node*/
typedef struct __nstack_rd_node
{
  struct hlist_node rdnode;
  rd_data_item item;
} nstack_rd_node;

typedef struct __nstack_rd_list
{
  struct hlist_head headlist;
} nstack_rd_list;

typedef struct __nstack_rd_stack_info
{
  /*stack name */
  char name[STACK_NAME_MAX];
  /*stack id */
  int stack_id;
  /*when route info not found, high priority stack was chose, same priority chose fist input one */
  int priority;                 /*0: highest: route info not found choose first */
} nstack_rd_stack_info;

/*rd local data*/
typedef struct __rd_local_data
{
  nstack_rd_stack_info *pstack_info;
  int stack_num;
  nstack_rd_list route_list[RD_DATA_TYPE_MAX];  /*route table */
  nstack_get_route_data sys_fun[NSTACK_SYS_FUN_MAX];    /*rd data sys proc function list */
  int fun_num;
} rd_local_data;

typedef struct __rd_data_proc
{
  int (*rd_item_cpy) (void *destdata, void *srcdata);
  int (*rd_item_inset) (nstack_rd_list * hlist, void *rditem);
  int (*rd_item_age) (nstack_rd_list * hlist);
  int (*rd_item_find) (nstack_rd_list * hlist, void *rdkey, void *outitem);
  int (*rd_item_spec) (void *rdkey);
  int (*rd_item_default) (void *rdkey);
} rd_data_proc;

extern rd_local_data *g_rd_local_data;
extern rd_data_proc g_rd_cpy[RD_DATA_TYPE_MAX];

#define NSTACK_NUM  (g_rd_local_data->stack_num)
#define NSTACK_RD_LIST(type)  (&(g_rd_local_data->route_list[(type)]))
#define NSTACK_GET_STACK(idx)  ((g_rd_local_data->pstack_info)[idx].stack_id)

int nstack_get_stackid_byname (char *name);

#endif
